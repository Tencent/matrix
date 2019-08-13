// Archive/Common/ItemNameUtils.cpp

#include "StdAfx.h"

#include "ItemNameUtils.h"

namespace NArchive {
namespace NItemName {

static const wchar_t kOSDirDelimiter = WCHAR_PATH_SEPARATOR;
static const wchar_t kDirDelimiter = L'/';

void ReplaceToOsPathSeparator(wchar_t *s)
{
  #ifdef _WIN32
  for (;;)
  {
    wchar_t c = *s;
    if (c == 0)
      break;
    if (c == kDirDelimiter)
      *s = kOSDirDelimiter;
    s++;
  }
  #endif
}

UString MakeLegalName(const UString &name)
{
  UString zipName = name;
  zipName.Replace(kOSDirDelimiter, kDirDelimiter);
  return zipName;
}

UString GetOSName(const UString &name)
{
  UString newName = name;
  newName.Replace(kDirDelimiter, kOSDirDelimiter);
  return newName;
}

UString GetOSName2(const UString &name)
{
  if (name.IsEmpty())
    return UString();
  UString newName = GetOSName(name);
  if (newName.Back() == kOSDirDelimiter)
    newName.DeleteBack();
  return newName;
}

void ConvertToOSName2(UString &name)
{
  if (!name.IsEmpty())
  {
    name.Replace(kDirDelimiter, kOSDirDelimiter);
    if (name.Back() == kOSDirDelimiter)
      name.DeleteBack();
  }
}

bool HasTailSlash(const AString &name, UINT
  #if defined(_WIN32) && !defined(UNDER_CE)
    codePage
  #endif
  )
{
  if (name.IsEmpty())
    return false;
  LPCSTR prev =
  #if defined(_WIN32) && !defined(UNDER_CE)
    CharPrevExA((WORD)codePage, name, &name[name.Len()], 0);
  #else
    (LPCSTR)(name) + (name.Len() - 1);
  #endif
  return (*prev == '/');
}

#ifndef _WIN32
UString WinNameToOSName(const UString &name)
{
  UString newName = name;
  newName.Replace(L'\\', kOSDirDelimiter);
  return newName;
}
#endif

}}
