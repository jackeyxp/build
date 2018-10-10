
#pragma once

#include "global.h"

class CTCPThread;
class CTCPClient
{
public:
  CTCPClient(CTCPThread * lpTCPThread, int connfd, int nHostPort, string & strSinAddr);
  ~CTCPClient();
public:
  int       ForRead();            // 读取网络数据
  int       ForWrite();           // 发送网络数据
  bool      IsTimeout();          // 检测是否超时
  void      ResetTimeout();       // 重置超时时间
  void      doUDPTeacherPusherOnLine(bool bIsOnLineFlag);
  void      doUDPStudentPusherOnLine(int inDBCameraID, bool bIsOnLineFlag);
  void      doLogoutForUDP(int nDBCameraID, uint8_t tmTag, uint8_t idTag);
public:
  int       GetConnFD() { return m_nConnFD; }
  int       GetRoomID() { return m_nRoomID; }
  int       GetClientType() { return m_nClientType; }
  string &  GetMacAddr() { return m_strMacAddr; }
private:
  int       parseJsonData(const char * lpJsonPtr, int nJsonLength);          // 统一的JSON解析接口...
  int       doPHPClient(Cmd_Header * lpHeader, const char * lpJsonPtr);      // 处理PHP客户端事件...
  int       doStudentClient(Cmd_Header * lpHeader, const char * lpJsonPtr);  // 处理Student事件...
  int       doTeacherClient(Cmd_Header * lpHeader, const char * lpJsonPtr);  // 处理Teacher事件...
private:
  int       doCmdStudentCameraLiveStop();
  int       doCmdStudentCameraPullStop();
  int       doCmdStudentCameraPullStart();
  int       doCmdStudentLogin();
  int       doCmdStudentOnLine();
  int       doCmdTeacherLogin();
  int       doCmdTeacherOnLine();
  int       doCmdTeacherCameraLiveStop();
  int       doCmdTeacherCameraLiveStart();
  int       doCmdTeacherCameraOnLineList();
  int       doTrasferCameraLiveCmdByTeacher(int nCmdID, int nDBCameraID);
  int       doSendCmdLoginForStudent(bool bIsTCPOnLine, bool bIsUDPOnLine);
  int       doSendCmdLoginForTeacher(int nSceneItemID, int inDBCameraID, bool bIsCameraOnLine);
  int       doSendCommonCmd(int nCmdID, const char * lpJsonPtr = NULL, int nJsonSize = 0);
private:
  int          m_epoll_fd;         // epoll句柄编号...
  int          m_nConnFD;          // 连接socket...
  int          m_nRoomID;          // 记录房间编号...
  int          m_nHostPort;        // 连接端口...
  int          m_nClientType;      // 客户端类型...
  int          m_nSceneItemID;     // 讲师端当前请求的场景资源编号...
  time_t       m_nStartTime;       // 超时检测起点...
  string       m_strSinAddr;       // 连接映射地址...
  string       m_strMacAddr;       // 记录登录MAC地址...
  string       m_strIPAddr;        // 记录登录IP地址...
  string       m_strRoomID;        // 记录房间编号...
  string       m_strPCName;        // 记录终端名称...
  string       m_strSend;          // 数据发送缓存...
  string       m_strRecv;          // 数据读取缓存...
  GM_MapJson   m_MapJson;          // 终端传递过来的JSON数据...
  CTCPRoom   * m_lpTCPRoom;        // TCP房间对象...
  CTCPThread * m_lpTCPThread;      // TCP线程对象...
  
  friend class CTCPThread;
};