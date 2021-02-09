// Windows/FileFind.cpp

#include "StdAfx.h"

#ifndef _UNICODE
#include "../Common/StringConvert.h"
#endif

#include "FileFind.h"
#include "FileIO.h"
#include "FileName.h"

#ifndef _UNICODE
extern bool g_IsNT;
#endif

using namespace NWindows;
using namespace NFile;
using namespace NName;

#if defined(_WIN32) && !defined(UNDER_CE)

EXTERN_C_BEGIN

typedef enum
{
  My_FindStreamInfoStandard,
  My_FindStreamInfoMaxInfoLevel
} MY_STREAM_INFO_LEVELS;

typedef struct
{
  LARGE_INTEGER StreamSize;
  WCHAR cStreamName[MAX_PATH + 36];
} MY_WIN32_FIND_STREAM_DATA, *MY_PWIN32_FIND_STREAM_DATA;

typedef WINBASEAPI HANDLE (WINAPI *FindFirstStreamW_Ptr)(LPCWSTR fileName, MY_STREAM_INFO_LEVELS infoLevel,
    LPVOID findStreamData, DWORD flags);

typedef WINBASEAPI BOOL (APIENTRY *FindNextStreamW_Ptr)(HANDLE findStream, LPVOID findStreamData);

EXTERN_C_END

#endif

namespace NWindows {
namespace NFile {

#ifdef SUPPORT_DEVICE_FILE
namespace NSystem
{
bool MyGetDiskFreeSpace(CFSTR rootPath, UInt64 &clusterSize, UInt64 &totalSize, UInt64 &freeSize);
}
#endif

namespace NFind {

bool CFileInfo::IsDots() const throw()
{
  if (!IsDir() || Name.IsEmpty())
    return false;
  if (Name[0] != '.')
    return false;
  return Name.Len() == 1 || (Name.Len() == 2 && Name[1] == '.');
}

#define WIN_FD_TO_MY_FI(fi, fd) \
  fi.Attrib = fd.dwFileAttributes; \
  fi.CTime = fd.ftCreationTime; \
  fi.ATime = fd.ftLastAccessTime; \
  fi.MTime = fd.ftLastWriteTime; \
  fi.Size = (((UInt64)fd.nFileSizeHigh) << 32) + fd.nFileSizeLow; \
  fi.IsAltStream = false; \
  fi.IsDevice = false;

