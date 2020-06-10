// Windows/FileDir.cpp

#include "StdAfx.h"

#ifndef _UNICODE
#include "../Common/StringConvert.h"
#endif

#include "FileDir.h"
#include "FileFind.h"
#include "FileName.h"

#ifndef _UNICODE
extern bool g_IsNT;
#endif

using namespace NWindows;
using namespace NFile;
using namespace NName;

namespace NWindows {
namespace NFile {
namespace NDir {

#ifndef UNDER_CE

bool GetWindowsDir(FString &path)
{
  UINT needLength;
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    TCHAR s[MAX_PATH + 2];
    s[0] = 0;
    needLength = ::GetWindowsDirectory(s, MAX_PATH + 1);
    path = fas2fs(s);
  }
  else
  #endif
  {
    WCHAR s[MAX_PATH + 2];
    s[0] = 0;
    needLength = ::GetWindowsDirectoryW(s, MAX_PATH + 1);
    path = us2fs(s);
  }
  return (needLength > 0 && needLength <= MAX_PATH);
}

bool GetSystemDir(FString &path)
{
  UINT needLength;
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    TCHAR s[MAX_PATH + 2];
    s[0] = 0;
    needLength = ::GetSystemDirectory(s, MAX_PATH + 1);
    path = fas2fs(s);
  }
  else
  #endif
  {
    WCHAR s[MAX_PATH + 2];
    s[0] = 0;
    needLength = ::GetSystemDirectoryW(s, MAX_PATH + 1);
    path = us2fs(s);
  }
  return (needLength > 0 && needLength <= MAX_PATH);
}
#endif

bool SetDirTime(CFSTR path, const FILETIME *cTime, const FILETIME *aTime, const FILETIME *mTime)
{
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    ::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return false;
  }
  #endif
  
  HANDLE hDir = INVALID_HANDLE_VALUE;
  IF_USE_MAIN_PATH
    hDir = ::CreateFileW(fs2us(path), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  #ifdef WIN_LONG_PATH
  if (hDir == INVALID_HANDLE_VALUE && USE_SUPER_PATH)
  {
    UString superPath;
    if (GetSuperPath(path, superPath, USE_MAIN_PATH))
      hDir = ::CreateFileW(superPath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
          NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  }
  #endif

  bool res = false;
  if (hDir != INVALID_HANDLE_VALUE)
  {
    res = BOOLToBool(::SetFileTime(hDir, cTime, aTime, mTime));
    ::CloseHandle(hDir);
  }
  return res;
}

bool SetFileAttrib(CFSTR path, DWORD attrib)
{
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    if (::SetFileAttributes(fs2fas(path), attrib))
      return true;
  }
  else
  #endif
  {
    IF_USE_MAIN_PATH
      if (::SetFileAttributesW(fs2us(path), attrib))
        return true;
    #ifdef WIN_LONG_PATH
    if (USE_SUPER_PATH)
    {
      UString superPath;
      if (GetSuperPath(path, superPath, USE_MAIN_PATH))
        return BOOLToBool(::SetFileAttributesW(superPath, attrib));
    }
    #endif
  }
  return false;
}


bool SetFileAttrib_PosixHighDetect(CFSTR path, DWORD attrib)
{
  if ((attrib & 0xF0000000) != 0)
    attrib &= 0x3FFF;
  return SetFileAttrib(path, attrib);
}


bool RemoveDir(CFSTR path)
{
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    if (::RemoveDirectory(fs2fas(path)))
      return true;
  }
  else
  #endif
  {
    IF_USE_MAIN_PATH
      if (::RemoveDirectoryW(fs2us(path)))
        return true;
    #ifdef WIN_LONG_PATH
    if (USE_SUPER_PATH)
    {
      UString superPath;
      if (GetSuperPath(path, superPath, USE_MAIN_PATH))
        return BOOLToBool(::RemoveDirectoryW(superPath));
    }
    #endif
  }
  return false;
}

bool MyMoveFile(CFSTR oldFile, CFSTR newFile)
{
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    if (::MoveFile(fs2fas(oldFile), fs2fas(newFile)))
      return true;
  }
  else
  #endif
  {
    IF_USE_MAIN_PATH_2(oldFile, newFile)
      if (::MoveFileW(fs2us(oldFile), fs2us(newFile)))
        return true;
    #ifdef WIN_LONG_PATH
    if (USE_SUPER_PATH_2)
    {
      UString d1, d2;
      if (GetSuperPaths(oldFile, newFile, d1, d2, USE_MAIN_PATH_2))
        return BOOLToBool(::MoveFileW(d1, d2));
    }
    #endif
  }
  return false;
}

#ifndef UNDER_CE

