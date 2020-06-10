// Windows/FileFind.h

#ifndef __WINDOWS_FILE_FIND_H
#define __WINDOWS_FILE_FIND_H

#include "../Common/MyString.h"
#include "Defs.h"

namespace NWindows {
namespace NFile {
namespace NFind {

namespace NAttributes
{
  inline bool IsReadOnly(DWORD attrib) { return (attrib & FILE_ATTRIBUTE_READONLY) != 0; }
  inline bool IsHidden(DWORD attrib) { return (attrib & FILE_ATTRIBUTE_HIDDEN) != 0; }
  inline bool IsSystem(DWORD attrib) { return (attrib & FILE_ATTRIBUTE_SYSTEM) != 0; }
  inline bool IsDir(DWORD attrib) { return (attrib & FILE_ATTRIBUTE_DIRECTORY) != 0; }
  inline bool IsArchived(DWORD attrib) { return (attrib & FILE_ATTRIBUTE_ARCHIVE) != 0; }
  inline bool IsCompressed(DWORD attrib) { return (attrib & FILE_ATTRIBUTE_COMPRESSED) != 0; }
  inline bool IsEncrypted(DWORD attrib) { return (attrib & FILE_ATTRIBUTE_ENCRYPTED) != 0; }
}

class CFileInfoBase
{
  bool MatchesMask(UINT32 mask) const { return ((Attrib & mask) != 0); }
public:
  UInt64 Size;
  FILETIME CTime;
  FILETIME ATime;
  FILETIME MTime;
  DWORD Attrib;
  bool IsAltStream;
  bool IsDevice;

  /*
  #ifdef UNDER_CE
  DWORD ObjectID;
  #else
  UINT32 ReparseTag;
  #endif
  */

  CFileInfoBase() { ClearBase(); }
  void ClearBase() throw();

  void SetAsDir() { Attrib = FILE_ATTRIBUTE_DIRECTORY; }

  bool IsArchived() const { return MatchesMask(FILE_ATTRIBUTE_ARCHIVE); }
  bool IsCompressed() const { return MatchesMask(FILE_ATTRIBUTE_COMPRESSED); }
  bool IsDir() const { return MatchesMask(FILE_ATTRIBUTE_DIRECTORY); }
  bool IsEncrypted() const { return MatchesMask(FILE_ATTRIBUTE_ENCRYPTED); }
  bool IsHidden() const { return MatchesMask(FILE_ATTRIBUTE_HIDDEN); }
  bool IsNormal() const { return MatchesMask(FILE_ATTRIBUTE_NORMAL); }
  bool IsOffline() const { return MatchesMask(FILE_ATTRIBUTE_OFFLINE); }
  bool IsReadOnly() const { return MatchesMask(FILE_ATTRIBUTE_READONLY); }
  bool HasReparsePoint() const { return MatchesMask(FILE_ATTRIBUTE_REPARSE_POINT); }
  bool IsSparse() const { return MatchesMask(FILE_ATTRIBUTE_SPARSE_FILE); }
  bool IsSystem() const { return MatchesMask(FILE_ATTRIBUTE_SYSTEM); }
  bool IsTemporary() const { return MatchesMask(FILE_ATTRIBUTE_TEMPORARY); }
};

struct CFileInfo: public CFileInfoBase
{
  FString Name;
  #if defined(_WIN32) && !defined(UNDER_CE)
  // FString ShortName;
  #endif

  bool IsDots() const throw();
  bool Find(CFSTR path);
};

class CFindFileBase
{
protected:
  HANDLE _handle;
public:
  bool IsHandleAllocated() const { return _handle != INVALID_HANDLE_VALUE; }
  CFindFileBase(): _handle(INVALID_HANDLE_VALUE) {}
  ~CFindFileBase() { Close(); }
  bool Close() throw();
};

class CFindFile: public CFindFileBase
{
public:
  bool FindFirst(CFSTR wildcard, CFileInfo &fileInfo);
  bool FindNext(CFileInfo &fileInfo);
};

#if defined(_WIN32) && !defined(UNDER_CE)

struct CStreamInfo
{
  UString Name;
  UInt64 Size;

  UString GetReducedName() const; // returns ":Name"
  // UString GetReducedName2() const; // returns "Name"
  bool IsMainStream() const throw();
};

class CFindStream: public CFindFileBase
{
public:
  bool FindFirst(CFSTR filePath, CStreamInfo &streamInfo);
  bool FindNext(CStreamInfo &streamInfo);
};

class CStreamEnumerator
{
  CFindStream _find;
  FString _filePath;

  bool NextAny(CFileInfo &fileInfo);
public:
  CStreamEnumerator(const FString &filePath): _filePath(filePath) {}
  bool Next(CStreamInfo &streamInfo, bool &found);
};

#endif

bool DoesFileExist(CFSTR name);
bool DoesDirExist(CFSTR name);
bool DoesFileOrDirExist(CFSTR name);

DWORD GetFileAttrib(CFSTR path);

class CEnumerator
{
  CFindFile _findFile;
  FString _wildcard;

  bool NextAny(CFileInfo &fileInfo);
public:
  void SetDirPrefix(const FString &dirPrefix);
  bool Next(CFileInfo &fileInfo);
  bool Next(CFileInfo &fileInfo, bool &found);
};

class CFindChangeNotification
{
  HANDLE _handle;
public:
  operator HANDLE () { return _handle; }
  bool IsHandleAllocated() const { return _handle != INVALID_HANDLE_VALUE && _handle != 0; }
  CFindChangeNotification(): _handle(INVALID_HANDLE_VALUE) {}
  ~CFindChangeNotification() { Close(); }
  bool Close() throw();
  HANDLE FindFirst(CFSTR pathName, bool watchSubtree, DWORD notifyFilter);
  bool FindNext() { return BOOLToBool(::FindNextChangeNotification(_handle)); }
};

#ifndef UNDER_CE
bool MyGetLogicalDriveStrings(CObjectVector<FString> &driveStrings);
#endif

}}}

#endif
