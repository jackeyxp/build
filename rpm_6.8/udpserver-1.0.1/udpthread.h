
#pragma once

#include "global.h"
#include "thread.h"
#include "circlebuf.h"

class CUDPThread : public CThread
{
public:
  CUDPThread();
  virtual ~CUDPThread();
  virtual void Entry();
public:
  bool        InitThread();
  CRoom   *   doCreateRoom(int inRoomID);
  void        doAddLoseForStudent(CStudent * lpStudent);
  void        doDelLoseForStudent(CStudent * lpStudent);
  void        doAddSupplyForTeacher(CTeacher * lpTeacher);
  void        doDelSupplyForTeacher(CTeacher * lpTeacher);
  void        doDeleteForCameraLiveStop(int inRoomID);
  bool        IsUDPTeacherPusherOnLine(int inRoomID);
  bool        IsUDPStudentPusherOnLine(int inRoomID, int inDBCameraID);
  bool        onRecvEvent(uint32_t inHostAddr, uint16_t inHostPort, char * lpBuffer, int inBufSize);
private:
  bool        doProcSocket(uint32_t nHostSinAddr, uint16_t nHostSinPort, char * lpBuffer, int inBufSize);
  void        doTagDelete(int nHostPort);
  void        doCheckTimeout();
  void        doSendDetectCmd();
  void        doSendSupplyCmd();
  void        doSendLoseCmd();
  void        doRecvPacket();
  void        clearAllRoom();
  void        clearAllClient();
private:
  GM_MapRoom        m_MapRoom;          // 房间列表...
  GM_MapNetwork     m_MapNetwork;       // 网络对象列表...
  GM_ListTeacher    m_ListTeacher;      // 有补包的老师推流者列表...
  GM_ListStudent    m_ListStudent;      // 有丢包的学生观看者列表...
  int64_t           m_next_check_ns;    // 下次超时检测的时间戳 => 纳秒 => 每隔10秒检测一次...
  int64_t           m_next_detect_ns;	  // 下次发送探测包的时间戳 => 纳秒 => 每隔1秒发送一次...
  circlebuf         m_circle;           // 网络环形队列...
  os_sem_t     *    m_sem_t;            // 线程信号量...
  pthread_mutex_t   m_buff_mutex;       // 缓存互斥对象...
  pthread_mutex_t   m_room_mutex;       // 房间互斥对象...
};