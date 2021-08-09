// Common/StringConvert.cpp

#include "StdAfx.h"

#include "StringConvert.h"

#ifndef _WIN32
#include <stdlib.h>
#endif

static const char k_DefultChar = '_';

#ifdef _WIN32

/*
MultiByteToWideChar(CodePage, DWORD dwFlags,
    LPCSTR lpMultiByteStr, int cbMultiByte,
    LPWSTR lpWideCharStr, int cchWideChar)

  if (cbMultiByte == 0)
    return: 0. ERR: ERROR_INVALID_PARAMETER

  if (cchWideChar == 0)
    return: the required buffer size in characters.

  if (supplied buffer size was not large enough)
    return: 0. ERR: ERROR_INSUFFICIENT_BUFFER
    The number of filled characters in lpWideCharStr can be smaller than cchWideChar (if last character is complex)

  If there are illegal characters:
    if MB_ERR_INVALID_CHARS is set in dwFlags:
      - the function stops conversion on illegal character.
      - Return: 0. ERR: ERROR_NO_UNICODE_TRANSLATION.
    
    if MB_ERR_INVALID_CHARS is NOT set in dwFlags:
      before Vista: illegal character is dropped (skipped). WinXP-64: GetLastError() returns 0.
      in Vista+:    illegal character is not dropped (MSDN). Undocumented: illegal
                    character is converted to U+FFFD, which is REPLACEMENT CHARACTER.
*/


void MultiByteToUnicodeString2(UString &dest, const AString &src, UINT codePage)
{
  dest.Empty();
  if (src.IsEmpty())
    return;
  {
    /*
    wchar_t *d = dest.GetBuf(src.Len());
    const char *s = (const char *)src;
    unsigned i;
    
    for (i = 0;;)
    {
      Byte c = (Byte)s[i];
      if (c >= 0x80 || c == 0)
        break;
      d[i++] = (wchar_t)c;
    }

    if (i != src.Len())
    {
      unsigned len = MultiByteToWideChar(codePage, 0, s + i,
          src.Len() - i, d + i,
          src.Len() + 1 - i);
      if (len == 0)
        throw 282228;
      i += len;
    }

    d[i] = 0;
    dest.ReleaseBuf_SetLen(i);
    */
    unsigned len = MultiByteToWideChar(codePage, 0, src, src.Len(), NULL, 0);
    if (len == 0)
    {
      if (GetLastError() != 0)
        throw 282228;
    }
    else
    {
      len = MultiByteToWideChar(codePage, 0, src, src.Len(), dest.GetBuf(len), len);
      if (len == 0)
        throw 282228;
      dest.ReleaseBuf_SetEnd(len);
    }
  }
}

/*
  int WideCharToMultiByte(
      UINT CodePage, DWORD dwFlags,
      LPCWSTR lpWideCharStr, int cchWideChar,
      LPSTR lpMultiByteStr, int cbMultiByte,
      LPCSTR lpDefaultChar, LPBOOL lpUsedDefaultChar);

if (lpDefaultChar == NULL),
  - it uses system default value.

if (CodePage == CP_UTF7 || CodePage == CP_UTF8)
  if (lpDefaultChar != NULL || lpUsedDefaultChar != NULL)
    return: 0. ERR: ERROR_INVALID_PARAMETER.

The function operates most efficiently, if (lpDefaultChar == NULL && lpUsedDefaultChar == NULL)

*/

