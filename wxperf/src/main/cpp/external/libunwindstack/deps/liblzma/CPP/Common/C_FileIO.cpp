// Common/C_FileIO.cpp

#include "C_FileIO.h"

#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace NC {
namespace NFile {
namespace NIO {

bool CFileBase::OpenBinary(const char *name, int flags)
{
  #ifdef O_BINARY
  flags |= O_BINARY;
  #endif
  Close();
  _handle = ::open(name, flags, 0666);
  return _handle != -1;
}

bool CFileBase::Close()
{
  if (_handle == -1)
    return true;
  if (close(_handle) != 0)
    return false;
  _handle = -1;
  return true;
}

bool CFileBase::GetLength(UInt64 &length) const
{
  off_t curPos = Seek(0, SEEK_CUR);
  off_t lengthTemp = Seek(0, SEEK_END);
  Seek(curPos, SEEK_SET);
  length = (UInt64)lengthTemp;
  return true;
}

off_t CFileBase::Seek(off_t distanceToMove, int moveMethod) const
{
  return ::lseek(_handle, distanceToMove, moveMethod);
}

/////////////////////////
// CInFile

bool CInFile::Open(const char *name)
{
  return CFileBase::OpenBinary(name, O_RDONLY);
}

bool CInFile::OpenShared(const char *name, bool)
{
  return Open(name);
}

ssize_t CInFile::Read(void *data, size_t size)
{
  return read(_handle, data, size);
}

/////////////////////////
// COutFile

bool COutFile::Create(const char *name, bool createAlways)
{
  if (createAlways)
  {
    Close();
    _handle = ::creat(name, 0666);
    return _handle != -1;
  }
  return OpenBinary(name, O_CREAT | O_EXCL | O_WRONLY);
}

bool COutFile::Open(const char *name, DWORD creationDisposition)
{
  return Create(name, false);
}

ssize_t COutFile::Write(const void *data, size_t size)
{
  return write(_handle, data, size);
}

}}}
