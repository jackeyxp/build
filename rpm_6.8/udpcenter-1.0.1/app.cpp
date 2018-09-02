
#include "app.h"
#include "tcpthread.h"

CApp::CApp()
  : m_sem_t(NULL)
  , m_lpTCPThread(NULL)
{
  // 初始化辅助线程信号量...
  os_sem_init(&m_sem_t, 0);
}

CApp::~CApp()
{
  // 删除TCP线程对象...
  if (m_lpTCPThread != NULL) {
    delete m_lpTCPThread;
    m_lpTCPThread = NULL;
  }
  // 释放服务器资源...
  GM_MapServer::iterator itorServer;
  for(itorServer = m_MapServer.begin(); itorServer != m_MapServer.end(); ++itorServer) {
    delete itorServer->second;
  }
  // 释放房间资源...
  GM_MapRoom::iterator itorRoom;
  for(itorRoom = m_MapRoom.begin(); itorRoom != m_MapRoom.end(); ++itorRoom) {
    delete itorRoom->second;
  }
  // 释放辅助线程信号量...
  os_sem_destroy(m_sem_t);
}

bool CApp::doStartThread()
{
  // 创建TCP监听对象，并启动线程...
  assert(m_lpTCPThread == NULL);
  m_lpTCPThread = new CTCPThread();
  if (!m_lpTCPThread->InitThread()) {
    log_trace("Init TCPThread failed!");
    return false;
  }
  return true;
}

bool CApp::doInitRLimit()
{
	// set max open file number for one process...
	struct rlimit rt = {0};
	rt.rlim_max = rt.rlim_cur = MAX_OPEN_FILE;
	if( setrlimit(RLIMIT_NOFILE, &rt) == -1 ) {
    log_trace("setrlimit error(%s)", strerror(errno));
		return false;
	}
  return true;
}

void CApp::doWaitForExit()
{
  // 设定默认的信号超时时间 => APP_SLEEP_MS 毫秒...
  unsigned long next_wait_ms = APP_SLEEP_MS;
  while( true ) {
    os_sem_timedwait(m_sem_t, next_wait_ms);
  }
}

// 创建服务器对象 => 通过套接字编号进行创建...
CUdpServer * CApp::doCreateUdpServer(int inSocketFD)
{
  // 如果找到了服务器对象，直接返回...
  CUdpServer * lpUdpServer = NULL;
  GM_MapServer::iterator itorServer = m_MapServer.find(inSocketFD);
  if( itorServer != m_MapServer.end() ) {
    lpUdpServer = itorServer->second;
    return lpUdpServer;
  }
  // 如果没有找到服务器，创建一个新的服务器对象...
  lpUdpServer = new CUdpServer();
  m_MapServer[inSocketFD] = lpUdpServer;
  return lpUdpServer;
}

// 删除服务器对象 => 通过套接字编号进行查找...
void CApp::doDeleteUdpServer(int inSocketFD)
{
  GM_MapServer::iterator itorServer = m_MapServer.find(inSocketFD);
  if( itorServer != m_MapServer.end() ) {
    delete itorServer->second;
    m_MapServer.erase(itorServer);
  }
}

// 通过套接字编号查找服务器对象...
CUdpServer * CApp::doFindUdpServer(int inSocketFD)
{
  CUdpServer * lpUdpServer = NULL;
  GM_MapServer::iterator itorServer = m_MapServer.find(inSocketFD);
  if( itorServer != m_MapServer.end() ) {
    lpUdpServer = itorServer->second;
  }
  return lpUdpServer;
}

// 查找直播服务器列表中挂载房间最少的服务器...
CUdpServer * CApp::doFindMinUdpServer()
{
  // 如果没有直播汇报源，直接返回NULL...
  if( m_MapServer.size() <= 0 )
    return NULL;
  assert( m_MapServer.size() > 0 );
  // 先拿出一个节点的数据作为最小值...
  GM_MapServer::iterator itorItem = m_MapServer.begin();
  CUdpServer * lpMinServer = itorItem->second;
  int nMinCount = lpMinServer->GetRoomCount();
  // 如果第一个服务器的挂载量为0，直接返回...
  if( nMinCount <= 0 )
    return lpMinServer;
  // 遍历所有节点，找到最小的节点...
  for(itorItem = m_MapServer.begin(); itorItem != m_MapServer.end(); ++itorItem) {
    CUdpServer * lpCurServer = itorItem->second;
    int nCurCount = lpCurServer->GetRoomCount();
    // 如果当前服务器挂载量为0，直接返回...
    if( nCurCount <= 0) {
      lpMinServer = lpCurServer;
      break;
    }
    // 计算最小挂载量的直播服务器...
    if( nCurCount < nMinCount ) {
      nMinCount = nCurCount;
      lpMinServer = lpCurServer;
    }
  }
  // 直接返回最小挂在量的直播服务器对象...
  assert( lpMinServer != NULL );
  return lpMinServer;
}

// 创建房间对象 => 通过房间编号进行创建...
CTCPRoom * CApp::doCreateRoom(int nRoomID, CUdpServer * lpUdpServer)
{
  // 如果找到了房间对象，直接返回...
  CTCPRoom * lpRoom = NULL;
  GM_MapRoom::iterator itorRoom = m_MapRoom.find(nRoomID);
  if( itorRoom != m_MapRoom.end() ) {
    lpRoom = itorRoom->second;
    return lpRoom;
  }
  // 如果没有找到房间，创建一个新的房间对象...
  lpRoom = new CTCPRoom(nRoomID, lpUdpServer);
  m_MapRoom[nRoomID] = lpRoom;
  return lpRoom;
}

