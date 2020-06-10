// Common/StdOutStream.h

#ifndef __COMMON_STD_OUT_STREAM_H
#define __COMMON_STD_OUT_STREAM_H

#include <stdio.h>

#include "MyString.h"
#include "MyTypes.h"

class CStdOutStream
{
  FILE *_stream;
  bool _streamIsOpen;
public:
  bool IsTerminalMode;

  CStdOutStream(): _stream(0), _streamIsOpen(false), IsTerminalMode(false) {};
  CStdOutStream(FILE *stream): _stream(stream), _streamIsOpen(false) {};
  ~CStdOutStream() { Close(); }

  // void AttachStdStream(FILE *stream) { _stream  = stream; _streamIsOpen = false; }
  // bool IsDefined() const { return _stream  != NULL; }

  operator FILE *() { return _stream; }
  bool Open(const char *fileName) throw();
  bool Close() throw();
  bool Flush() throw();
  
  CStdOutStream & operator<<(CStdOutStream & (* func)(CStdOutStream  &))
  {
    (*func)(*this);
    return *this;
  }

  CStdOutStream & operator<<(const char *s) throw()
  {
    fputs(s, _stream);
    return *this;
  }

  CStdOutStream & operator<<(char c) throw()
  {
    fputc((unsigned char)c, _stream);
    return *this;
  }

  CStdOutStream & operator<<(Int32 number) throw();
  CStdOutStream & operator<<(Int64 number) throw();
  CStdOutStream & operator<<(UInt32 number) throw();
  CStdOutStream & operator<<(UInt64 number) throw();

  CStdOutStream & operator<<(const wchar_t *s);
  void PrintUString(const UString &s, AString &temp);

  void Normalize_UString__LF_Allowed(UString &s);
  void Normalize_UString(UString &s);

  void NormalizePrint_UString(const UString &s, UString &tempU, AString &tempA);
  void NormalizePrint_UString(const UString &s);
  void NormalizePrint_wstr(const wchar_t *s);
};

CStdOutStream & endl(CStdOutStream & outStream) throw();

extern CStdOutStream g_StdOut;
extern CStdOutStream g_StdErr;

void StdOut_Convert_UString_to_AString(const UString &s, AString &temp);

#endif