EXTERN_C_BEGIN
typedef BOOL (WINAPI *Func_CreateHardLinkW)(
    LPCWSTR lpFileName,
    LPCWSTR lpExistingFileName,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes
    );
EXTERN_C_END

bool MyCreateHardLink(CFSTR newFileName, CFSTR existFileName)
{
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return false;
    /*
    if (::CreateHardLink(fs2fas(newFileName), fs2fas(existFileName), NULL))
      return true;
    */
  }
  else
  #endif
  {
    Func_CreateHardLinkW my_CreateHardLinkW = (Func_CreateHardLinkW)
        ::GetProcAddress(::GetModuleHandleW(L"kernel32.dll"), "CreateHardLinkW");
    if (!my_CreateHardLinkW)
      return false;
    IF_USE_MAIN_PATH_2(newFileName, existFileName)
      if (my_CreateHardLinkW(fs2us(newFileName), fs2us(existFileName), NULL))
        return true;
    #ifdef WIN_LONG_PATH
    if (USE_SUPER_PATH_2)
    {
      UString d1, d2;
      if (GetSuperPaths(newFileName, existFileName, d1, d2, USE_MAIN_PATH_2))
        return BOOLToBool(my_CreateHardLinkW(d1, d2, NULL));
    }
    #endif
  }
  return false;
}

#endif

/*
WinXP-64 CreateDir():
  ""                  - ERROR_PATH_NOT_FOUND
  \                   - ERROR_ACCESS_DENIED
  C:\                 - ERROR_ACCESS_DENIED, if there is such drive,
  
  D:\folder             - ERROR_PATH_NOT_FOUND, if there is no such drive,
  C:\nonExistent\folder - ERROR_PATH_NOT_FOUND
  
  C:\existFolder      - ERROR_ALREADY_EXISTS
  C:\existFolder\     - ERROR_ALREADY_EXISTS

  C:\folder   - OK
  C:\folder\  - OK

  \\Server\nonExistent    - ERROR_BAD_NETPATH
  \\Server\Share_Readonly - ERROR_ACCESS_DENIED
  \\Server\Share          - ERROR_ALREADY_EXISTS

  \\Server\Share_NTFS_drive - ERROR_ACCESS_DENIED
  \\Server\Share_FAT_drive  - ERROR_ALREADY_EXISTS
*/

bool CreateDir(CFSTR path)
{
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    if (::CreateDirectory(fs2fas(path), NULL))
      return true;
  }
  else
  #endif
  {
    IF_USE_MAIN_PATH
      if (::CreateDirectoryW(fs2us(path), NULL))
        return true;
    #ifdef WIN_LONG_PATH
    if ((!USE_MAIN_PATH || ::GetLastError() != ERROR_ALREADY_EXISTS) && USE_SUPER_PATH)
    {
      UString superPath;
      if (GetSuperPath(path, superPath, USE_MAIN_PATH))
        return BOOLToBool(::CreateDirectoryW(superPath, NULL));
    }
    #endif
  }
  return false;
}

/*
  CreateDir2 returns true, if directory can contain files after the call (two cases):
    1) the directory already exists
    2) the directory was created
  path must be WITHOUT trailing path separator.

  We need CreateDir2, since fileInfo.Find() for reserved names like "com8"
   returns FILE instead of DIRECTORY. And we need to use SuperPath */
 
static bool CreateDir2(CFSTR path)
{
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    if (::CreateDirectory(fs2fas(path), NULL))
      return true;
  }
  else
  #endif
  {
    IF_USE_MAIN_PATH
      if (::CreateDirectoryW(fs2us(path), NULL))
        return true;
    #ifdef WIN_LONG_PATH
    if ((!USE_MAIN_PATH || ::GetLastError() != ERROR_ALREADY_EXISTS) && USE_SUPER_PATH)
    {
      UString superPath;
      if (GetSuperPath(path, superPath, USE_MAIN_PATH))
      {
        if (::CreateDirectoryW(superPath, NULL))
          return true;
        if (::GetLastError() != ERROR_ALREADY_EXISTS)
          return false;
        NFind::CFileInfo fi;
        if (!fi.Find(us2fs(superPath)))
          return false;
        return fi.IsDir();
      }
    }
    #endif
  }
  if (::GetLastError() != ERROR_ALREADY_EXISTS)
    return false;
  NFind::CFileInfo fi;
  if (!fi.Find(path))
    return false;
  return fi.IsDir();
}