static void UnicodeStringToMultiByte2(AString &dest, const UString &src, UINT codePage, char defaultChar, bool &defaultCharWasUsed)
{
  dest.Empty();
  defaultCharWasUsed = false;
  if (src.IsEmpty())
    return;
  {
    /*
    unsigned numRequiredBytes = src.Len() * 2;
    char *d = dest.GetBuf(numRequiredBytes);
    const wchar_t *s = (const wchar_t *)src;
    unsigned i;
    
    for (i = 0;;)
    {
      wchar_t c = s[i];
      if (c >= 0x80 || c == 0)
        break;
      d[i++] = (char)c;
    }
    
    if (i != src.Len())
    {
      BOOL defUsed = FALSE;
      defaultChar = defaultChar;

      bool isUtf = (codePage == CP_UTF8 || codePage == CP_UTF7);
      unsigned len = WideCharToMultiByte(codePage, 0, s + i, src.Len() - i,
          d + i, numRequiredBytes + 1 - i,
          (isUtf ? NULL : &defaultChar),
          (isUtf ? NULL : &defUsed));
      defaultCharWasUsed = (defUsed != FALSE);
      if (len == 0)
        throw 282229;
      i += len;
    }

    d[i] = 0;
    dest.ReleaseBuf_SetLen(i);
    */

    /*
    if (codePage != CP_UTF7)
    {
      const wchar_t *s = (const wchar_t *)src;
      unsigned i;
      for (i = 0;; i++)
      {
        wchar_t c = s[i];
        if (c >= 0x80 || c == 0)
          break;
      }
      
      if (s[i] == 0)
      {
        char *d = dest.GetBuf(src.Len());
        for (i = 0;;)
        {
          wchar_t c = s[i];
          if (c == 0)
            break;
          d[i++] = (char)c;
        }
        d[i] = 0;
        dest.ReleaseBuf_SetLen(i);
        return;
      }
    }
    */

    unsigned len = WideCharToMultiByte(codePage, 0, src, src.Len(), NULL, 0, NULL, NULL);
    if (len == 0)
    {
      if (GetLastError() != 0)
        throw 282228;
    }
    else
    {
      BOOL defUsed = FALSE;
      bool isUtf = (codePage == CP_UTF8 || codePage == CP_UTF7);
      // defaultChar = defaultChar;
      len = WideCharToMultiByte(codePage, 0, src, src.Len(),
          dest.GetBuf(len), len,
          (isUtf ? NULL : &defaultChar),
          (isUtf ? NULL : &defUsed)
          );
      if (!isUtf)
        defaultCharWasUsed = (defUsed != FALSE);
      if (len == 0)
        throw 282228;
      dest.ReleaseBuf_SetEnd(len);
    }
  }
}

/*
#ifndef UNDER_CE
AString SystemStringToOemString(const CSysString &src)
{
  AString dest;
  const unsigned len = src.Len() * 2;
  CharToOem(src, dest.GetBuf(len));
  dest.ReleaseBuf_CalcLen(len);
  return dest;
}
#endif
*/

#else

void MultiByteToUnicodeString2(UString &dest, const AString &src, UINT /* codePage */)
{
  dest.Empty();
  if (src.IsEmpty())
    return;

  size_t limit = ((size_t)src.Len() + 1) * 2;
  wchar_t *d = dest.GetBuf((unsigned)limit);
  size_t len = mbstowcs(d, src, limit);
  if (len != (size_t)-1)
  {
    dest.ReleaseBuf_SetEnd((unsigned)len);
    return;
  }
  
  {
    unsigned i;
    const char *s = (const char *)src;
    for (i = 0;;)
    {
      Byte c = (Byte)s[i];
      if (c == 0)
        break;
      d[i++] = (wchar_t)c;
    }
    d[i] = 0;
    dest.ReleaseBuf_SetLen(i);
  }
}

static void UnicodeStringToMultiByte2(AString &dest, const UString &src, UINT /* codePage */, char defaultChar, bool &defaultCharWasUsed)
{
  dest.Empty();
  defaultCharWasUsed = false;
  if (src.IsEmpty())
    return;

  size_t limit = ((size_t)src.Len() + 1) * 6;
  char *d = dest.GetBuf((unsigned)limit);
  size_t len = wcstombs(d, src, limit);
  if (len != (size_t)-1)
  {
    dest.ReleaseBuf_SetEnd((unsigned)len);
    return;
  }

  {
    const wchar_t *s = (const wchar_t *)src;
    unsigned i;
    for (i = 0;;)
    {
      wchar_t c = s[i];
      if (c == 0)
        break;
      if (c >= 0x100)
      {
        c = defaultChar;
        defaultCharWasUsed = true;
      }
      d[i++] = (char)c;
    }
    d[i] = 0;
    dest.ReleaseBuf_SetLen(i);
  }
}

#endif


UString MultiByteToUnicodeString(const AString &src, UINT codePage)
{
  UString dest;
  MultiByteToUnicodeString2(dest, src, codePage);
  return dest;
}

UString MultiByteToUnicodeString(const char *src, UINT codePage)
{
  return MultiByteToUnicodeString(AString(src), codePage);
}


void UnicodeStringToMultiByte2(AString &dest, const UString &src, UINT codePage)
{
  bool defaultCharWasUsed;
  UnicodeStringToMultiByte2(dest, src, codePage, k_DefultChar, defaultCharWasUsed);
}

AString UnicodeStringToMultiByte(const UString &src, UINT codePage, char defaultChar, bool &defaultCharWasUsed)
{
  AString dest;
  UnicodeStringToMultiByte2(dest, src, codePage, defaultChar, defaultCharWasUsed);
  return dest;
}

AString UnicodeStringToMultiByte(const UString &src, UINT codePage)
{
  AString dest;
  bool defaultCharWasUsed;
  UnicodeStringToMultiByte2(dest, src, codePage, k_DefultChar, defaultCharWasUsed);
  return dest;
}
