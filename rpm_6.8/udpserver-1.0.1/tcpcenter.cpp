
#include "app.h"
#include <netdb.h>
#include <json/json.h>
#include "tcpcenter.h"
#include "tcpthread.h"
#include "tcproom.h"

#define ONLINE_TIME_OUT          30   // 向中心服务器汇报频率30秒...
#define MAX_LINE_SIZE      4 * 1024   // 读取最大数据长度...

CTCPCenter::CTCPCenter(CTCPThread * lpTCPThread)
  : m_nClientType(kClientUdpServer)
  , m_lpTCPThread(lpTCPThread)
  , m_center_fd(0)
  , m_epoll_fd(0)
{
  m_nStartTime = time(NULL);  
}

CTCPCenter::~CTCPCenter()
{
  // 关闭套接字对象...
  if( m_center_fd > 0 ) {
    close(m_center_fd);
    m_center_fd = 0;
  }  
}

// 执行epoll的网络事件...
int CTCPCenter::doEpollEvent(int nEvent)
{
  int nRetValue = -1;
  if( nEvent & EPOLLIN ) {
    nRetValue = this->doHandleRead();
  } else if( nEvent & EPOLLOUT ) {
    nRetValue = this->doHandleWrite();
  }
  // 判断事件处理结果...
  if( nRetValue < 0 ) {
    // 处理失败，从epoll队列中删除...
    struct epoll_event evDelete = {0};
    evDelete.data.fd = m_center_fd;
    evDelete.events = EPOLLIN | EPOLLET;
    epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, m_center_fd, &evDelete);
    // 关闭中心服务器的连接...
    close(m_center_fd);
    m_center_fd = NULL;
  }
  // 返回最终执行结果...
  return nRetValue;
}

// 处理UDP中心套接字的写事件...
int CTCPCenter::doHandleWrite()
{
  if( m_center_fd <= 0 )
    return -1;
  // 如果没有需要发送的数据，直接返回...
  if( m_strSend.size() <= 0 )
    return 0;
  // 发送全部的数据包内容...
  assert( m_strSend.size() > 0 );
  int nWriteLen = write(m_center_fd, m_strSend.c_str(), m_strSend.size());
  if( nWriteLen <= 0 ) {
    log_trace("transmit command error(%s)", strerror(errno));
    return -1;
  }
  // 每次发送成功，必须清空发送缓存...
  m_strSend.clear();
  // 注意：epoll写事件，只要投递就会触发，读事件则会等待底层激发...
  // 准备修改事件需要的数据，先发起读取事件，写事件会有投递者调用接口修改...
  struct epoll_event evClient = {0};
  evClient.data.fd = m_center_fd;
  evClient.events = EPOLLIN | EPOLLET;
  // 重新修改事件，加入读取事件...
  if( epoll_ctl(m_epoll_fd, EPOLL_CTL_MOD, m_center_fd, &evClient) < 0 ) {
    log_trace("mod socket '%d' to epoll failed: %s", m_center_fd, strerror(errno));
    return -1;
  }
  // 操作成功，返回0...
  return 0;  
}

// 处理UDP中心套接字的读事件...
int CTCPCenter::doHandleRead()
{
  if( m_center_fd <= 0 )
    return -1;
  // 直接读取网络数据...
  char bufRead[MAX_LINE_SIZE] = {0};
  int  nReadLen = read(m_center_fd, bufRead, MAX_LINE_SIZE);
  // 读取失败，返回错误，让上层关闭...
  if( nReadLen == 0 ) {
    log_trace("Client: UdpCenter, ForRead: Close, Socket: %d", m_center_fd);
    return -1;
  }
  // 读取失败，返回错误，让上层关闭...
  if( nReadLen < 0 ) {
    log_trace("Client: UdpCenter, read error(%s)", strerror(errno));
    return -1;
  }
  // 返回最终读取到的字节数...
  return nReadLen;
}

// 初始化中心套接字对象...
int CTCPCenter::InitTCPCenter(int nEpollFD)
{
  m_epoll_fd = nEpollFD;
  int nHostPort = GetApp()->GetCenterPort();
  const char * lpszAddr = GetApp()->GetCenterAddr();
  return this->doCreateTCPSocket(lpszAddr, nHostPort);
}

// 查看超时状态...
int CTCPCenter::doHandleTimeout()
{
  // 如果中心套接字无效，需要重新创建...
  if( m_center_fd <= 0 ) {
    int nHostPort = GetApp()->GetCenterPort();
    const char * lpszAddr = GetApp()->GetCenterAddr();
    return this->doCreateTCPSocket(lpszAddr, nHostPort);
  }
  // 如果发生汇报超时，向中心汇报，并重置超时...
  if( this->IsTimeout() ) {
    this->doCmdUdpServerOnLine();
    this->ResetTimeout();
  }
  // 返回正常...
  return 0;
}