  /*
  #ifdef UNDER_CE
  fi.ObjectID = fd.dwOID;
  #else
  fi.ReparseTag = fd.dwReserved0;
  #endif
  */

static void Convert_WIN32_FIND_DATA_to_FileInfo(const WIN32_FIND_DATAW &fd, CFileInfo &fi)
{
  WIN_FD_TO_MY_FI(fi, fd);
  fi.Name = us2fs(fd.cFileName);
  #if defined(_WIN32) && !defined(UNDER_CE)
  // fi.ShortName = us2fs(fd.cAlternateFileName);
  #endif
}

#ifndef _UNICODE

static void Convert_WIN32_FIND_DATA_to_FileInfo(const WIN32_FIND_DATA &fd, CFileInfo &fi)
{
  WIN_FD_TO_MY_FI(fi, fd);
  fi.Name = fas2fs(fd.cFileName);
  #if defined(_WIN32) && !defined(UNDER_CE)
  // fi.ShortName = fas2fs(fd.cAlternateFileName);
  #endif
}
#endif
  
////////////////////////////////
// CFindFile

bool CFindFileBase::Close() throw()
{
  if (_handle == INVALID_HANDLE_VALUE)
    return true;
  if (!::FindClose(_handle))
    return false;
  _handle = INVALID_HANDLE_VALUE;
  return true;
}

/*
WinXP-64 FindFirstFile():
  ""      -  ERROR_PATH_NOT_FOUND
  folder\ -  ERROR_FILE_NOT_FOUND
  \       -  ERROR_FILE_NOT_FOUND
  c:\     -  ERROR_FILE_NOT_FOUND
  c:      -  ERROR_FILE_NOT_FOUND, if current dir is ROOT     ( c:\ )
  c:      -  OK,                   if current dir is NOT ROOT ( c:\folder )
  folder  -  OK

  \\               - ERROR_INVALID_NAME
  \\Server         - ERROR_INVALID_NAME
  \\Server\        - ERROR_INVALID_NAME
      
  \\Server\Share            - ERROR_BAD_NETPATH
  \\Server\Share            - ERROR_BAD_NET_NAME (Win7).
             !!! There is problem : Win7 makes some requests for "\\Server\Shar" (look in Procmon),
                 when we call it for "\\Server\Share"
                      
  \\Server\Share\           - ERROR_FILE_NOT_FOUND
  
  \\?\UNC\Server\Share      - ERROR_INVALID_NAME
  \\?\UNC\Server\Share      - ERROR_BAD_PATHNAME (Win7)
  \\?\UNC\Server\Share\     - ERROR_FILE_NOT_FOUND
  
  \\Server\Share_RootDrive  - ERROR_INVALID_NAME
  \\Server\Share_RootDrive\ - ERROR_INVALID_NAME
  
  c:\* - ERROR_FILE_NOT_FOUND, if thare are no item in that folder
*/

bool CFindFile::FindFirst(CFSTR path, CFileInfo &fi)
{
  if (!Close())
    return false;
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    WIN32_FIND_DATAA fd;
    _handle = ::FindFirstFileA(fs2fas(path), &fd);
    if (_handle == INVALID_HANDLE_VALUE)
      return false;
    Convert_WIN32_FIND_DATA_to_FileInfo(fd, fi);
  }
  else
  #endif
  {
    WIN32_FIND_DATAW fd;

    IF_USE_MAIN_PATH
      _handle = ::FindFirstFileW(fs2us(path), &fd);
    #ifdef WIN_LONG_PATH
    if (_handle == INVALID_HANDLE_VALUE && USE_SUPER_PATH)
    {
      UString superPath;
      if (GetSuperPath(path, superPath, USE_MAIN_PATH))
        _handle = ::FindFirstFileW(superPath, &fd);
    }
    #endif
    if (_handle == INVALID_HANDLE_VALUE)
      return false;
    Convert_WIN32_FIND_DATA_to_FileInfo(fd, fi);
  }
  return true;
}

bool CFindFile::FindNext(CFileInfo &fi)
{
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    WIN32_FIND_DATAA fd;
    if (!::FindNextFileA(_handle, &fd))
      return false;
    Convert_WIN32_FIND_DATA_to_FileInfo(fd, fi);
  }
  else
  #endif
  {
    WIN32_FIND_DATAW fd;
    if (!::FindNextFileW(_handle, &fd))
      return false;
    Convert_WIN32_FIND_DATA_to_FileInfo(fd, fi);
  }
  return true;
}

#if defined(_WIN32) && !defined(UNDER_CE)

////////////////////////////////
// AltStreams

static FindFirstStreamW_Ptr g_FindFirstStreamW;
static FindNextStreamW_Ptr g_FindNextStreamW;

struct CFindStreamLoader
{
  CFindStreamLoader()
  {
    g_FindFirstStreamW = (FindFirstStreamW_Ptr)::GetProcAddress(::GetModuleHandleA("kernel32.dll"), "FindFirstStreamW");
    g_FindNextStreamW = (FindNextStreamW_Ptr)::GetProcAddress(::GetModuleHandleA("kernel32.dll"), "FindNextStreamW");
  }
} g_FindStreamLoader;

bool CStreamInfo::IsMainStream() const throw()
{
  return StringsAreEqualNoCase_Ascii(Name, "::$DATA");
};

UString CStreamInfo::GetReducedName() const
{
  // remove ":$DATA" postfix, but keep postfix, if Name is "::$DATA"
  UString s (Name);
  if (s.Len() > 6 + 1 && StringsAreEqualNoCase_Ascii(s.RightPtr(6), ":$DATA"))
    s.DeleteFrom(s.Len() - 6);
  return s;
}

/*
UString CStreamInfo::GetReducedName2() const
{
  UString s = GetReducedName();
  if (!s.IsEmpty() && s[0] == ':')
    s.Delete(0);
  return s;
}
*/

static void Convert_WIN32_FIND_STREAM_DATA_to_StreamInfo(const MY_WIN32_FIND_STREAM_DATA &sd, CStreamInfo &si)
{
  si.Size = sd.StreamSize.QuadPart;
  si.Name = sd.cStreamName;
}

/*
  WinXP-64 FindFirstStream():
  ""      -  ERROR_PATH_NOT_FOUND
  folder\ -  OK
  folder  -  OK
  \       -  OK
  c:\     -  OK
  c:      -  OK, if current dir is ROOT     ( c:\ )
  c:      -  OK, if current dir is NOT ROOT ( c:\folder )
  \\Server\Share   - OK
  \\Server\Share\  - OK

  \\               - ERROR_INVALID_NAME
  \\Server         - ERROR_INVALID_NAME
  \\Server\        - ERROR_INVALID_NAME
*/

bool CFindStream::FindFirst(CFSTR path, CStreamInfo &si)
{
  if (!Close())
    return false;
  if (!g_FindFirstStreamW)
  {
    ::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return false;
  }
  {
    MY_WIN32_FIND_STREAM_DATA sd;
    SetLastError(0);
    IF_USE_MAIN_PATH
      _handle = g_FindFirstStreamW(fs2us(path), My_FindStreamInfoStandard, &sd, 0);
    if (_handle == INVALID_HANDLE_VALUE)
    {
      if (::GetLastError() == ERROR_HANDLE_EOF)
        return false;
      // long name can be tricky for path like ".\dirName".
      #ifdef WIN_LONG_PATH
      if (USE_SUPER_PATH)
      {
        UString superPath;
        if (GetSuperPath(path, superPath, USE_MAIN_PATH))
          _handle = g_FindFirstStreamW(superPath, My_FindStreamInfoStandard, &sd, 0);
      }
      #endif
    }
    if (_handle == INVALID_HANDLE_VALUE)
      return false;
    Convert_WIN32_FIND_STREAM_DATA_to_StreamInfo(sd, si);
  }
  return true;
}

bool CFindStream::FindNext(CStreamInfo &si)
{
  if (!g_FindNextStreamW)
  {
    ::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return false;
  }
  {
    MY_WIN32_FIND_STREAM_DATA sd;
    if (!g_FindNextStreamW(_handle, &sd))
      return false;
    Convert_WIN32_FIND_STREAM_DATA_to_StreamInfo(sd, si);
  }
  return true;
}

bool CStreamEnumerator::Next(CStreamInfo &si, bool &found)
{
  bool res;
  if (_find.IsHandleAllocated())
    res = _find.FindNext(si);
  else
    res = _find.FindFirst(_filePath, si);
  if (res)
  {
    found = true;
    return true;
  }
  found = false;
  return (::GetLastError() == ERROR_HANDLE_EOF);
}

#endif


#define MY_CLEAR_FILETIME(ft) ft.dwLowDateTime = ft.dwHighDateTime = 0;

void CFileInfoBase::ClearBase() throw()
{
  Size = 0;
  MY_CLEAR_FILETIME(CTime);
  MY_CLEAR_FILETIME(ATime);
  MY_CLEAR_FILETIME(MTime);
  Attrib = 0;
  IsAltStream = false;
  IsDevice = false;
}

/*
WinXP-64 GetFileAttributes():
  If the function fails, it returns INVALID_FILE_ATTRIBUTES and use GetLastError() to get error code

  \    - OK
  C:\  - OK, if there is such drive,
  D:\  - ERROR_PATH_NOT_FOUND, if there is no such drive,

  C:\folder     - OK
  C:\folder\    - OK
  C:\folderBad  - ERROR_FILE_NOT_FOUND

  \\Server\BadShare  - ERROR_BAD_NETPATH
  \\Server\Share     - WORKS OK, but MSDN says:
                          GetFileAttributes for a network share, the function fails, and GetLastError
                          returns ERROR_BAD_NETPATH. You must specify a path to a subfolder on that share.
*/

DWORD GetFileAttrib(CFSTR path)
{
  #ifndef _UNICODE
  if (!g_IsNT)
    return ::GetFileAttributes(fs2fas(path));
  else
  #endif
  {
    IF_USE_MAIN_PATH
    {
      DWORD dw = ::GetFileAttributesW(fs2us(path));
      if (dw != INVALID_FILE_ATTRIBUTES)
        return dw;
    }
    #ifdef WIN_LONG_PATH
    if (USE_SUPER_PATH)
    {
      UString superPath;
      if (GetSuperPath(path, superPath, USE_MAIN_PATH))
        return ::GetFileAttributesW(superPath);
    }
    #endif
    return INVALID_FILE_ATTRIBUTES;
  }
}

/* if path is "c:" or "c::" then CFileInfo::Find() returns name of current folder for that disk
   so instead of absolute path we have relative path in Name. That is not good in some calls */

/* In CFileInfo::Find() we want to support same names for alt streams as in CreateFile(). */

/* CFileInfo::Find()
We alow the following paths (as FindFirstFile):
  C:\folder
  c:                      - if current dir is NOT ROOT ( c:\folder )

also we support paths that are not supported by FindFirstFile:
  \
  \\.\c:
  c:\                     - Name will be without tail slash ( c: )
  \\?\c:\                 - Name will be without tail slash ( c: )
  \\Server\Share
  \\?\UNC\Server\Share

  c:\folder:stream  - Name = folder:stream
  c:\:stream        - Name = :stream
  c::stream         - Name = c::stream
*/

bool CFileInfo::Find(CFSTR path)
{
  #ifdef SUPPORT_DEVICE_FILE
  if (IsDevicePath(path))
  {
    ClearBase();
    Name = path + 4;
    IsDevice = true;
    
    if (NName::IsDrivePath2(path + 4) && path[6] == 0)
    {
      FChar drive[4] = { path[4], ':', '\\', 0 };
      UInt64 clusterSize, totalSize, freeSize;
      if (NSystem::MyGetDiskFreeSpace(drive, clusterSize, totalSize, freeSize))
      {
        Size = totalSize;
        return true;
      }
    }

    NIO::CInFile inFile;
    // ::OutputDebugStringW(path);
    if (!inFile.Open(path))
      return false;
    // ::OutputDebugStringW(L"---");
    if (inFile.SizeDefined)
      Size = inFile.Size;
    return true;
  }
  #endif

  #if defined(_WIN32) && !defined(UNDER_CE)

  int colonPos = FindAltStreamColon(path);
  if (colonPos >= 0 && path[(unsigned)colonPos + 1] != 0)
  {
    UString streamName = fs2us(path + (unsigned)colonPos);
    FString filePath (path);
    filePath.DeleteFrom(colonPos);
    /* we allow both cases:
      name:stream
      name:stream:$DATA
    */
    const unsigned kPostfixSize = 6;
    if (streamName.Len() <= kPostfixSize
        || !StringsAreEqualNoCase_Ascii(streamName.RightPtr(kPostfixSize), ":$DATA"))
      streamName += ":$DATA";

    bool isOk = true;
    
    if (IsDrivePath2(filePath) &&
        (colonPos == 2 || colonPos == 3 && filePath[2] == '\\'))
    {
      // FindFirstFile doesn't work for "c:\" and for "c:" (if current dir is ROOT)
      ClearBase();
      Name.Empty();
      if (colonPos == 2)
        Name = filePath;
    }
    else
      isOk = Find(filePath);

    if (isOk)
    {
      Attrib &= ~(FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT);
      Size = 0;
      CStreamEnumerator enumerator(filePath);
      for (;;)
      {
        CStreamInfo si;
        bool found;
        if (!enumerator.Next(si, found))
          return false;
        if (!found)
        {
          ::SetLastError(ERROR_FILE_NOT_FOUND);
          return false;
        }
        if (si.Name.IsEqualTo_NoCase(streamName))
        {
          // we delete postfix, if alt stream name is not "::$DATA"
          if (si.Name.Len() > kPostfixSize + 1)
            si.Name.DeleteFrom(si.Name.Len() - kPostfixSize);
          Name += us2fs(si.Name);
          Size = si.Size;
          IsAltStream = true;
          return true;
        }
      }
    }
  }
  
  #endif

  CFindFile finder;

  #if defined(_WIN32) && !defined(UNDER_CE)
  {
    /*
    DWORD lastError = GetLastError();
    if (lastError == ERROR_FILE_NOT_FOUND
        || lastError == ERROR_BAD_NETPATH  // XP64: "\\Server\Share"
        || lastError == ERROR_BAD_NET_NAME // Win7: "\\Server\Share"
        || lastError == ERROR_INVALID_NAME // XP64: "\\?\UNC\Server\Share"
        || lastError == ERROR_BAD_PATHNAME // Win7: "\\?\UNC\Server\Share"
        )
    */
    
    unsigned rootSize = 0;
    if (IsSuperPath(path))
      rootSize = kSuperPathPrefixSize;
    
    if (NName::IsDrivePath(path + rootSize) && path[rootSize + 3] == 0)
    {
      DWORD attrib = GetFileAttrib(path);
      if (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY) != 0)
      {
        ClearBase();
        Attrib = attrib;
        Name = path + rootSize;
        Name.DeleteFrom(2); // we don't need backslash (C:)
        return true;
      }
    }
    else if (IS_PATH_SEPAR(path[0]))
      if (path[1] == 0)
      {
        DWORD attrib = GetFileAttrib(path);
        if (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY) != 0)
        {
          ClearBase();
          Name.Empty();
          Attrib = attrib;
          return true;
        }
      }
      else
      {
        const unsigned prefixSize = GetNetworkServerPrefixSize(path);
        if (prefixSize > 0 && path[prefixSize] != 0)
        {
          if (NName::FindSepar(path + prefixSize) < 0)
          {
            FString s (path);
            s.Add_PathSepar();
            s += '*'; // CHAR_ANY_MASK
            
            bool isOK = false;
            if (finder.FindFirst(s, *this))
            {
              if (Name == FTEXT("."))
              {
                Name = path + prefixSize;
                return true;
              }
              isOK = true;
              /* if "\\server\share" maps to root folder "d:\", there is no "." item.
                 But it's possible that there are another items */
            }
            {
              DWORD attrib = GetFileAttrib(path);
              if (isOK || attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY) != 0)
              {
                ClearBase();
                if (attrib != INVALID_FILE_ATTRIBUTES)
                  Attrib = attrib;
                else
                  SetAsDir();
                Name = path + prefixSize;
                return true;
              }
            }
            // ::SetLastError(lastError);
          }
        }
      }
  }
  #endif

  return finder.FindFirst(path, *this);
}


