// Windows/FileIO.h

#ifndef __WINDOWS_FILE_IO_H
#define __WINDOWS_FILE_IO_H

#include "../Common/MyWindows.h"

#if defined(_WIN32) && !defined(UNDER_CE)
#include <winioctl.h>
#endif

#include "../Common/MyString.h"
#include "../Common/MyBuffer.h"

#include "Defs.h"

#define _my_IO_REPARSE_TAG_MOUNT_POINT  (0xA0000003L)
#define _my_IO_REPARSE_TAG_SYMLINK      (0xA000000CL)

#define _my_SYMLINK_FLAG_RELATIVE 1

#define my_FSCTL_SET_REPARSE_POINT  CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 41, METHOD_BUFFERED, FILE_SPECIAL_ACCESS) // REPARSE_DATA_BUFFER
#define my_FSCTL_GET_REPARSE_POINT  CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 42, METHOD_BUFFERED, FILE_ANY_ACCESS)     // REPARSE_DATA_BUFFER

namespace NWindows {
namespace NFile {

#if defined(_WIN32) && !defined(UNDER_CE)
bool FillLinkData(CByteBuffer &dest, const wchar_t *path, bool isSymLink);
#endif

struct CReparseShortInfo
{
  unsigned Offset;
  unsigned Size;

  bool Parse(const Byte *p, size_t size);
};

struct CReparseAttr
{
  UInt32 Tag;
  UInt32 Flags;
  UString SubsName;
  UString PrintName;

  CReparseAttr(): Tag(0), Flags(0) {}

  // Parse()
  // returns true and (errorCode = 0), if (correct MOUNT_POINT or SYMLINK)
  // returns false and (errorCode = ERROR_REPARSE_TAG_MISMATCH), if not (MOUNT_POINT or SYMLINK)
  bool Parse(const Byte *p, size_t size, DWORD &errorCode);

  bool IsMountPoint() const { return Tag == _my_IO_REPARSE_TAG_MOUNT_POINT; } // it's Junction
  bool IsSymLink() const { return Tag == _my_IO_REPARSE_TAG_SYMLINK; }
  bool IsRelative() const { return Flags == _my_SYMLINK_FLAG_RELATIVE; }
  // bool IsVolume() const;

  bool IsOkNamePair() const;
  UString GetPath() const;
};

namespace NIO {

bool GetReparseData(CFSTR path, CByteBuffer &reparseData, BY_HANDLE_FILE_INFORMATION *fileInfo = NULL);
bool SetReparseData(CFSTR path, bool isDir, const void *data, DWORD size);

class CFileBase
{
protected:
  HANDLE _handle;
  
  bool Create(CFSTR path, DWORD desiredAccess,
      DWORD shareMode, DWORD creationDisposition, DWORD flagsAndAttributes);

public:

  bool DeviceIoControl(DWORD controlCode, LPVOID inBuffer, DWORD inSize,
      LPVOID outBuffer, DWORD outSize, LPDWORD bytesReturned, LPOVERLAPPED overlapped = NULL) const
  {
    return BOOLToBool(::DeviceIoControl(_handle, controlCode, inBuffer, inSize,
        outBuffer, outSize, bytesReturned, overlapped));
  }

  bool DeviceIoControlOut(DWORD controlCode, LPVOID outBuffer, DWORD outSize, LPDWORD bytesReturned) const
  {
    return DeviceIoControl(controlCode, NULL, 0, outBuffer, outSize, bytesReturned);
  }

  bool DeviceIoControlOut(DWORD controlCode, LPVOID outBuffer, DWORD outSize) const
  {
    DWORD bytesReturned;
    return DeviceIoControlOut(controlCode, outBuffer, outSize, &bytesReturned);
  }

public:
  #ifdef SUPPORT_DEVICE_FILE
  bool IsDeviceFile;
  bool SizeDefined;
  UInt64 Size; // it can be larger than real available size
  #endif

  CFileBase(): _handle(INVALID_HANDLE_VALUE) {};
  ~CFileBase() { Close(); }

  bool Close() throw();

  bool GetPosition(UInt64 &position) const throw();
  bool GetLength(UInt64 &length) const throw();

  bool Seek(Int64 distanceToMove, DWORD moveMethod, UInt64 &newPosition) const throw();
  bool Seek(UInt64 position, UInt64 &newPosition) const throw();
  bool SeekToBegin() const throw();
  bool SeekToEnd(UInt64 &newPosition) const throw();
  
