// Common/StringConvert.h

#ifndef __COMMON_STRING_CONVERT_H
#define __COMMON_STRING_CONVERT_H

#include "MyString.h"
#include "MyWindows.h"

UString MultiByteToUnicodeString(const AString &srcString, UINT codePage = CP_ACP);

// optimized versions that work faster for ASCII strings
void MultiByteToUnicodeString2(UString &dest, const AString &srcString, UINT codePage = CP_ACP);
// void UnicodeStringToMultiByte2(AString &dest, const UString &s, UINT codePage, char defaultChar, bool &defaultCharWasUsed);
void UnicodeStringToMultiByte2(AString &dest, const UString &srcString, UINT codePage);

AString UnicodeStringToMultiByte(const UString &srcString, UINT codePage, char defaultChar, bool &defaultCharWasUsed);
AString UnicodeStringToMultiByte(const UString &srcString, UINT codePage = CP_ACP);

inline const wchar_t* GetUnicodeString(const wchar_t* unicodeString)
  { return unicodeString; }
inline const UString& GetUnicodeString(const UString &unicodeString)
  { return unicodeString; }
inline UString GetUnicodeString(const AString &ansiString)
  { return MultiByteToUnicodeString(ansiString); }
inline UString GetUnicodeString(const AString &multiByteString, UINT codePage)
  { return MultiByteToUnicodeString(multiByteString, codePage); }
inline const wchar_t* GetUnicodeString(const wchar_t* unicodeString, UINT)
  { return unicodeString; }
inline const UString& GetUnicodeString(const UString &unicodeString, UINT)
  { return unicodeString; }

inline const char* GetAnsiString(const char* ansiString)
  { return ansiString; }
inline const AString& GetAnsiString(const AString &ansiString)
  { return ansiString; }
inline AString GetAnsiString(const UString &unicodeString)
  { return UnicodeStringToMultiByte(unicodeString); }

inline const char* GetOemString(const char* oemString)
  { return oemString; }
inline const AString& GetOemString(const AString &oemString)
  { return oemString; }
inline AString GetOemString(const UString &unicodeString)
  { return UnicodeStringToMultiByte(unicodeString, CP_OEMCP); }


#ifdef _UNICODE
  inline const wchar_t* GetSystemString(const wchar_t* unicodeString)
    { return unicodeString;}
  inline const UString& GetSystemString(const UString &unicodeString)
    { return unicodeString;}
  inline const wchar_t* GetSystemString(const wchar_t* unicodeString, UINT /* codePage */)
    { return unicodeString;}
  inline const UString& GetSystemString(const UString &unicodeString, UINT /* codePage */)
    { return unicodeString;}
  inline UString GetSystemString(const AString &multiByteString, UINT codePage)
    { return MultiByteToUnicodeString(multiByteString, codePage);}
  inline UString GetSystemString(const AString &multiByteString)
    { return MultiByteToUnicodeString(multiByteString);}
#else
  inline const char* GetSystemString(const char *ansiString)
    { return ansiString; }
  inline const AString& GetSystemString(const AString &multiByteString, UINT)
    { return multiByteString; }
  inline const char * GetSystemString(const char *multiByteString, UINT)
    { return multiByteString; }
  inline AString GetSystemString(const UString &unicodeString)
    { return UnicodeStringToMultiByte(unicodeString); }
  inline AString GetSystemString(const UString &unicodeString, UINT codePage)
    { return UnicodeStringToMultiByte(unicodeString, codePage); }
#endif

#ifndef UNDER_CE
AString SystemStringToOemString(const CSysString &srcString);
#endif

#endif