// 删除与服务器相关的房间对象...
void CApp::doDeleteRoom(CUdpServer * lpUdpServer)
{
  GM_MapRoom::iterator itorRoom = m_MapRoom.begin();
  while( itorRoom != m_MapRoom.end() ) {
    CTCPRoom * lpRoom = itorRoom->second;
    if( lpRoom->GetUdpServer() == lpUdpServer ) {
      delete lpRoom; lpRoom = NULL;
      m_MapRoom.erase(itorRoom++);
    } else {
      ++itorRoom;
    }
  }
}

// 通过指定的房间号删除房间对象...
void CApp::doDeleteRoom(int nRoomID)
{
  GM_MapRoom::iterator itorRoom = m_MapRoom.find(nRoomID);
  if( itorRoom != m_MapRoom.end() ) {
    delete itorRoom->second;
    m_MapRoom.erase(itorRoom);
  }
}

// 通过房间编号查找房间对象...
CTCPRoom * CApp::doFindTCPRoom(int nRoomID)
{
  CTCPRoom * lpTCPRoom = NULL;
  GM_MapRoom::iterator itorRoom = m_MapRoom.find(nRoomID);
  if( itorRoom != m_MapRoom.end() ) {
    lpTCPRoom = itorRoom->second;
  }
  return lpTCPRoom;
}

CUdpServer::CUdpServer()
{
  
}

CUdpServer::~CUdpServer()
{
  // 删除与服务器相关的房间对象...
  GetApp()->doDeleteRoom(this);
}

// 挂载房间到当前直播服务器上...
void CUdpServer::doMountRoom(CTCPRoom * lpRoom)
{
  int nRoomID = lpRoom->GetRoomID();
  m_MapRoom[nRoomID] = lpRoom;
}

// 从服务器上卸载房间...
void CUdpServer::doUnMountRoom(int nRoomID)
{
  GM_MapRoom::iterator itorRoom = m_MapRoom.find(nRoomID);
  if( itorRoom != m_MapRoom.end() ) {
    m_MapRoom.erase(itorRoom);
  }
}

// 指定房间里的老师引用计数增加...
void CUdpServer::doAddTeacher(int nRoomID)
{
  // 创建或更新房间对象，创建成功，更新信息...
  CTCPRoom * lpRoom = GetApp()->doCreateRoom(nRoomID, this);
  // 累加房间里的老师计数器...
  if( lpRoom != NULL ) {
    ++lpRoom->m_nTeacherCount;
    int nTeacherCount = lpRoom->GetTeacherCount();
    int nStudentCount = lpRoom->GetStudentCount();
    log_debug("[Add-Teacher] RoomID: %d, Teacher: %d, Student: %d", nRoomID, nTeacherCount, nStudentCount);
  }
}

// 指定房间里的老师引用计数减少...
void CUdpServer::doDelTeacher(int nRoomID)
{
  // 创建或更新房间对象，创建成功，更新信息...
  CTCPRoom * lpRoom = GetApp()->doCreateRoom(nRoomID, this);
  // 减少房间里的老师计数器...
  if( lpRoom != NULL ) {
    --lpRoom->m_nTeacherCount;
    int nTeacherCount = lpRoom->GetTeacherCount();
    int nStudentCount = lpRoom->GetStudentCount();
    log_debug("[Del-Teacher] RoomID: %d, Teacher: %d, Student: %d", nRoomID, nTeacherCount, nStudentCount);
    // 如果房间里的讲师和学生都为0，则发起房间删除操作...
    if((nTeacherCount <= 0) && (nStudentCount <= 0)) {
      GetApp()->doDeleteRoom(nRoomID);
    }
  }
}

// 指定房间里的学生引用计数增加...
void CUdpServer::doAddStudent(int nRoomID)
{
  // 创建或更新房间对象，创建成功，更新信息...
  CTCPRoom * lpRoom = GetApp()->doCreateRoom(nRoomID, this);
  // 累加房间里的学生计数器...
  if( lpRoom != NULL ) {
    ++lpRoom->m_nStudentCount;
    int nTeacherCount = lpRoom->GetTeacherCount();
    int nStudentCount = lpRoom->GetStudentCount();
    log_debug("[Add-Student] RoomID: %d, Teacher: %d, Student: %d", nRoomID, nTeacherCount, nStudentCount);
  }
}

// 指定房间里的学生引用计数减少...
void CUdpServer::doDelStudent(int nRoomID)
{
  // 创建或更新房间对象，创建成功，更新信息...
  CTCPRoom * lpRoom = GetApp()->doCreateRoom(nRoomID, this);
  // 减少房间里的学生计数器...
  if( lpRoom != NULL ) {
    --lpRoom->m_nStudentCount;
    int nTeacherCount = lpRoom->GetTeacherCount();
    int nStudentCount = lpRoom->GetStudentCount();
    log_debug("[Del-Student] RoomID: %d, Teacher: %d, Student: %d", nRoomID, nTeacherCount, nStudentCount);
    // 如果房间里的讲师和学生都为0，则发起房间删除操作...
    if((nTeacherCount <= 0) && (nStudentCount <= 0)) {
      GetApp()->doDeleteRoom(nRoomID);
    }
  }
}

CTCPRoom::CTCPRoom(int nRoomID, CUdpServer * lpUdpServer)
  : m_lpUdpServer(lpUdpServer)
  , m_nRoomID(nRoomID)
  , m_nTeacherCount(0)
  , m_nStudentCount(0)
{
  assert(m_nRoomID > 0);
  assert(m_lpUdpServer != NULL);
  // 挂载放到直播服务器上...
  m_lpUdpServer->doMountRoom(this);
}

CTCPRoom::~CTCPRoom()
{
  // 从服务器上卸载房间...
  if( m_lpUdpServer != NULL ) {
    m_lpUdpServer->doUnMountRoom(m_nRoomID);
  }
}