  bool GetFileInformation(BY_HANDLE_FILE_INFORMATION *info) const
    { return BOOLToBool(GetFileInformationByHandle(_handle, info)); }

  static bool GetFileInformation(CFSTR path, BY_HANDLE_FILE_INFORMATION *info)
  {
    NIO::CFileBase file;
    if (!file.Create(path, 0, FILE_SHARE_READ, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS))
      return false;
    return file.GetFileInformation(info);
  }
};

#ifndef UNDER_CE
#define IOCTL_CDROM_BASE  FILE_DEVICE_CD_ROM
#define IOCTL_CDROM_GET_DRIVE_GEOMETRY  CTL_CODE(IOCTL_CDROM_BASE, 0x0013, METHOD_BUFFERED, FILE_READ_ACCESS)
// #define IOCTL_CDROM_MEDIA_REMOVAL  CTL_CODE(IOCTL_CDROM_BASE, 0x0201, METHOD_BUFFERED, FILE_READ_ACCESS)

// IOCTL_DISK_GET_DRIVE_GEOMETRY_EX works since WinXP
#define my_IOCTL_DISK_GET_DRIVE_GEOMETRY_EX  CTL_CODE(IOCTL_DISK_BASE, 0x0028, METHOD_BUFFERED, FILE_ANY_ACCESS)

struct my_DISK_GEOMETRY_EX
{
  DISK_GEOMETRY Geometry;
  LARGE_INTEGER DiskSize;
  BYTE Data[1];
};
#endif

class CInFile: public CFileBase
{
  #ifdef SUPPORT_DEVICE_FILE

  #ifndef UNDER_CE
  
  bool GetGeometry(DISK_GEOMETRY *res) const
    { return DeviceIoControlOut(IOCTL_DISK_GET_DRIVE_GEOMETRY, res, sizeof(*res)); }

  bool GetGeometryEx(my_DISK_GEOMETRY_EX *res) const
    { return DeviceIoControlOut(my_IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, res, sizeof(*res)); }

  bool GetCdRomGeometry(DISK_GEOMETRY *res) const
    { return DeviceIoControlOut(IOCTL_CDROM_GET_DRIVE_GEOMETRY, res, sizeof(*res)); }
  
  bool GetPartitionInfo(PARTITION_INFORMATION *res)
    { return DeviceIoControlOut(IOCTL_DISK_GET_PARTITION_INFO, LPVOID(res), sizeof(*res)); }
  
  #endif

  void CorrectDeviceSize();
  void CalcDeviceSize(CFSTR name);
  
  #endif

public:
  bool Open(CFSTR fileName, DWORD shareMode, DWORD creationDisposition, DWORD flagsAndAttributes);
  bool OpenShared(CFSTR fileName, bool shareForWrite);
  bool Open(CFSTR fileName);

  #ifndef UNDER_CE

  bool OpenReparse(CFSTR fileName)
  {
    // 17.02 fix: to support Windows XP compatibility junctions:
    //   we use Create() with (desiredAccess = 0) instead of Open() with GENERIC_READ
    return
        Create(fileName, 0,
        // Open(fileName,
        FILE_SHARE_READ, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS);
  }
  
  #endif

  bool Read1(void *data, UInt32 size, UInt32 &processedSize) throw();
  bool ReadPart(void *data, UInt32 size, UInt32 &processedSize) throw();
  bool Read(void *data, UInt32 size, UInt32 &processedSize) throw();
};

class COutFile: public CFileBase
{
public:
  bool Open(CFSTR fileName, DWORD shareMode, DWORD creationDisposition, DWORD flagsAndAttributes);
  bool Open(CFSTR fileName, DWORD creationDisposition);
  bool Create(CFSTR fileName, bool createAlways);
  bool CreateAlways(CFSTR fileName, DWORD flagsAndAttributes);

  bool SetTime(const FILETIME *cTime, const FILETIME *aTime, const FILETIME *mTime) throw();
  bool SetMTime(const FILETIME *mTime) throw();
  bool WritePart(const void *data, UInt32 size, UInt32 &processedSize) throw();
  bool Write(const void *data, UInt32 size, UInt32 &processedSize) throw();
  bool SetEndOfFile() throw();
  bool SetLength(UInt64 length) throw();
};

}}}

#endif