int CTCPCenter::SetNonBlocking(int sockfd)
{
	if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0)|O_NONBLOCK) == -1) {
		return -1;
	}
	return 0;
}

// 创建连接UDP中心服务器的套接字，并加入到epoll队列当中...
int CTCPCenter::doCreateTCPSocket(const char * lpInAddr, int nHostPort)
{
  // 如果epoll对象创建失败，直接返回...
  if( m_epoll_fd <= 0 ) {
    log_trace("m_epoll_fd is null");
    return -1;
  }
  // 创建TCP中心套接字...
	int center_fd = socket(AF_INET, SOCK_STREAM, 0); 
  if( center_fd < 0 ) {
    log_trace("can't create tcp socket");
    return -1;
  }
  // 设置异步套接字 => 失败，关闭套接字...
  if( this->SetNonBlocking(center_fd) < 0 ) {
    log_trace("SetNonBlocking error: %s", strerror(errno));
    close(center_fd);
    return -1;
  }
  // 设定发送和接收缓冲最大值...
  int nRecvMaxLen = 32 * 1024;
  int nSendMaxLen = 32 * 1024;
  // 设置接收缓冲区...
  if( setsockopt(center_fd, SOL_SOCKET, SO_RCVBUF, &nRecvMaxLen, sizeof(nRecvMaxLen)) != 0 ) {
    log_trace("SO_RCVBUF error: %s", strerror(errno));
    close(center_fd);
    return -1;
  }
  // 设置发送缓冲区...
  if( setsockopt(center_fd, SOL_SOCKET, SO_SNDBUF, &nSendMaxLen, sizeof(nSendMaxLen)) != 0 ) {
    log_trace("SO_SNDBUF error: %s", strerror(errno));
    close(center_fd);
    return -1;
  }
  // 先通过域名解析出IP地址...
	const char * lpszAddr = lpInAddr;
	struct hostent * lpHost = gethostbyname(lpszAddr);
	if( lpHost != NULL && lpHost->h_addr_list != NULL ) {
		lpszAddr = inet_ntoa(*(in_addr*)lpHost->h_addr_list[0]);
	}
  // 准备绑定地址结构体...
	struct sockaddr_in tcpAddr = {0};
	bzero(&tcpAddr, sizeof(tcpAddr));
	tcpAddr.sin_family = AF_INET; 
	tcpAddr.sin_port = htons(nHostPort);
	tcpAddr.sin_addr.s_addr = inet_addr(lpszAddr);
  // 连接UDP中心服务器 => 注意正在连接的情况...
  int nResult = connect(center_fd, (struct sockaddr *)&tcpAddr, sizeof(tcpAddr));
  if( nResult < 0 && errno != EINPROGRESS ) {
    log_trace("connect error: %s", strerror(errno));
    close(center_fd);
    return -1;
  }
  // 注意：epoll写事件，只要投递就会触发，读事件则会等待底层激发...
  // 连接成功之后，需要立即汇报信息，因此，只发起写事件...
  struct epoll_event evConnect = {0};
	evConnect.data.fd = center_fd;
	evConnect.events = EPOLLOUT | EPOLLET; 
  // EPOLLEF模式下，accept时必须用循环来接收链接，防止链接丢失...
	if( epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, center_fd, &evConnect) < 0 ) {
    log_trace("epoll set insertion error: fd=%d", center_fd);
		return -1;
	}
  // 重建成功，重置超时时间...
  this->ResetTimeout();
  // 返回已经绑定完毕的TCP套接字...
  m_center_fd = center_fd;
  // 准备登录汇报命令需要的数据内容信息...
  int nUdpPort = GetApp()->GetUdpPort();
  int nRemotePort = GetApp()->GetTcpPort();
  bool bIsDebugMode = GetApp()->IsDebugMode();
  string & strWanAddr = GetApp()->GetWanAddr();
  json_object * new_obj = json_object_new_object();
  // 注意：阿里云专有网络无法获取外网地址，中心服务器可以同链接获取外网地址，这里的外网地址为空地址...
  json_object_object_add(new_obj, "remote_addr", json_object_new_string(strWanAddr.c_str()));
  json_object_object_add(new_obj, "remote_port", json_object_new_int(nRemotePort));
  json_object_object_add(new_obj, "udp_addr", json_object_new_string(strWanAddr.c_str()));
  json_object_object_add(new_obj, "udp_port", json_object_new_int(nUdpPort));
  json_object_object_add(new_obj, "debug_mode", json_object_new_int(bIsDebugMode));
  // 转换成json字符串，获取字符串长度...
  char * lpNewJson = (char*)json_object_to_json_string(new_obj);
  // 使用统一的通用命令发送接口函数...
  nResult = this->doSendCommonCmd(kCmd_UdpServer_Login, lpNewJson, strlen(lpNewJson));
  // json对象引用计数减少...
  json_object_put(new_obj);
  // 返回执行结果...
  return nResult;
}