bool CreateComplexDir(CFSTR _path)
{
  #ifdef _WIN32
  
  {
    DWORD attrib = NFind::GetFileAttrib(_path);
    if (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY) != 0)
      return true;
  }

  #ifndef UNDER_CE
  
  if (IsDriveRootPath_SuperAllowed(_path))
    return false;
  
  unsigned prefixSize = GetRootPrefixSize(_path);
  
  #endif
  
  #endif

  FString path (_path);

  int pos = path.ReverseFind_PathSepar();
  if (pos >= 0 && (unsigned)pos == path.Len() - 1)
  {
    if (path.Len() == 1)
      return true;
    path.DeleteBack();
  }

  const FString path2 (path);
  pos = path.Len();
  
  for (;;)
  {
    if (CreateDir2(path))
      break;
    if (::GetLastError() == ERROR_ALREADY_EXISTS)
      return false;
    pos = path.ReverseFind_PathSepar();
    if (pos < 0 || pos == 0)
      return false;
    
    #if defined(_WIN32) && !defined(UNDER_CE)
    if (pos == 1 && IS_PATH_SEPAR(path[0]))
      return false;
    if (prefixSize >= (unsigned)pos + 1)
      return false;
    #endif
    
    path.DeleteFrom(pos);
  }
  
  while (pos < (int)path2.Len())
  {
    int pos2 = NName::FindSepar(path2.Ptr(pos + 1));
    if (pos2 < 0)
      pos = path2.Len();
    else
      pos += 1 + pos2;
    path.SetFrom(path2, pos);
    if (!CreateDir(path))
      return false;
  }
  
  return true;
}

bool DeleteFileAlways(CFSTR path)
{
  /* If alt stream, we also need to clear READ-ONLY attribute of main file before delete.
     SetFileAttrib("name:stream", ) changes attributes of main file. */
  {
    DWORD attrib = NFind::GetFileAttrib(path);
    if (attrib != INVALID_FILE_ATTRIBUTES
        && (attrib & FILE_ATTRIBUTE_DIRECTORY) == 0
        && (attrib & FILE_ATTRIBUTE_READONLY) != 0)
    {
      if (!SetFileAttrib(path, attrib & ~FILE_ATTRIBUTE_READONLY))
        return false;
    }
  }

  #ifndef _UNICODE
  if (!g_IsNT)
  {
    if (::DeleteFile(fs2fas(path)))
      return true;
  }
  else
  #endif
  {
    /* DeleteFile("name::$DATA") deletes all alt streams (same as delete DeleteFile("name")).
       Maybe it's better to open "name::$DATA" and clear data for unnamed stream? */
    IF_USE_MAIN_PATH
      if (::DeleteFileW(fs2us(path)))
        return true;
    #ifdef WIN_LONG_PATH
    if (USE_SUPER_PATH)
    {
      UString superPath;
      if (GetSuperPath(path, superPath, USE_MAIN_PATH))
        return BOOLToBool(::DeleteFileW(superPath));
    }
    #endif
  }
  return false;
}

bool RemoveDirWithSubItems(const FString &path)
{
  bool needRemoveSubItems = true;
  {
    NFind::CFileInfo fi;
    if (!fi.Find(path))
      return false;
    if (!fi.IsDir())
    {
      ::SetLastError(ERROR_DIRECTORY);
      return false;
    }
    if (fi.HasReparsePoint())
      needRemoveSubItems = false;
  }

  if (needRemoveSubItems)
  {
    FString s (path);
    s.Add_PathSepar();
    const unsigned prefixSize = s.Len();
    NFind::CEnumerator enumerator;
    enumerator.SetDirPrefix(s);
    NFind::CFileInfo fi;
    while (enumerator.Next(fi))
    {
      s.DeleteFrom(prefixSize);
      s += fi.Name;
      if (fi.IsDir())
      {
        if (!RemoveDirWithSubItems(s))
          return false;
      }
      else if (!DeleteFileAlways(s))
        return false;
    }
  }
  
  if (!SetFileAttrib(path, 0))
    return false;
  return RemoveDir(path);
}

#ifdef UNDER_CE

bool MyGetFullPathName(CFSTR path, FString &resFullPath)
{
  resFullPath = path;
  return true;
}

#else

bool MyGetFullPathName(CFSTR path, FString &resFullPath)
{
  return GetFullPath(path, resFullPath);
}

bool SetCurrentDir(CFSTR path)
{
  // SetCurrentDirectory doesn't support \\?\ prefix
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    return BOOLToBool(::SetCurrentDirectory(fs2fas(path)));
  }
  else
  #endif
  {
    return BOOLToBool(::SetCurrentDirectoryW(fs2us(path)));
  }
}

bool GetCurrentDir(FString &path)
{
  path.Empty();
  DWORD needLength;
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    TCHAR s[MAX_PATH + 2];
    s[0] = 0;
    needLength = ::GetCurrentDirectory(MAX_PATH + 1, s);
    path = fas2fs(s);
  }
  else
  #endif
  {
    WCHAR s[MAX_PATH + 2];
    s[0] = 0;
    needLength = ::GetCurrentDirectoryW(MAX_PATH + 1, s);
    path = us2fs(s);
  }
  return (needLength > 0 && needLength <= MAX_PATH);
}

