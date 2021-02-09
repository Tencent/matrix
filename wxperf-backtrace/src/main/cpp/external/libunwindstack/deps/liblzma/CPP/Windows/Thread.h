// Windows/Thread.h

#ifndef __WINDOWS_THREAD_H
#define __WINDOWS_THREAD_H

#include "../../C/Threads.h"

#include "Defs.h"

namespace NWindows {

class CThread
{
  ::CThread thread;
public:
  CThread() { Thread_Construct(&thread); }
  ~CThread() { Close(); }
  bool IsCreated() { return Thread_WasCreated(&thread) != 0; }
  WRes Close()  { return Thread_Close(&thread); }
  WRes Create(THREAD_FUNC_RET_TYPE (THREAD_FUNC_CALL_TYPE *startAddress)(void *), LPVOID parameter)
    { return Thread_Create(&thread, startAddress, parameter); }
  WRes Wait() { return Thread_Wait(&thread); }
  
  #ifdef _WIN32
  operator HANDLE() { return thread; }
  void Attach(HANDLE handle) { thread = handle; }
  HANDLE Detach() { HANDLE h = thread; thread = NULL; return h; }
  DWORD Resume() { return ::ResumeThread(thread); }
  DWORD Suspend() { return ::SuspendThread(thread); }
  bool Terminate(DWORD exitCode) { return BOOLToBool(::TerminateThread(thread, exitCode)); }
  int GetPriority() { return ::GetThreadPriority(thread); }
  bool SetPriority(int priority) { return BOOLToBool(::SetThreadPriority(thread, priority)); }
  #endif
};

}

#endif
