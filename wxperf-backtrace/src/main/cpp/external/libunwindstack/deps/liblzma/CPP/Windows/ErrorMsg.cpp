// Windows/ErrorMsg.h

#include "StdAfx.h"

#ifndef _UNICODE
#include "../Common/StringConvert.h"
#endif

#include "ErrorMsg.h"

#ifndef _UNICODE
extern bool g_IsNT;
#endif

namespace NWindows {
namespace NError {

static bool MyFormatMessage(DWORD errorCode, UString &message)
{
  LPVOID msgBuf;
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    if (::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorCode, 0, (LPTSTR) &msgBuf, 0, NULL) == 0)
      return false;
    message = GetUnicodeString((LPCTSTR)msgBuf);
  }
  else
  #endif
  {
    if (::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorCode, 0, (LPWSTR) &msgBuf, 0, NULL) == 0)
      return false;
    message = (LPCWSTR)msgBuf;
  }
  ::LocalFree(msgBuf);
  return true;
}

UString MyFormatMessage(DWORD errorCode)
{
  UString m;
  if (!MyFormatMessage(errorCode, m) || m.IsEmpty())
  {
    char s[16];
    for (int i = 0; i < 8; i++)
    {
      unsigned t = errorCode & 0xF;
      errorCode >>= 4;
      s[7 - i] = (char)((t < 10) ? ('0' + t) : ('A' + (t - 10)));
    }
    s[8] = 0;
    m += "Error #";
    m += s;
  }
  else if (m.Len() >= 2
      && m[m.Len() - 1] == 0x0A
      && m[m.Len() - 2] == 0x0D)
    m.DeleteFrom(m.Len() - 2);
  return m;
}

}}
