// Common/StdInStream.h

#ifndef __COMMON_STD_IN_STREAM_H
#define __COMMON_STD_IN_STREAM_H

#include <stdio.h>

#include "MyString.h"
#include "MyTypes.h"

class CStdInStream
{
  FILE *_stream;
  bool _streamIsOpen;
public:
  CStdInStream(): _stream(0), _streamIsOpen(false) {};
  CStdInStream(FILE *stream): _stream(stream), _streamIsOpen(false) {};
  ~CStdInStream() { Close(); }

  bool Open(LPCTSTR fileName) throw();
  bool Close() throw();

  AString ScanStringUntilNewLine(bool allowEOF = false);
  void ReadToString(AString &resultString);
  UString ScanUStringUntilNewLine();

  bool Eof() throw();
  int GetChar();
};

extern CStdInStream g_StdIn;

#endif
