// Windows/TimeUtils.h

#ifndef __WINDOWS_TIME_UTILS_H
#define __WINDOWS_TIME_UTILS_H

#include "../Common/MyTypes.h"
#include "../Common/MyWindows.h"

namespace NWindows {
namespace NTime {

bool DosTimeToFileTime(UInt32 dosTime, FILETIME &fileTime) throw();
bool FileTimeToDosTime(const FILETIME &fileTime, UInt32 &dosTime) throw();
void UnixTimeToFileTime(UInt32 unixTime, FILETIME &fileTime) throw();
bool UnixTime64ToFileTime(Int64 unixTime, FILETIME &fileTime) throw();
bool FileTimeToUnixTime(const FILETIME &fileTime, UInt32 &unixTime) throw();
Int64 FileTimeToUnixTime64(const FILETIME &ft) throw();
bool GetSecondsSince1601(unsigned year, unsigned month, unsigned day,
  unsigned hour, unsigned min, unsigned sec, UInt64 &resSeconds) throw();
void GetCurUtcFileTime(FILETIME &ft) throw();

}}

#endif