bool DoesFileExist(CFSTR name)
{
  CFileInfo fi;
  return fi.Find(name) && !fi.IsDir();
}

bool DoesDirExist(CFSTR name)
{
  CFileInfo fi;
  return fi.Find(name) && fi.IsDir();
}

bool DoesFileOrDirExist(CFSTR name)
{
  CFileInfo fi;
  return fi.Find(name);
}


void CEnumerator::SetDirPrefix(const FString &dirPrefix)
{
  _wildcard = dirPrefix;
  _wildcard += '*';
}

bool CEnumerator::NextAny(CFileInfo &fi)
{
  if (_findFile.IsHandleAllocated())
    return _findFile.FindNext(fi);
  else
    return _findFile.FindFirst(_wildcard, fi);
}

bool CEnumerator::Next(CFileInfo &fi)
{
  for (;;)
  {
    if (!NextAny(fi))
      return false;
    if (!fi.IsDots())
      return true;
  }
}

bool CEnumerator::Next(CFileInfo &fi, bool &found)
{
  if (Next(fi))
  {
    found = true;
    return true;
  }
  found = false;
  return (::GetLastError() == ERROR_NO_MORE_FILES);
}

////////////////////////////////
// CFindChangeNotification
// FindFirstChangeNotification can return 0. MSDN doesn't tell about it.

