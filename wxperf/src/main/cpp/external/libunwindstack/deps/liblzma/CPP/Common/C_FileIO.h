// Common/C_FileIO.h

#ifndef __COMMON_C_FILEIO_H
#define __COMMON_C_FILEIO_H

#include <stdio.h>
#include <sys/types.h>

#include "MyTypes.h"
#include "MyWindows.h"

#ifdef _WIN32
#ifdef _MSC_VER
typedef size_t ssize_t;
#endif
#endif

namespace NC {
namespace NFile {
namespace NIO {

class CFileBase
{
protected:
  int _handle;
  bool OpenBinary(const char *name, int flags);
public:
  CFileBase(): _handle(-1) {};
  ~CFileBase() { Close(); }
  bool Close();
  bool GetLength(UInt64 &length) const;
  off_t Seek(off_t distanceToMove, int moveMethod) const;
};

class CInFile: public CFileBase
{
public:
  bool Open(const char *name);
  bool OpenShared(const char *name, bool shareForWrite);
  ssize_t Read(void *data, size_t size);
};

class COutFile: public CFileBase
{
public:
  bool Create(const char *name, bool createAlways);
  bool Open(const char *name, DWORD creationDisposition);
  ssize_t Write(const void *data, size_t size);
};

}}}

#endif
