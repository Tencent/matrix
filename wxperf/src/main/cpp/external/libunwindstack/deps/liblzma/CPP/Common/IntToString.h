// Common/IntToString.h

#ifndef __COMMON_INT_TO_STRING_H
#define __COMMON_INT_TO_STRING_H

#include "MyTypes.h"

void ConvertUInt32ToString(UInt32 value, char *s) throw();
void ConvertUInt64ToString(UInt64 value, char *s) throw();

void ConvertUInt32ToString(UInt32 value, wchar_t *s) throw();
void ConvertUInt64ToString(UInt64 value, wchar_t *s) throw();

void ConvertUInt64ToOct(UInt64 value, char *s) throw();

void ConvertUInt32ToHex(UInt32 value, char *s) throw();
void ConvertUInt64ToHex(UInt64 value, char *s) throw();
void ConvertUInt32ToHex8Digits(UInt32 value, char *s) throw();
// void ConvertUInt32ToHex8Digits(UInt32 value, wchar_t *s) throw();

void ConvertInt64ToString(Int64 value, char *s) throw();
void ConvertInt64ToString(Int64 value, wchar_t *s) throw();

#endif
