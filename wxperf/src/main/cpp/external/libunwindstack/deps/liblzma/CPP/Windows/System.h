// Windows/System.h

#ifndef __WINDOWS_SYSTEM_H
#define __WINDOWS_SYSTEM_H

#include "../Common/MyTypes.h"

namespace NWindows {
namespace NSystem {

UInt32 CountAffinity(DWORD_PTR mask);

struct CProcessAffinity
{
  // UInt32 numProcessThreads;
  // UInt32 numSysThreads;
  DWORD_PTR processAffinityMask;
  DWORD_PTR systemAffinityMask;

  void InitST()
  {
    // numProcessThreads = 1;
    // numSysThreads = 1;
    processAffinityMask = 1;
    systemAffinityMask = 1;
  }

  UInt32 GetNumProcessThreads() const { return CountAffinity(processAffinityMask); }
  UInt32 GetNumSystemThreads() const { return CountAffinity(systemAffinityMask); }

  BOOL Get();
};

UInt32 GetNumberOfProcessors();

bool GetRamSize(UInt64 &size); // returns false, if unknown ram size

}}

#endif
