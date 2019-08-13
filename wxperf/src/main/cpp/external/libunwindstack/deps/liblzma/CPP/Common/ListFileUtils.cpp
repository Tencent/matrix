// Common/ListFileUtils.cpp

#include "StdAfx.h"

#include "../../C/CpuArch.h"

#include "../Windows/FileIO.h"

#include "ListFileUtils.h"
#include "MyBuffer.h"
#include "StringConvert.h"
#include "UTFConvert.h"

static const char kQuoteChar = '\"';

static void AddName(UStringVector &strings, UString &s)
{
  s.Trim();
  if (s.Len() >= 2 && s[0] == kQuoteChar && s.Back() == kQuoteChar)
  {
    s.DeleteBack();
    s.Delete(0);
  }
  if (!s.IsEmpty())
    strings.Add(s);
}

bool ReadNamesFromListFile(CFSTR fileName, UStringVector &strings, UINT codePage)
{
  NWindows::NFile::NIO::CInFile file;
  if (!file.Open(fileName))
    return false;
  UInt64 fileSize;
  if (!file.GetLength(fileSize))
    return false;
  if (fileSize >= ((UInt32)1 << 31) - 32)
    return false;
  UString u;
  if (codePage == MY__CP_UTF16 || codePage == MY__CP_UTF16BE)
  {
    if ((fileSize & 1) != 0)
      return false;
    CByteArr buf((size_t)fileSize);
    UInt32 processed;
    if (!file.Read(buf, (UInt32)fileSize, processed))
      return false;
    if (processed != fileSize)
      return false;
    file.Close();
    unsigned num = (unsigned)fileSize / 2;
    wchar_t *p = u.GetBuf(num);
    if (codePage == MY__CP_UTF16)
      for (unsigned i = 0; i < num; i++)
      {
        wchar_t c = GetUi16(buf + i * 2);
        if (c == 0)
          return false;
        p[i] = c;
      }
    else
      for (unsigned i = 0; i < num; i++)
      {
        wchar_t c = (wchar_t)GetBe16(buf + i * 2);
        if (c == 0)
          return false;
        p[i] = c;
      }
    p[num] = 0;
    u.ReleaseBuf_SetLen(num);
  }
  else
  {
    AString s;
    char *p = s.GetBuf((unsigned)fileSize);
    UInt32 processed;
    if (!file.Read(p, (UInt32)fileSize, processed))
      return false;
    if (processed != fileSize)
      return false;
    file.Close();
    s.ReleaseBuf_CalcLen((unsigned)processed);
    if (s.Len() != processed)
      return false;
    
    // #ifdef CP_UTF8
    if (codePage == CP_UTF8)
    {
      if (!ConvertUTF8ToUnicode(s, u))
        return false;
    }
    else
    // #endif
      MultiByteToUnicodeString2(u, s, codePage);
  }

  const wchar_t kGoodBOM = 0xFEFF;
  const wchar_t kBadBOM  = 0xFFFE;
  
  UString s;
  unsigned i = 0;
  for (; i < u.Len() && u[i] == kGoodBOM; i++);
  for (; i < u.Len(); i++)
  {
    wchar_t c = u[i];
    if (c == kGoodBOM || c == kBadBOM)
      return false;
    if (c == L'\n' || c == 0xD)
    {
      AddName(strings, s);
      s.Empty();
    }
    else
      s += c;
  }
  AddName(strings, s);
  return true;
}
