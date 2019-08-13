// Archive/Common/ItemNameUtils.h

#ifndef __ARCHIVE_ITEM_NAME_UTILS_H
#define __ARCHIVE_ITEM_NAME_UTILS_H

#include "../../../Common/MyString.h"

namespace NArchive {
namespace NItemName {

  void ReplaceToOsPathSeparator(wchar_t *s);

  UString MakeLegalName(const UString &name);
  UString GetOSName(const UString &name);
  UString GetOSName2(const UString &name);
  void ConvertToOSName2(UString &name);
  bool HasTailSlash(const AString &name, UINT codePage);

  #ifdef _WIN32
  inline UString WinNameToOSName(const UString &name)  { return name; }
  #else
  UString WinNameToOSName(const UString &name);
  #endif

}}

#endif
