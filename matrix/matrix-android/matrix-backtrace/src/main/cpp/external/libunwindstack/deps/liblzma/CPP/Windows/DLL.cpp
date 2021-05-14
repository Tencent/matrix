// Windows/DLL.cpp

#include "StdAfx.h"

#include "DLL.h"

#ifndef _UNICODE
extern bool g_IsNT;
#endif

extern HINSTANCE g_hInstance;

namespace NWindows {
namespace NDLL {

bool CLibrary::Free() throw()
{
  if (_module == 0)
    return true;
  if (!::FreeLibrary(_module))
    return false;
  _module = 0;
  return true;
}

bool CLibrary::LoadEx(CFSTR path, DWORD flags) throw()
{
  if (!Free())
    return false;
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    _module = ::LoadLibraryEx(fs2fas(path), NULL, flags);
  }
  else
  #endif
  {
    _module = ::LoadLibraryExW(fs2us(path), NULL, flags);
  }
  return (_module != NULL);
}

bool CLibrary::Load(CFSTR path) throw()
{
  if (!Free())
    return false;
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    _module = ::LoadLibrary(fs2fas(path));
  }
  else
  #endif
  {
    _module = ::LoadLibraryW(fs2us(path));
  }
  return (_module != NULL);
}

bool MyGetModuleFileName(FString &path)
{
  HMODULE hModule = g_hInstance;
  path.Empty();
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    TCHAR s[MAX_PATH + 2];
    s[0] = 0;
    DWORD size = ::GetModuleFileName(hModule, s, MAX_PATH + 1);
    if (size <= MAX_PATH && size != 0)
    {
      path = fas2fs(s);
      return true;
    }
  }
  else
  #endif
  {
    WCHAR s[MAX_PATH + 2];
    s[0] = 0;
    DWORD size = ::GetModuleFileNameW(hModule, s, MAX_PATH + 1);
    if (size <= MAX_PATH && size != 0)
    {
      path = us2fs(s);
      return true;
    }
  }
  return false;
}

#ifndef _SFX

FString GetModuleDirPrefix()
{
  FString s;
  if (MyGetModuleFileName(s))
  {
    int pos = s.ReverseFind_PathSepar();
    if (pos >= 0)
      s.DeleteFrom(pos + 1);
  }
  if (s.IsEmpty())
    s = "." STRING_PATH_SEPARATOR;
  return s;
}

#endif

}}