bool CFindChangeNotification::Close() throw()
{
  if (!IsHandleAllocated())
    return true;
  if (!::FindCloseChangeNotification(_handle))
    return false;
  _handle = INVALID_HANDLE_VALUE;
  return true;
}
           
HANDLE CFindChangeNotification::FindFirst(CFSTR path, bool watchSubtree, DWORD notifyFilter)
{
  #ifndef _UNICODE
  if (!g_IsNT)
    _handle = ::FindFirstChangeNotification(fs2fas(path), BoolToBOOL(watchSubtree), notifyFilter);
  else
  #endif
  {
    IF_USE_MAIN_PATH
    _handle = ::FindFirstChangeNotificationW(fs2us(path), BoolToBOOL(watchSubtree), notifyFilter);
    #ifdef WIN_LONG_PATH
    if (!IsHandleAllocated())
    {
      UString superPath;
      if (GetSuperPath(path, superPath, USE_MAIN_PATH))
        _handle = ::FindFirstChangeNotificationW(superPath, BoolToBOOL(watchSubtree), notifyFilter);
    }
    #endif
  }
  return _handle;
}

#ifndef UNDER_CE

bool MyGetLogicalDriveStrings(CObjectVector<FString> &driveStrings)
{
  driveStrings.Clear();
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    driveStrings.Clear();
    UINT32 size = GetLogicalDriveStrings(0, NULL);
    if (size == 0)
      return false;
    CObjArray<char> buf(size);
    UINT32 newSize = GetLogicalDriveStrings(size, buf);
    if (newSize == 0 || newSize > size)
      return false;
    AString s;
    UINT32 prev = 0;
    for (UINT32 i = 0; i < newSize; i++)
    {
      if (buf[i] == 0)
      {
        s = buf + prev;
        prev = i + 1;
        driveStrings.Add(fas2fs(s));
      }
    }
    return prev == newSize;
  }
  else
  #endif
  {
    UINT32 size = GetLogicalDriveStringsW(0, NULL);
    if (size == 0)
      return false;
    CObjArray<wchar_t> buf(size);
    UINT32 newSize = GetLogicalDriveStringsW(size, buf);
    if (newSize == 0 || newSize > size)
      return false;
    UString s;
    UINT32 prev = 0;
    for (UINT32 i = 0; i < newSize; i++)
    {
      if (buf[i] == 0)
      {
        s = buf + prev;
        prev = i + 1;
        driveStrings.Add(us2fs(s));
      }
    }
    return prev == newSize;
  }
}

#endif

}}}
