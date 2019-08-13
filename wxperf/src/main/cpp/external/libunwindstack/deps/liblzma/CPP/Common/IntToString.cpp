// Common/IntToString.cpp

#include "StdAfx.h"

#include "IntToString.h"

#define CONVERT_INT_TO_STR(charType, tempSize) \
  unsigned char temp[tempSize]; unsigned i = 0; \
  while (val >= 10) { temp[i++] = (unsigned char)('0' + (unsigned)(val % 10)); val /= 10; } \
  *s++ = (charType)('0' + (unsigned)val); \
  while (i != 0) { i--; *s++ = temp[i]; } \
  *s = 0;

void ConvertUInt32ToString(UInt32 val, char *s) throw()
{
  CONVERT_INT_TO_STR(char, 16);
}

void ConvertUInt64ToString(UInt64 val, char *s) throw()
{
  if (val <= (UInt32)0xFFFFFFFF)
  {
    ConvertUInt32ToString((UInt32)val, s);
    return;
  }
  CONVERT_INT_TO_STR(char, 24);
}

void ConvertUInt64ToOct(UInt64 val, char *s) throw()
{
  UInt64 v = val;
  unsigned i;
  for (i = 1;; i++)
  {
    v >>= 3;
    if (v == 0)
      break;
  }
  s[i] = 0;
  do
  {
    unsigned t = (unsigned)(val & 0x7);
    val >>= 3;
    s[--i] = (char)('0' + t);
  }
  while (i);
}

void ConvertUInt32ToHex(UInt32 val, char *s) throw()
{
  UInt32 v = val;
  unsigned i;
  for (i = 1;; i++)
  {
    v >>= 4;
    if (v == 0)
      break;
  }
  s[i] = 0;
  do
  {
    unsigned t = (unsigned)((val & 0xF));
    val >>= 4;
    s[--i] = (char)((t < 10) ? ('0' + t) : ('A' + (t - 10)));
  }
  while (i);
}

void ConvertUInt64ToHex(UInt64 val, char *s) throw()
{
  UInt64 v = val;
  unsigned i;
  for (i = 1;; i++)
  {
    v >>= 4;
    if (v == 0)
      break;
  }
  s[i] = 0;
  do
  {
    unsigned t = (unsigned)((val & 0xF));
    val >>= 4;
    s[--i] = (char)((t < 10) ? ('0' + t) : ('A' + (t - 10)));
  }
  while (i);
}

void ConvertUInt32ToHex8Digits(UInt32 val, char *s) throw()
{
  s[8] = 0;
  for (int i = 7; i >= 0; i--)
  {
    unsigned t = val & 0xF;
    val >>= 4;
    s[i] = (char)(((t < 10) ? ('0' + t) : ('A' + (t - 10))));
  }
}

/*
void ConvertUInt32ToHex8Digits(UInt32 val, wchar_t *s)
{
  s[8] = 0;
  for (int i = 7; i >= 0; i--)
  {
    unsigned t = val & 0xF;
    val >>= 4;
    s[i] = (wchar_t)(((t < 10) ? ('0' + t) : ('A' + (t - 10))));
  }
}
*/

void ConvertUInt32ToString(UInt32 val, wchar_t *s) throw()
{
  CONVERT_INT_TO_STR(wchar_t, 16);
}

void ConvertUInt64ToString(UInt64 val, wchar_t *s) throw()
{
  if (val <= (UInt32)0xFFFFFFFF)
  {
    ConvertUInt32ToString((UInt32)val, s);
    return;
  }
  CONVERT_INT_TO_STR(wchar_t, 24);
}

void ConvertInt64ToString(Int64 val, char *s) throw()
{
  if (val < 0)
  {
    *s++ = '-';
    val = -val;
  }
  ConvertUInt64ToString(val, s);
}

void ConvertInt64ToString(Int64 val, wchar_t *s) throw()
{
  if (val < 0)
  {
    *s++ = L'-';
    val = -val;
  }
  ConvertUInt64ToString(val, s);
}
