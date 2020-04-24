// Common/UTFConvert.h

#ifndef __COMMON_UTF_CONVERT_H
#define __COMMON_UTF_CONVERT_H

#include "MyString.h"

bool CheckUTF8(const char *src, bool allowReduced = false) throw();
bool ConvertUTF8ToUnicode(const AString &utfString, UString &resultString);
void ConvertUnicodeToUTF8(const UString &unicodeString, AString &resultString);

#endif
