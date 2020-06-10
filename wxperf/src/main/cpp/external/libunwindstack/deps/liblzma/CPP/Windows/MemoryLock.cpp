// Windows/MemoryLock.cpp

#include "StdAfx.h"

#include "MemoryLock.h"

namespace NWindows {
namespace NSecurity {

#ifndef UNDER_CE

#ifdef _UNICODE
#define MY_FUNC_SELECT(f) :: f
#else
#define MY_FUNC_SELECT(f) my_ ## f
extern "C" {
typedef BOOL (WINAPI * Func_OpenProcessToken)(HANDLE ProcessHandle, DWORD DesiredAccess, PHANDLE TokenHandle);
typedef BOOL (WINAPI * Func_LookupPrivilegeValue)(LPCTSTR lpSystemName, LPCTSTR lpName, PLUID lpLuid);
typedef BOOL (WINAPI * Func_AdjustTokenPrivileges)(HANDLE TokenHandle, BOOL DisableAllPrivileges,
    PTOKEN_PRIVILEGES NewState, DWORD BufferLength, PTOKEN_PRIVILEGES PreviousState, PDWORD ReturnLength);
}
#define GET_PROC_ADDR(fff, name) Func_ ## fff  my_ ## fff  = (Func_ ## fff)GetProcAddress(hModule, name)
#endif

bool EnablePrivilege(LPCTSTR privilegeName, bool enable)
{
  bool res = false;

  #ifndef _UNICODE

  HMODULE hModule = ::LoadLibrary(TEXT("Advapi32.dll"));
  if (hModule == NULL)
    return false;
  
  GET_PROC_ADDR(OpenProcessToken, "OpenProcessToken");
  GET_PROC_ADDR(LookupPrivilegeValue, "LookupPrivilegeValueA");
  GET_PROC_ADDR(AdjustTokenPrivileges, "AdjustTokenPrivileges");
  
  if (my_OpenProcessToken &&
      my_AdjustTokenPrivileges &&
      my_LookupPrivilegeValue)
  
  #endif

  {
    HANDLE token;
    if (MY_FUNC_SELECT(OpenProcessToken)(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token))
    {
      TOKEN_PRIVILEGES tp;
      if (MY_FUNC_SELECT(LookupPrivilegeValue)(NULL, privilegeName, &(tp.Privileges[0].Luid)))
      {
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Attributes = (enable ? SE_PRIVILEGE_ENABLED : 0);
        if (MY_FUNC_SELECT(AdjustTokenPrivileges)(token, FALSE, &tp, 0, NULL, NULL))
          res = (GetLastError() == ERROR_SUCCESS);
      }
      ::CloseHandle(token);
    }
  }
    
  #ifndef _UNICODE

  ::FreeLibrary(hModule);
  
  #endif

  return res;
}



typedef void (WINAPI * Func_RtlGetVersion) (OSVERSIONINFOEXW *);

/*
  We suppose that Window 10 works incorrectly with "Large Pages" at:
    - Windows 10 1703 (15063)
    - Windows 10 1709 (16299)
*/

unsigned Get_LargePages_RiskLevel()
{
  OSVERSIONINFOEXW vi;
  HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
  if (!ntdll)
    return 0;
  Func_RtlGetVersion func = (Func_RtlGetVersion)GetProcAddress(ntdll, "RtlGetVersion");
  if (!func)
    return 0;
  func(&vi);
  return (vi.dwPlatformId == VER_PLATFORM_WIN32_NT
      && vi.dwMajorVersion + vi.dwMinorVersion == 10
      && vi.dwBuildNumber <= 16299) ? 1 : 0;
}

#endif

}}
