// Windows/FileIO.cpp

#include "StdAfx.h"

#ifdef SUPPORT_DEVICE_FILE
#include "../../C/Alloc.h"
#endif

#include "FileIO.h"
#include "FileName.h"

#ifndef _UNICODE
extern bool g_IsNT;
#endif

using namespace NWindows;
using namespace NFile;
using namespace NName;

namespace NWindows {
namespace NFile {

#ifdef SUPPORT_DEVICE_FILE

namespace NSystem
{
bool MyGetDiskFreeSpace(CFSTR rootPath, UInt64 &clusterSize, UInt64 &totalSize, UInt64 &freeSize);
}
#endif

namespace NIO {

/*
WinXP-64 CreateFile():
  ""             -  ERROR_PATH_NOT_FOUND
  :stream        -  OK
  .:stream       -  ERROR_PATH_NOT_FOUND
  .\:stream      -  OK
  
  folder\:stream -  ERROR_INVALID_NAME
  folder:stream  -  OK

  c:\:stream     -  OK

  c::stream      -  ERROR_INVALID_NAME, if current dir is NOT ROOT ( c:\dir1 )
  c::stream      -  OK,                 if current dir is ROOT     ( c:\ )
*/

bool CFileBase::Create(CFSTR path, DWORD desiredAccess,
    DWORD shareMode, DWORD creationDisposition, DWORD flagsAndAttributes)
{
  if (!Close())
    return false;

  #ifdef SUPPORT_DEVICE_FILE
  IsDeviceFile = false;
  #endif

  #ifndef _UNICODE
  if (!g_IsNT)
  {
    _handle = ::CreateFile(fs2fas(path), desiredAccess, shareMode,
        (LPSECURITY_ATTRIBUTES)NULL, creationDisposition, flagsAndAttributes, (HANDLE)NULL);
  }
  else
  #endif
  {
    IF_USE_MAIN_PATH
      _handle = ::CreateFileW(fs2us(path), desiredAccess, shareMode,
        (LPSECURITY_ATTRIBUTES)NULL, creationDisposition, flagsAndAttributes, (HANDLE)NULL);
    #ifdef WIN_LONG_PATH
    if (_handle == INVALID_HANDLE_VALUE && USE_SUPER_PATH)
    {
      UString superPath;
      if (GetSuperPath(path, superPath, USE_MAIN_PATH))
        _handle = ::CreateFileW(superPath, desiredAccess, shareMode,
            (LPSECURITY_ATTRIBUTES)NULL, creationDisposition, flagsAndAttributes, (HANDLE)NULL);
    }
    #endif
  }
  return (_handle != INVALID_HANDLE_VALUE);
}

bool CFileBase::Close() throw()
{
  if (_handle == INVALID_HANDLE_VALUE)
    return true;
  if (!::CloseHandle(_handle))
    return false;
  _handle = INVALID_HANDLE_VALUE;
  return true;
}

bool CFileBase::GetPosition(UInt64 &position) const throw()
{
  return Seek(0, FILE_CURRENT, position);
}

bool CFileBase::GetLength(UInt64 &length) const throw()
{
  #ifdef SUPPORT_DEVICE_FILE
  if (IsDeviceFile && SizeDefined)
  {
    length = Size;
    return true;
  }
  #endif

  DWORD sizeHigh;
  DWORD sizeLow = ::GetFileSize(_handle, &sizeHigh);
  if (sizeLow == 0xFFFFFFFF)
    if (::GetLastError() != NO_ERROR)
      return false;
  length = (((UInt64)sizeHigh) << 32) + sizeLow;
  return true;
}

bool CFileBase::Seek(Int64 distanceToMove, DWORD moveMethod, UInt64 &newPosition) const throw()
{
  #ifdef SUPPORT_DEVICE_FILE
  if (IsDeviceFile && SizeDefined && moveMethod == FILE_END)
  {
    distanceToMove += Size;
    moveMethod = FILE_BEGIN;
  }
  #endif

  LONG high = (LONG)(distanceToMove >> 32);
  DWORD low = ::SetFilePointer(_handle, (LONG)(distanceToMove & 0xFFFFFFFF), &high, moveMethod);
  if (low == 0xFFFFFFFF)
    if (::GetLastError() != NO_ERROR)
      return false;
  newPosition = (((UInt64)(UInt32)high) << 32) + low;
  return true;
}

bool CFileBase::Seek(UInt64 position, UInt64 &newPosition) const throw()
{
  return Seek(position, FILE_BEGIN, newPosition);
}

bool CFileBase::SeekToBegin() const throw()
{
  UInt64 newPosition;
  return Seek(0, newPosition);
}

bool CFileBase::SeekToEnd(UInt64 &newPosition) const throw()
{
  return Seek(0, FILE_END, newPosition);
}

// ---------- CInFile ---------

#ifdef SUPPORT_DEVICE_FILE

void CInFile::CorrectDeviceSize()
{
  // maybe we must decrease kClusterSize to 1 << 12, if we want correct size at tail
  static const UInt32 kClusterSize = 1 << 14;
  UInt64 pos = Size & ~(UInt64)(kClusterSize - 1);
  UInt64 realNewPosition;
  if (!Seek(pos, realNewPosition))
    return;
  Byte *buf = (Byte *)MidAlloc(kClusterSize);

  bool needbackward = true;

  for (;;)
  {
    UInt32 processed = 0;
    // up test is slow for "PhysicalDrive".
    // processed size for latest block for "PhysicalDrive0" is 0.
    if (!Read1(buf, kClusterSize, processed))
      break;
    if (processed == 0)
      break;
    needbackward = false;
    Size = pos + processed;
    if (processed != kClusterSize)
      break;
    pos += kClusterSize;
  }

  if (needbackward && pos != 0)
  {
    pos -= kClusterSize;
    for (;;)
    {
      // break;
      if (!Seek(pos, realNewPosition))
        break;
      if (!buf)
      {
        buf = (Byte *)MidAlloc(kClusterSize);
        if (!buf)
          break;
      }
      UInt32 processed = 0;
      // that code doesn't work for "PhysicalDrive0"
      if (!Read1(buf, kClusterSize, processed))
        break;
      if (processed != 0)
      {
        Size = pos + processed;
        break;
      }
      if (pos == 0)
        break;
      pos -= kClusterSize;
    }
  }
  MidFree(buf);
}


void CInFile::CalcDeviceSize(CFSTR s)
{
  SizeDefined = false;
  Size = 0;
  if (_handle == INVALID_HANDLE_VALUE || !IsDeviceFile)
    return;
  #ifdef UNDER_CE

  SizeDefined = true;
  Size = 128 << 20;
  
  #else
  
  PARTITION_INFORMATION partInfo;
  bool needCorrectSize = true;

  /*
    WinXP 64-bit:

    HDD \\.\PhysicalDrive0 (MBR):
      GetPartitionInfo == GeometryEx :  corrrect size? (includes tail)
      Geometry   :  smaller than GeometryEx (no tail, maybe correct too?)
      MyGetDiskFreeSpace : FAIL
      Size correction is slow and block size (kClusterSize) must be small?

    HDD partition \\.\N: (NTFS):
      MyGetDiskFreeSpace   :  Size of NTFS clusters. Same size can be calculated after correction
      GetPartitionInfo     :  size of partition data: NTFS clusters + TAIL; TAIL contains extra empty sectors and copy of first sector of NTFS
      Geometry / CdRomGeometry / GeometryEx :  size of HDD (not that partition)

    CD-ROM drive (ISO):
      MyGetDiskFreeSpace   :  correct size. Same size can be calculated after correction
      Geometry == CdRomGeometry  :  smaller than corrrect size
      GetPartitionInfo == GeometryEx :  larger than corrrect size

    Floppy \\.\a: (FAT):
      Geometry :  correct size.
      CdRomGeometry / GeometryEx / GetPartitionInfo / MyGetDiskFreeSpace - FAIL
      correction works OK for FAT.
      correction works OK for non-FAT, if kClusterSize = 512.
  */

  if (GetPartitionInfo(&partInfo))
  {
    Size = partInfo.PartitionLength.QuadPart;
    SizeDefined = true;
    needCorrectSize = false;
    if ((s)[0] == '\\' && (s)[1] == '\\' && (s)[2] == '.' && (s)[3] == '\\' && (s)[5] == ':' && (s)[6] == 0)
    {
      FChar path[4] = { s[4], ':', '\\', 0 };
      UInt64 clusterSize, totalSize, freeSize;
      if (NSystem::MyGetDiskFreeSpace(path, clusterSize, totalSize, freeSize))
        Size = totalSize;
      else
        needCorrectSize = true;
    }
  }
  
  if (!SizeDefined)
  {
    my_DISK_GEOMETRY_EX geomEx;
    SizeDefined = GetGeometryEx(&geomEx);
    if (SizeDefined)
      Size = geomEx.DiskSize.QuadPart;
    else
    {
      DISK_GEOMETRY geom;
      SizeDefined = GetGeometry(&geom);
      if (!SizeDefined)
        SizeDefined = GetCdRomGeometry(&geom);
      if (SizeDefined)
        Size = geom.Cylinders.QuadPart * geom.TracksPerCylinder * geom.SectorsPerTrack * geom.BytesPerSector;
    }
  }
  
  if (needCorrectSize && SizeDefined && Size != 0)
  {
    CorrectDeviceSize();
    SeekToBegin();
  }

  // SeekToBegin();
  #endif
}

// ((desiredAccess & (FILE_WRITE_DATA | FILE_APPEND_DATA | GENERIC_WRITE)) == 0 &&

#define MY_DEVICE_EXTRA_CODE \
  IsDeviceFile = IsDevicePath(fileName); \
  CalcDeviceSize(fileName);
#else
#define MY_DEVICE_EXTRA_CODE
#endif

bool CInFile::Open(CFSTR fileName, DWORD shareMode, DWORD creationDisposition, DWORD flagsAndAttributes)
{
  bool res = Create(fileName, GENERIC_READ, shareMode, creationDisposition, flagsAndAttributes);
  MY_DEVICE_EXTRA_CODE
  return res;
}

bool CInFile::OpenShared(CFSTR fileName, bool shareForWrite)
{ return Open(fileName, FILE_SHARE_READ | (shareForWrite ? FILE_SHARE_WRITE : 0), OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL); }

bool CInFile::Open(CFSTR fileName)
  { return OpenShared(fileName, false); }

// ReadFile and WriteFile functions in Windows have BUG:
// If you Read or Write 64MB or more (probably min_failure_size = 64MB - 32KB + 1)
// from/to Network file, it returns ERROR_NO_SYSTEM_RESOURCES
// (Insufficient system resources exist to complete the requested service).

// Probably in some version of Windows there are problems with other sizes:
// for 32 MB (maybe also for 16 MB).
// And message can be "Network connection was lost"

static UInt32 kChunkSizeMax = (1 << 22);

bool CInFile::Read1(void *data, UInt32 size, UInt32 &processedSize) throw()
{
  DWORD processedLoc = 0;
  bool res = BOOLToBool(::ReadFile(_handle, data, size, &processedLoc, NULL));
  processedSize = (UInt32)processedLoc;
  return res;
}

bool CInFile::ReadPart(void *data, UInt32 size, UInt32 &processedSize) throw()
{
  if (size > kChunkSizeMax)
    size = kChunkSizeMax;
  return Read1(data, size, processedSize);
}

bool CInFile::Read(void *data, UInt32 size, UInt32 &processedSize) throw()
{
  processedSize = 0;
  do
  {
    UInt32 processedLoc = 0;
    bool res = ReadPart(data, size, processedLoc);
    processedSize += processedLoc;
    if (!res)
      return false;
    if (processedLoc == 0)
      return true;
    data = (void *)((unsigned char *)data + processedLoc);
    size -= processedLoc;
  }
  while (size > 0);
  return true;
}

// ---------- COutFile ---------

static inline DWORD GetCreationDisposition(bool createAlways)
  { return createAlways? CREATE_ALWAYS: CREATE_NEW; }

bool COutFile::Open(CFSTR fileName, DWORD shareMode, DWORD creationDisposition, DWORD flagsAndAttributes)
  { return CFileBase::Create(fileName, GENERIC_WRITE, shareMode, creationDisposition, flagsAndAttributes); }

bool COutFile::Open(CFSTR fileName, DWORD creationDisposition)
  { return Open(fileName, FILE_SHARE_READ, creationDisposition, FILE_ATTRIBUTE_NORMAL); }

bool COutFile::Create(CFSTR fileName, bool createAlways)
  { return Open(fileName, GetCreationDisposition(createAlways)); }

bool COutFile::CreateAlways(CFSTR fileName, DWORD flagsAndAttributes)
  { return Open(fileName, FILE_SHARE_READ, GetCreationDisposition(true), flagsAndAttributes); }

bool COutFile::SetTime(const FILETIME *cTime, const FILETIME *aTime, const FILETIME *mTime) throw()
  { return BOOLToBool(::SetFileTime(_handle, cTime, aTime, mTime)); }

bool COutFile::SetMTime(const FILETIME *mTime) throw() {  return SetTime(NULL, NULL, mTime); }

bool COutFile::WritePart(const void *data, UInt32 size, UInt32 &processedSize) throw()
{
  if (size > kChunkSizeMax)
    size = kChunkSizeMax;
  DWORD processedLoc = 0;
  bool res = BOOLToBool(::WriteFile(_handle, data, size, &processedLoc, NULL));
  processedSize = (UInt32)processedLoc;
  return res;
}

bool COutFile::Write(const void *data, UInt32 size, UInt32 &processedSize) throw()
{
  processedSize = 0;
  do
  {
    UInt32 processedLoc = 0;
    bool res = WritePart(data, size, processedLoc);
    processedSize += processedLoc;
    if (!res)
      return false;
    if (processedLoc == 0)
      return true;
    data = (const void *)((const unsigned char *)data + processedLoc);
    size -= processedLoc;
  }
  while (size > 0);
  return true;
}

bool COutFile::SetEndOfFile() throw() { return BOOLToBool(::SetEndOfFile(_handle)); }

bool COutFile::SetLength(UInt64 length) throw()
{
  UInt64 newPosition;
  if (!Seek(length, newPosition))
    return false;
  if (newPosition != length)
    return false;
  return SetEndOfFile();
}

}}}
