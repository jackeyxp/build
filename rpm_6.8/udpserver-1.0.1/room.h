
#pragma once

#include "global.h"

class CTeacher;
class CStudent;
class CRoom
{
public:
  CRoom(int inRoomID);
  ~CRoom();
public:
  void         ResetRoomFlow() { m_nUpFlowByte = 0; m_nDownFlowByte = 0; }
  int          GetUpFlowMB() { return m_nUpFlowByte/1000/1000; }
  int          GetDownFlowMB() { return m_nDownFlowByte/1000/1000; }
  CTeacher  *  GetTeacherLooker() { return m_lpTeacherLooker; }
  CTeacher  *  GetTeacherPusher() { return m_lpTeacherPusher; }
  CStudent  *  GetStudentPusher() { return m_lpStudentPusher; }
public:
  void         doAddDownFlowByte(int nDownSize) { m_nDownFlowByte += nDownSize; }
  void         doAddUpFlowByte(int nUpSize) { m_nUpFlowByte += nUpSize; }
  void         doDumpRoomInfo();
  void         doCreateStudent(CStudent * lpStudent);
  void         doDeleteStudent(CStudent * lpStudent);
  void         doCreateTeacher(CTeacher * lpTeacher);
  void         doDeleteTeacher(CTeacher * lpTeacher);
  bool         doStudentPusherToStudentLooker(char * lpBuffer, int inBufSize);
  bool         doTeacherPusherToStudentLooker(char * lpBuffer, int inBufSize);
private:
  uint16_t     AddExAudioChangeNum() { return ++m_wExAudioChangeNum; }
  uint16_t     GetExAudioChangeNum() { return m_wExAudioChangeNum; }
private:
  int             m_nRoomID;            // 房间标识号码...
  uint64_t        m_nUpFlowByte;        // 房间上行流量...
  uint64_t        m_nDownFlowByte;      // 房间下行流量...
  uint16_t        m_wExAudioChangeNum;  // 扩展音频变化次数 => 自动溢出回还...
  CStudent   *    m_lpStudentPusher;    // 只有一个学生端推流，发给一个老师端...
  CTeacher   *    m_lpTeacherLooker;    // 接收学生端推流...
  CTeacher   *    m_lpTeacherPusher;    // 只有一个老师端推流，发给多个学生端...
  GM_MapStudent   m_MapStudentLooker;   // 学生端观看者列表，都接收老师端推流...
};