#endif

bool GetFullPathAndSplit(CFSTR path, FString &resDirPrefix, FString &resFileName)
{
  bool res = MyGetFullPathName(path, resDirPrefix);
  if (!res)
    resDirPrefix = path;
  int pos = resDirPrefix.ReverseFind_PathSepar();
  resFileName = resDirPrefix.Ptr(pos + 1);
  resDirPrefix.DeleteFrom(pos + 1);
  return res;
}

bool GetOnlyDirPrefix(CFSTR path, FString &resDirPrefix)
{
  FString resFileName;
  return GetFullPathAndSplit(path, resDirPrefix, resFileName);
}

bool MyGetTempPath(FString &path)
{
  path.Empty();
  DWORD needLength;
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    TCHAR s[MAX_PATH + 2];
    s[0] = 0;
    needLength = ::GetTempPath(MAX_PATH + 1, s);
    path = fas2fs(s);
  }
  else
  #endif
  {
    WCHAR s[MAX_PATH + 2];
    s[0] = 0;
    needLength = ::GetTempPathW(MAX_PATH + 1, s);;
    path = us2fs(s);
  }
  return (needLength > 0 && needLength <= MAX_PATH);
}

static bool CreateTempFile(CFSTR prefix, bool addRandom, FString &path, NIO::COutFile *outFile)
{
  UInt32 d = (GetTickCount() << 12) ^ (GetCurrentThreadId() << 14) ^ GetCurrentProcessId();
  for (unsigned i = 0; i < 100; i++)
  {
    path = prefix;
    if (addRandom)
    {
      char s[16];
      UInt32 val = d;
      unsigned k;
      for (k = 0; k < 8; k++)
      {
        unsigned t = val & 0xF;
        val >>= 4;
        s[k] = (char)((t < 10) ? ('0' + t) : ('A' + (t - 10)));
      }
      s[k] = '\0';
      if (outFile)
        path += '.';
      path += s;
      UInt32 step = GetTickCount() + 2;
      if (step == 0)
        step = 1;
      d += step;
    }
    addRandom = true;
    if (outFile)
      path += ".tmp";
    if (NFind::DoesFileOrDirExist(path))
    {
      SetLastError(ERROR_ALREADY_EXISTS);
      continue;
    }
    if (outFile)
    {
      if (outFile->Create(path, false))
        return true;
    }
    else
    {
      if (CreateDir(path))
        return true;
    }
    DWORD error = GetLastError();
    if (error != ERROR_FILE_EXISTS &&
        error != ERROR_ALREADY_EXISTS)
      break;
  }
  path.Empty();
  return false;
}

bool CTempFile::Create(CFSTR prefix, NIO::COutFile *outFile)
{
  if (!Remove())
    return false;
  if (!CreateTempFile(prefix, false, _path, outFile))
    return false;
  _mustBeDeleted = true;
  return true;
}

bool CTempFile::CreateRandomInTempFolder(CFSTR namePrefix, NIO::COutFile *outFile)
{
  if (!Remove())
    return false;
  FString tempPath;
  if (!MyGetTempPath(tempPath))
    return false;
  if (!CreateTempFile(tempPath + namePrefix, true, _path, outFile))
    return false;
  _mustBeDeleted = true;
  return true;
}

bool CTempFile::Remove()
{
  if (!_mustBeDeleted)
    return true;
  _mustBeDeleted = !DeleteFileAlways(_path);
  return !_mustBeDeleted;
}

bool CTempFile::MoveTo(CFSTR name, bool deleteDestBefore)
{
  // DWORD attrib = 0;
  if (deleteDestBefore)
  {
    if (NFind::DoesFileExist(name))
    {
      // attrib = NFind::GetFileAttrib(name);
      if (!DeleteFileAlways(name))
        return false;
    }
  }
  DisableDeleting();
  return MyMoveFile(_path, name);
  
  /*
  if (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_READONLY))
  {
    DWORD attrib2 = NFind::GetFileAttrib(name);
    if (attrib2 != INVALID_FILE_ATTRIBUTES)
      SetFileAttrib(name, attrib2 | FILE_ATTRIBUTE_READONLY);
  }
  */
}

bool CTempDir::Create(CFSTR prefix)
{
  if (!Remove())
    return false;
  FString tempPath;
  if (!MyGetTempPath(tempPath))
    return false;
  if (!CreateTempFile(tempPath + prefix, true, _path, NULL))
    return false;
  _mustBeDeleted = true;
  return true;
}

bool CTempDir::Remove()
{
  if (!_mustBeDeleted)
    return true;
  _mustBeDeleted = !RemoveDirWithSubItems(_path);
  return !_mustBeDeleted;
}

}}}