// 统一的通用命令发送接口函数...
int CTCPCenter::doSendCommonCmd(int nCmdID, const char * lpJsonPtr/* = NULL*/, int nJsonSize/* = 0*/)
{
  // 构造回复结构头信息...
  Cmd_Header theHeader = {0};
  theHeader.m_pkg_len = ((lpJsonPtr != NULL) ? nJsonSize : 0);
  theHeader.m_type = kClientUdpServer;
  theHeader.m_cmd  = nCmdID;
  // 先填充名头头结构数据内容 => 注意是assign重建字符串...
  m_strSend.assign((char*)&theHeader, sizeof(theHeader));
  // 如果传入的数据内容有效，才进行数据的填充...
  if( lpJsonPtr != NULL && nJsonSize > 0 ) {
    m_strSend.append(lpJsonPtr, nJsonSize);
  }
  // 向当前终端对象发起发送数据事件...
  epoll_event evClient = {0};
  evClient.data.fd = m_center_fd;
  evClient.events = EPOLLOUT | EPOLLET;
  // 重新修改事件，加入写入事件...
  if( epoll_ctl(m_epoll_fd, EPOLL_CTL_MOD, m_center_fd, &evClient) < 0 ) {
    log_trace("mod socket '%d' to epoll failed: %s", m_center_fd, strerror(errno));
    return -1;
  }
  // 返回执行正确...
  return 0;
}

// 采用集中汇报房间列表，而不是单独发送...
int CTCPCenter::doCmdUdpServerOnLine()
{
  string strRoomList;
  GM_MapTCPRoom & theMapRoom = m_lpTCPThread->GetMapTCPRoom();
  GM_MapTCPRoom::iterator itorRoom = theMapRoom.begin();
  while( itorRoom != theMapRoom.end() ) {
    char szValue[255] = {0};
    int  nRoomID = itorRoom->first;
    CTCPRoom * lpTCPRoom = itorRoom->second;
    int nTeacherCount = lpTCPRoom->GetTeacherCount();
    int nStudentCount = lpTCPRoom->GetStudentCount();
    // 构造每个房间的信息 => 房间号-老师数量-学生数量...
    sprintf(szValue, "%d-%d-%d", nRoomID, nTeacherCount, nStudentCount);
    // 追加特殊换行符号 => |
    if (++itorRoom != theMapRoom.end()) {
      strcat(szValue, "|");
    }
    // 将数据更新到字符串缓存当中...
    strRoomList.append(szValue, strlen(szValue));
  }
  // 仅供测试使用...
  /*int g_tCount = 0;
  if (g_tCount == 0 ) {
    strRoomList.append("200001-1-1|200002-0-2|200003-1-0|200005-1-4");
    ++g_tCount;
  } else if( g_tCount == 1) {
    strRoomList.append("200002-0-2|200005-1-4");
    ++g_tCount;
  } else if( g_tCount == 2) {
    strRoomList.append("200001-1-1|200002-0-2|200003-1-0");
    ++g_tCount;
  }*/
  // 封装成json数据包...
  int    nJsonSize = 0;
  char * lpNewJson = NULL;
  json_object * new_obj = NULL;
  if (strRoomList.size() > 0) {
    // 将数据转换成json数据包...
    new_obj = json_object_new_object();
    json_object_object_add(new_obj, "room_list", json_object_new_string(strRoomList.c_str()));
    // 转换成json字符串，获取字符串长度...
    lpNewJson = (char*)json_object_to_json_string(new_obj);
    nJsonSize = strlen(lpNewJson);
  }
  // 使用统一的通用命令发送接口函数...
  int nResult = this->doSendCommonCmd(kCmd_UdpServer_OnLine, lpNewJson, nJsonSize);
  // json对象引用计数减少...
  if (new_obj != NULL ) {
    json_object_put(new_obj);
    new_obj = NULL;
  }
  // 返回执行结果...
  return nResult;
}

// 针对TCP房间里讲师端、学生端计数器变化的命令通知...
int CTCPCenter::doRoomCommand(int nCmdID, int nRoomID)
{
  // 将房间编号放入json对象当中...
  json_object * new_obj = json_object_new_object();
  json_object_object_add(new_obj, "room_id", json_object_new_int(nRoomID));
  // 转换成json字符串，获取字符串长度...
  char * lpNewJson = (char*)json_object_to_json_string(new_obj);
  // 使用统一的通用命令发送接口函数...
  int nResult = this->doSendCommonCmd(nCmdID, lpNewJson, strlen(lpNewJson));
  // json对象引用计数减少...
  json_object_put(new_obj);
  // 返回执行结果...
  return nResult;
}

// 检测是否超时...
bool CTCPCenter::IsTimeout()
{
  time_t nDeltaTime = time(NULL) - m_nStartTime;
  return ((nDeltaTime >= ONLINE_TIME_OUT) ? true : false);
}

// 重置超时时间...
void CTCPCenter::ResetTimeout()
{
  m_nStartTime = time(NULL);
}
