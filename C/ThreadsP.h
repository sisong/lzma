// ThreadsP.h
//   for not _WIN32 ,used pthread
//   2019-02-05 : by housisong : for HDiffPatch

#ifndef __7Z_THRESDS_P_H
#define __7Z_THRESDS_P_H
#ifndef _WIN32
#include "7zTypes.h"
#include <string.h> //memcpy

typedef void*   HANDLE;
typedef void*   LPVOID;
#define LONG    Int32
#define HRESULT UInt32

typedef void*   CRITICAL_SECTION;
void _def_DeleteCriticalSection(CRITICAL_SECTION* p);
void _def_EnterCriticalSection (CRITICAL_SECTION* p);
void _def_LeaveCriticalSection (CRITICAL_SECTION* p);
#define  DeleteCriticalSection  _def_DeleteCriticalSection
#define  EnterCriticalSection   _def_EnterCriticalSection
#define  LeaveCriticalSection   _def_LeaveCriticalSection

//#ifdef MTCODER__USE_WRITE_THREAD
LONG _def_InterlockedIncrement(LONG* lpAddend);
#define  InterlockedIncrement   _def_InterlockedIncrement
//#endif

#endif
#endif

