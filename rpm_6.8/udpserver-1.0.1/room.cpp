
#include "room.h"
#include "student.h"
#include "teacher.h"

CRoom::CRoom(int inRoomID, int inLiveID)
  : m_lpStudentPusher(NULL)
  , m_lpTeacherPusher(NULL)
  , m_lpTeacherLooker(NULL)
  , m_nRoomID(inRoomID)
  , m_nLiveID(inLiveID)
{
  
}

CRoom::~CRoom()
{
  
}

void CRoom::doDumpRoomInfo()
{
  log_trace("\n======== RoomID: %d, LiveID: %d ========\n Student-Pusher: %d, Teacher-Looker: %d\n Teacher-Pusher: %d, Student-Looker: %d", m_nRoomID, m_nLiveID,
            ((m_lpStudentPusher != NULL) ? 1 : 0), ((m_lpTeacherLooker != NULL) ? 1 : 0), ((m_lpTeacherPusher != NULL) ? 1 : 0), m_MapStudentLooker.size());
}
//
// 根据传递过来的学生端类型，进行变量更新...
void CRoom::doCreateStudent(CStudent * lpStudent)
{
  int nHostPort = lpStudent->GetHostPort();
  uint8_t idTag = lpStudent->GetIdTag();
  if( idTag == ID_TAG_PUSHER ) {
    m_lpStudentPusher = lpStudent;
  } else if( idTag == ID_TAG_LOOKER ) {
    m_MapStudentLooker[nHostPort] = lpStudent;
  }
}
//
// 处理学生端删除事件...
void CRoom::doDeleteStudent(CStudent * lpStudent)
{
  // 如果输入的学生端对象无效，直接返回...
  if( lpStudent == NULL )
    return;
  // 进行指针比较，如果是学生推流端，置空，返回...
  if( lpStudent == m_lpStudentPusher ) {
    m_lpStudentPusher = NULL;
    return;
  }
  // 在学生观看端中遍历查找...
  int nHostPort = lpStudent->GetHostPort();
  GM_MapStudent::iterator itorItem = m_MapStudentLooker.find(nHostPort);
  // 找到相关观看学生端对象，直接删除返回...
  if( itorItem != m_MapStudentLooker.end() ) {
    m_MapStudentLooker.erase(itorItem);
    return;
  }
  // 如果通过端口方式没有找到，通过指针遍历查找...
  itorItem = m_MapStudentLooker.begin();
  while(itorItem != m_MapStudentLooker.end()) {
    // 找到了相关节点 => 删除节点，返回...
    if(itorItem->second == lpStudent) {
      m_MapStudentLooker.erase(itorItem);
      return;
    }
  }
  // 通过指针遍历也没有找到，打印错误信息...
  log_trace("Can't find Student-Looker, HostPort: %d", nHostPort);
}
//
// 根据传递过来的老师端类型，进行变量更新...
void CRoom::doCreateTeacher(CTeacher * lpTeacher)
{
  uint8_t idTag = lpTeacher->GetIdTag();
  if( idTag == ID_TAG_PUSHER ) {
    m_lpTeacherPusher = lpTeacher;
  } else if( idTag == ID_TAG_LOOKER ) {
    m_lpTeacherLooker = lpTeacher;
  }
}
//
// 处理老师端删除事件...
void CRoom::doDeleteTeacher(CTeacher * lpTeacher)
{
  // 如果是老师推流端，置空，返回...
  if( m_lpTeacherPusher == lpTeacher ) {
    m_lpTeacherPusher = NULL;
    return;
  }
  // 如果是老师观看端，置空，返回...
  if( m_lpTeacherLooker == lpTeacher ) {
    m_lpTeacherLooker = NULL;
    return;
  }
}

bool CRoom::doTransferToStudentLooker(char * lpBuffer, int inBufSize)
{
  if( lpBuffer == NULL || inBufSize <= 0 )
    return false;
  if( m_MapStudentLooker.size() <= 0 )
    return false;
  GM_MapStudent::iterator itorItem;
  for(itorItem = m_MapStudentLooker.begin(); itorItem != m_MapStudentLooker.end(); ++itorItem) {
    CStudent * lpStudent = itorItem->second;
    if( lpStudent == NULL ) continue;
    lpStudent->doTransferToStudentLooker(lpBuffer, inBufSize);
  }
  return true;
}