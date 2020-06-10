// ArchiveExtractCallback.cpp

#include "StdAfx.h"

#undef sprintf
#undef printf

// #include <stdio.h>
// #include "../../../../C/CpuTicks.h"

#include "../../../../C/Alloc.h"
#include "../../../../C/CpuArch.h"


#include "../../../Common/ComTry.h"
#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"
#include "../../../Common/Wildcard.h"

#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileFind.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/PropVariant.h"
#include "../../../Windows/PropVariantConv.h"

#if defined(_WIN32) && !defined(UNDER_CE)  && !defined(_SFX)
#define _USE_SECURITY_CODE
#include "../../../Windows/SecurityUtils.h"
#endif

#include "../../Common/FilePathAutoRename.h"
// #include "../../Common/StreamUtils.h"

#include "../Common/ExtractingFilePath.h"
#include "../Common/PropIDUtils.h"

#include "ArchiveExtractCallback.h"

using namespace NWindows;
using namespace NFile;
using namespace NDir;

static const char * const kCantAutoRename = "Can not create file with auto name";
static const char * const kCantRenameFile = "Can not rename existing file";
static const char * const kCantDeleteOutputFile = "Can not delete output file";
static const char * const kCantDeleteOutputDir = "Can not delete output folder";
static const char * const kCantCreateHardLink = "Can not create hard link";
static const char * const kCantCreateSymLink = "Can not create symbolic link";
static const char * const kCantOpenOutFile = "Can not open output file";
static const char * const kCantSetFileLen = "Can not set length for output file";


#ifndef _SFX

STDMETHODIMP COutStreamWithHash::Write(const void *data, UInt32 size, UInt32 *processedSize)
{
  HRESULT result = S_OK;
  if (_stream)
    result = _stream->Write(data, size, &size);
  if (_calculate)
    _hash->Update(data, size);
  _size += size;
  if (processedSize)
    *processedSize = size;
  return result;
}

#endif

#ifdef _USE_SECURITY_CODE
bool InitLocalPrivileges()
{
  NSecurity::CAccessToken token;
  if (!token.OpenProcessToken(GetCurrentProcess(),
      TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES))
    return false;

  TOKEN_PRIVILEGES tp;
 
  tp.PrivilegeCount = 1;
  tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
  
  if  (!::LookupPrivilegeValue(NULL, SE_SECURITY_NAME, &tp.Privileges[0].Luid))
    return false;
  if (!token.AdjustPrivileges(&tp))
    return false;
  return (GetLastError() == ERROR_SUCCESS);
}
#endif

#ifdef SUPPORT_LINKS

int CHardLinkNode::Compare(const CHardLinkNode &a) const
{
  if (StreamId < a.StreamId) return -1;
  if (StreamId > a.StreamId) return 1;
  return MyCompare(INode, a.INode);
}

static HRESULT Archive_Get_HardLinkNode(IInArchive *archive, UInt32 index, CHardLinkNode &h, bool &defined)
{
  h.INode = 0;
  h.StreamId = (UInt64)(Int64)-1;
  defined = false;
  {
    NCOM::CPropVariant prop;
    RINOK(archive->GetProperty(index, kpidINode, &prop));
    if (!ConvertPropVariantToUInt64(prop, h.INode))
      return S_OK;
  }
  {
    NCOM::CPropVariant prop;
    RINOK(archive->GetProperty(index, kpidStreamId, &prop));
    ConvertPropVariantToUInt64(prop, h.StreamId);
  }
  defined = true;
  return S_OK;
}


HRESULT CArchiveExtractCallback::PrepareHardLinks(const CRecordVector<UInt32> *realIndices)
{
  _hardLinks.Clear();

  if (!_arc->Ask_INode)
    return S_OK;
  
  IInArchive *archive = _arc->Archive;
  CRecordVector<CHardLinkNode> &hardIDs = _hardLinks.IDs;

  {
    UInt32 numItems;
    if (realIndices)
      numItems = realIndices->Size();
    else
    {
      RINOK(archive->GetNumberOfItems(&numItems));
    }

    for (UInt32 i = 0; i < numItems; i++)
    {
      CHardLinkNode h;
      bool defined;
      UInt32 realIndex = realIndices ? (*realIndices)[i] : i;

      RINOK(Archive_Get_HardLinkNode(archive, realIndex, h, defined));
      if (defined)
      {
        bool isAltStream = false;
        RINOK(Archive_IsItem_AltStream(archive, realIndex, isAltStream));
        if (!isAltStream)
          hardIDs.Add(h);
      }
    }
  }
  
  hardIDs.Sort2();
  
  {
    // wee keep only items that have 2 or more items
    unsigned k = 0;
    unsigned numSame = 1;
    for (unsigned i = 1; i < hardIDs.Size(); i++)
    {
      if (hardIDs[i].Compare(hardIDs[i - 1]) != 0)
        numSame = 1;
      else if (++numSame == 2)
      {
        if (i - 1 != k)
          hardIDs[k] = hardIDs[i - 1];
        k++;
      }
    }
    hardIDs.DeleteFrom(k);
  }
  
  _hardLinks.PrepareLinks();
  return S_OK;
}

#endif

CArchiveExtractCallback::CArchiveExtractCallback():
    _arc(NULL),
    WriteCTime(true),
    WriteATime(true),
    WriteMTime(true),
    _multiArchives(false)
{
  LocalProgressSpec = new CLocalProgress();
  _localProgress = LocalProgressSpec;

  #ifdef _USE_SECURITY_CODE
  _saclEnabled = InitLocalPrivileges();
  #endif
}

void CArchiveExtractCallback::Init(
    const CExtractNtOptions &ntOptions,
    const NWildcard::CCensorNode *wildcardCensor,
    const CArc *arc,
    IFolderArchiveExtractCallback *extractCallback2,
    bool stdOutMode, bool testMode,
    const FString &directoryPath,
    const UStringVector &removePathParts, bool removePartsForAltStreams,
    UInt64 packSize)
{
  ClearExtractedDirsInfo();
  _outFileStream.Release();
  
  #ifdef SUPPORT_LINKS
  _hardLinks.Clear();
  #endif

  #ifdef SUPPORT_ALT_STREAMS
  _renamedFiles.Clear();
  #endif

  _ntOptions = ntOptions;
  _wildcardCensor = wildcardCensor;

  _stdOutMode = stdOutMode;
  _testMode = testMode;
  
  // _progressTotal = 0;
  // _progressTotal_Defined = false;
  
  _packTotal = packSize;
  _progressTotal = packSize;
  _progressTotal_Defined = true;

  _extractCallback2 = extractCallback2;
  _compressProgress.Release();
  _extractCallback2.QueryInterface(IID_ICompressProgressInfo, &_compressProgress);
  _extractCallback2.QueryInterface(IID_IArchiveExtractCallbackMessage, &_callbackMessage);
  _extractCallback2.QueryInterface(IID_IFolderArchiveExtractCallback2, &_folderArchiveExtractCallback2);

  #ifndef _SFX

  _extractCallback2.QueryInterface(IID_IFolderExtractToStreamCallback, &ExtractToStreamCallback);
  if (ExtractToStreamCallback)
  {
    Int32 useStreams = 0;
    if (ExtractToStreamCallback->UseExtractToStream(&useStreams) != S_OK)
      useStreams = 0;
    if (useStreams == 0)
      ExtractToStreamCallback.Release();
  }
  
  #endif

  LocalProgressSpec->Init(extractCallback2, true);
  LocalProgressSpec->SendProgress = false;
 
  _removePathParts = removePathParts;
  _removePartsForAltStreams = removePartsForAltStreams;

  #ifndef _SFX
  _baseParentFolder = (UInt32)(Int32)-1;
  _use_baseParentFolder_mode = false;
  #endif

  _arc = arc;
  _dirPathPrefix = directoryPath;
  _dirPathPrefix_Full = directoryPath;
  #if defined(_WIN32) && !defined(UNDER_CE)
  if (!NName::IsAltPathPrefix(_dirPathPrefix))
  #endif
  {
    NName::NormalizeDirPathPrefix(_dirPathPrefix);
    NDir::MyGetFullPathName(directoryPath, _dirPathPrefix_Full);
    NName::NormalizeDirPathPrefix(_dirPathPrefix_Full);
  }
}

STDMETHODIMP CArchiveExtractCallback::SetTotal(UInt64 size)
{
  COM_TRY_BEGIN
  _progressTotal = size;
  _progressTotal_Defined = true;
  if (!_multiArchives && _extractCallback2)
    return _extractCallback2->SetTotal(size);
  return S_OK;
  COM_TRY_END
}

static void NormalizeVals(UInt64 &v1, UInt64 &v2)
{
  const UInt64 kMax = (UInt64)1 << 31;
  while (v1 > kMax)
  {
    v1 >>= 1;
    v2 >>= 1;
  }
}

static UInt64 MyMultDiv64(UInt64 unpCur, UInt64 unpTotal, UInt64 packTotal)
{
  NormalizeVals(packTotal, unpTotal);
  NormalizeVals(unpCur, unpTotal);
  if (unpTotal == 0)
    unpTotal = 1;
  return unpCur * packTotal / unpTotal;
}

STDMETHODIMP CArchiveExtractCallback::SetCompleted(const UInt64 *completeValue)
{
  COM_TRY_BEGIN
  
  if (!_extractCallback2)
    return S_OK;

  UInt64 packCur;
  if (_multiArchives)
  {
    packCur = LocalProgressSpec->InSize;
    if (completeValue && _progressTotal_Defined)
      packCur += MyMultDiv64(*completeValue, _progressTotal, _packTotal);
    completeValue = &packCur;
  }
  return _extractCallback2->SetCompleted(completeValue);
 
  COM_TRY_END
}

STDMETHODIMP CArchiveExtractCallback::SetRatioInfo(const UInt64 *inSize, const UInt64 *outSize)
{
  COM_TRY_BEGIN
  return _localProgress->SetRatioInfo(inSize, outSize);
  COM_TRY_END
}

void CArchiveExtractCallback::CreateComplexDirectory(const UStringVector &dirPathParts, FString &fullPath)
{
  bool isAbsPath = false;
  
  if (!dirPathParts.IsEmpty())
  {
    const UString &s = dirPathParts[0];
    if (s.IsEmpty())
      isAbsPath = true;
    #if defined(_WIN32) && !defined(UNDER_CE)
    else
    {
      if (NName::IsDrivePath2(s))
        isAbsPath = true;
    }
    #endif
  }
  
  if (_pathMode == NExtract::NPathMode::kAbsPaths && isAbsPath)
    fullPath.Empty();
  else
    fullPath = _dirPathPrefix;

  FOR_VECTOR (i, dirPathParts)
  {
    if (i != 0)
      fullPath.Add_PathSepar();
    const UString &s = dirPathParts[i];
    fullPath += us2fs(s);
    #if defined(_WIN32) && !defined(UNDER_CE)
    if (_pathMode == NExtract::NPathMode::kAbsPaths)
      if (i == 0 && s.Len() == 2 && NName::IsDrivePath2(s))
        continue;
    #endif
    CreateDir(fullPath);
  }
}

HRESULT CArchiveExtractCallback::GetTime(UInt32 index, PROPID propID, FILETIME &filetime, bool &filetimeIsDefined)
{
  filetimeIsDefined = false;
  filetime.dwLowDateTime = 0;
  filetime.dwHighDateTime = 0;
  NCOM::CPropVariant prop;
  RINOK(_arc->Archive->GetProperty(index, propID, &prop));
  if (prop.vt == VT_FILETIME)
  {
    filetime = prop.filetime;
    filetimeIsDefined = (filetime.dwHighDateTime != 0 || filetime.dwLowDateTime != 0);
  }
  else if (prop.vt != VT_EMPTY)
    return E_FAIL;
  return S_OK;
}

HRESULT CArchiveExtractCallback::GetUnpackSize()
{
  return _arc->GetItemSize(_index, _curSize, _curSizeDefined);
}

static void AddPathToMessage(UString &s, const FString &path)
{
  s += " : ";
  s += fs2us(path);
}

HRESULT CArchiveExtractCallback::SendMessageError(const char *message, const FString &path)
{
  UString s (message);
  AddPathToMessage(s, path);
  return _extractCallback2->MessageError(s);
}

HRESULT CArchiveExtractCallback::SendMessageError_with_LastError(const char *message, const FString &path)
{
  DWORD errorCode = GetLastError();
  UString s (message);
  if (errorCode != 0)
  {
    s += " : ";
    s += NError::MyFormatMessage(errorCode);
  }
  AddPathToMessage(s, path);
  return _extractCallback2->MessageError(s);
}

HRESULT CArchiveExtractCallback::SendMessageError2(const char *message, const FString &path1, const FString &path2)
{
  UString s (message);
  AddPathToMessage(s, path1);
  AddPathToMessage(s, path2);
  return _extractCallback2->MessageError(s);
}

#ifndef _SFX

STDMETHODIMP CGetProp::GetProp(PROPID propID, PROPVARIANT *value)
{
  /*
  if (propID == kpidName)
  {
    COM_TRY_BEGIN
    NCOM::CPropVariant prop = Name;
    prop.Detach(value);
    return S_OK;
    COM_TRY_END
  }
  */
  return Arc->Archive->GetProperty(IndexInArc, propID, value);
}

#endif


#ifdef SUPPORT_LINKS

static UString GetDirPrefixOf(const UString &src)
{
  UString s (src);
  if (!s.IsEmpty())
  {
    if (IsPathSepar(s.Back()))
      s.DeleteBack();
    int pos = s.ReverseFind_PathSepar();
    s.DeleteFrom(pos + 1);
  }
  return s;
}

#endif


bool IsSafePath(const UString &path)
{
  if (NName::IsAbsolutePath(path))
    return false;

  UStringVector parts;
  SplitPathToParts(path, parts);
  unsigned level = 0;
  
  FOR_VECTOR (i, parts)
  {
    const UString &s = parts[i];
    if (s.IsEmpty())
    {
      if (i == 0)
        return false;
      continue;
    }
    if (s == L".")
      continue;
    if (s == L"..")
    {
      if (level == 0)
        return false;
      level--;
    }
    else
      level++;
  }
  
  return level > 0;
}


bool CensorNode_CheckPath2(const NWildcard::CCensorNode &node, const CReadArcItem &item, bool &include)
{
  bool found = false;
  
  if (node.CheckPathVect(item.PathParts, !item.MainIsDir, include))
  {
    if (!include)
      return true;
    
    #ifdef SUPPORT_ALT_STREAMS
    if (!item.IsAltStream)
      return true;
    #endif
    
    found = true;
  }
  
  #ifdef SUPPORT_ALT_STREAMS

  if (!item.IsAltStream)
    return false;
  
  UStringVector pathParts2 = item.PathParts;
  if (pathParts2.IsEmpty())
    pathParts2.AddNew();
  UString &back = pathParts2.Back();
  back += ':';
  back += item.AltStreamName;
  bool include2;
  
  if (node.CheckPathVect(pathParts2,
      true, // isFile,
      include2))
  {
    include = include2;
    return true;
  }

  #endif

  return found;
}

bool CensorNode_CheckPath(const NWildcard::CCensorNode &node, const CReadArcItem &item)
{
  bool include;
  if (CensorNode_CheckPath2(node, item, include))
    return include;
  return false;
}

static FString MakePath_from_2_Parts(const FString &prefix, const FString &path)
{
  FString s (prefix);
  #if defined(_WIN32) && !defined(UNDER_CE)
  if (!path.IsEmpty() && path[0] == ':' && !prefix.IsEmpty() && IsPathSepar(prefix.Back()))
  {
    if (!NName::IsDriveRootPath_SuperAllowed(prefix))
      s.DeleteBack();
  }
  #endif
  s += path;
  return s;
}


/*
#ifdef SUPPORT_LINKS

struct CTempMidBuffer
{
  void *Buf;

  CTempMidBuffer(size_t size): Buf(NULL) { Buf = ::MidAlloc(size); }
  ~CTempMidBuffer() { ::MidFree(Buf); }
};

HRESULT CArchiveExtractCallback::MyCopyFile(ISequentialOutStream *outStream)
{
  const size_t kBufSize = 1 << 16;
  CTempMidBuffer buf(kBufSize);
  if (!buf.Buf)
    return E_OUTOFMEMORY;
  
  NIO::CInFile inFile;
  NIO::COutFile outFile;
  
  if (!inFile.Open(_CopyFile_Path))
    return SendMessageError_with_LastError("Open error", _CopyFile_Path);
    
  for (;;)
  {
    UInt32 num;
    
    if (!inFile.Read(buf.Buf, kBufSize, num))
      return SendMessageError_with_LastError("Read error", _CopyFile_Path);
      
    if (num == 0)
      return S_OK;
      
      
    RINOK(WriteStream(outStream, buf.Buf, num));
  }
}

#endif
*/


STDMETHODIMP CArchiveExtractCallback::GetStream(UInt32 index, ISequentialOutStream **outStream, Int32 askExtractMode)
{
  COM_TRY_BEGIN

  *outStream = NULL;

  #ifndef _SFX
  if (_hashStream)
    _hashStreamSpec->ReleaseStream();
  _hashStreamWasUsed = false;
  #endif

  _outFileStream.Release();

  _encrypted = false;
  _position = 0;
  _isSplit = false;
  
  _curSize = 0;
  _curSizeDefined = false;
  _fileLengthWasSet = false;
  _index = index;

  _diskFilePath.Empty();

  // _fi.Clear();

  #ifdef SUPPORT_LINKS
  // _CopyFile_Path.Empty();
  linkPath.Empty();
  #endif

  IInArchive *archive = _arc->Archive;

  #ifndef _SFX
  _item._use_baseParentFolder_mode = _use_baseParentFolder_mode;
  if (_use_baseParentFolder_mode)
  {
    _item._baseParentFolder = _baseParentFolder;
    if (_pathMode == NExtract::NPathMode::kFullPaths ||
        _pathMode == NExtract::NPathMode::kAbsPaths)
      _item._baseParentFolder = -1;
  }
  #endif

  #ifdef SUPPORT_ALT_STREAMS
  _item.WriteToAltStreamIfColon = _ntOptions.WriteToAltStreamIfColon;
  #endif

  RINOK(_arc->GetItem(index, _item));

  {
    NCOM::CPropVariant prop;
    RINOK(archive->GetProperty(index, kpidPosition, &prop));
    if (prop.vt != VT_EMPTY)
    {
      if (prop.vt != VT_UI8)
        return E_FAIL;
      _position = prop.uhVal.QuadPart;
      _isSplit = true;
    }
  }

  #ifdef SUPPORT_LINKS
  
  // bool isCopyLink = false;
  bool isHardLink = false;
  bool isJunction = false;
  bool isRelative = false;

  {
    NCOM::CPropVariant prop;
    RINOK(archive->GetProperty(index, kpidHardLink, &prop));
    if (prop.vt == VT_BSTR)
    {
      isHardLink = true;
      // isCopyLink = false;
      isRelative = false; // RAR5, TAR: hard links are from root folder of archive
      linkPath.SetFromBstr(prop.bstrVal);
    }
    else if (prop.vt != VT_EMPTY)
      return E_FAIL;
  }
  
  /*
  {
    NCOM::CPropVariant prop;
    RINOK(archive->GetProperty(index, kpidCopyLink, &prop));
    if (prop.vt == VT_BSTR)
    {
      isHardLink = false;
      isCopyLink = true;
      isRelative = false; // RAR5: copy links are from root folder of archive
      linkPath.SetFromBstr(prop.bstrVal);
    }
    else if (prop.vt != VT_EMPTY)
      return E_FAIL;
  }
  */

  {
    NCOM::CPropVariant prop;
    RINOK(archive->GetProperty(index, kpidSymLink, &prop));
    if (prop.vt == VT_BSTR)
    {
      isHardLink = false;
      // isCopyLink = false;
      isRelative = true; // RAR5, TAR: symbolic links can be relative
      linkPath.SetFromBstr(prop.bstrVal);
    }
    else if (prop.vt != VT_EMPTY)
      return E_FAIL;
  }


  bool isOkReparse = false;

  if (linkPath.IsEmpty() && _arc->GetRawProps)
  {
    const void *data;
    UInt32 dataSize;
    UInt32 propType;
    
    _arc->GetRawProps->GetRawProp(_index, kpidNtReparse, &data, &dataSize, &propType);
    
    if (dataSize != 0)
    {
      if (propType != NPropDataType::kRaw)
        return E_FAIL;
      UString s;
      CReparseAttr reparse;
      DWORD errorCode = 0;
      isOkReparse = reparse.Parse((const Byte *)data, dataSize, errorCode);
      if (isOkReparse)
      {
        isHardLink = false;
        // isCopyLink = false;
        linkPath = reparse.GetPath();
        isJunction = reparse.IsMountPoint();
        isRelative = reparse.IsRelative();
        #ifndef _WIN32
        linkPath.Replace(L'\\', WCHAR_PATH_SEPARATOR);
        #endif
      }
    }
  }

  if (!linkPath.IsEmpty())
  {
    #ifdef _WIN32
    linkPath.Replace(L'/', WCHAR_PATH_SEPARATOR);
    #endif

    // rar5 uses "\??\" prefix for absolute links
    if (linkPath.IsPrefixedBy(WSTRING_PATH_SEPARATOR L"??" WSTRING_PATH_SEPARATOR))
    {
      isRelative = false;
      linkPath.DeleteFrontal(4);
    }
    
    for (;;)
    // while (NName::IsAbsolutePath(linkPath))
    {
      unsigned n = NName::GetRootPrefixSize(linkPath);
      if (n == 0)
        break;
      isRelative = false;
      linkPath.DeleteFrontal(n);
    }
  }

  if (!linkPath.IsEmpty() && !isRelative && _removePathParts.Size() != 0)
  {
    UStringVector pathParts;
    SplitPathToParts(linkPath, pathParts);
    bool badPrefix = false;
    FOR_VECTOR (i, _removePathParts)
    {
      if (CompareFileNames(_removePathParts[i], pathParts[i]) != 0)
      {
        badPrefix = true;
        break;
      }
    }
    if (!badPrefix)
      pathParts.DeleteFrontal(_removePathParts.Size());
    linkPath = MakePathFromParts(pathParts);
  }

  #endif
  
  RINOK(Archive_GetItemBoolProp(archive, index, kpidEncrypted, _encrypted));

  RINOK(GetUnpackSize());

  #ifdef SUPPORT_ALT_STREAMS
  
  if (!_ntOptions.AltStreams.Val && _item.IsAltStream)
    return S_OK;

  #endif


  UStringVector &pathParts = _item.PathParts;

  if (_wildcardCensor)
  {
    if (!CensorNode_CheckPath(*_wildcardCensor, _item))
      return S_OK;
  }

  #ifndef _SFX
  if (_use_baseParentFolder_mode)
  {
    if (!pathParts.IsEmpty())
    {
      unsigned numRemovePathParts = 0;
      
      #ifdef SUPPORT_ALT_STREAMS
      if (_pathMode == NExtract::NPathMode::kNoPathsAlt && _item.IsAltStream)
        numRemovePathParts = pathParts.Size();
      else
      #endif
      if (_pathMode == NExtract::NPathMode::kNoPaths ||
          _pathMode == NExtract::NPathMode::kNoPathsAlt)
        numRemovePathParts = pathParts.Size() - 1;
      pathParts.DeleteFrontal(numRemovePathParts);
    }
  }
  else
  #endif
  {
    if (pathParts.IsEmpty())
    {
      if (_item.IsDir)
        return S_OK;
      /*
      #ifdef SUPPORT_ALT_STREAMS
      if (!_item.IsAltStream)
      #endif
        return E_FAIL;
      */
    }

    unsigned numRemovePathParts = 0;
    
    switch (_pathMode)
    {
      case NExtract::NPathMode::kFullPaths:
      case NExtract::NPathMode::kCurPaths:
      {
        if (_removePathParts.IsEmpty())
          break;
        bool badPrefix = false;
        
        if (pathParts.Size() < _removePathParts.Size())
          badPrefix = true;
        else
        {
          if (pathParts.Size() == _removePathParts.Size())
          {
            if (_removePartsForAltStreams)
            {
              #ifdef SUPPORT_ALT_STREAMS
              if (!_item.IsAltStream)
              #endif
                badPrefix = true;
            }
            else
            {
              if (!_item.MainIsDir)
                badPrefix = true;
            }
          }
          
          if (!badPrefix)
          FOR_VECTOR (i, _removePathParts)
          {
            if (CompareFileNames(_removePathParts[i], pathParts[i]) != 0)
            {
              badPrefix = true;
              break;
            }
          }
        }
        
        if (badPrefix)
        {
          if (askExtractMode == NArchive::NExtract::NAskMode::kExtract && !_testMode)
            return E_FAIL;
        }
        else
          numRemovePathParts = _removePathParts.Size();
        break;
      }
      
      case NExtract::NPathMode::kNoPaths:
      {
        if (!pathParts.IsEmpty())
          numRemovePathParts = pathParts.Size() - 1;
        break;
      }
      case NExtract::NPathMode::kNoPathsAlt:
      {
        #ifdef SUPPORT_ALT_STREAMS
        if (_item.IsAltStream)
          numRemovePathParts = pathParts.Size();
        else
        #endif
        if (!pathParts.IsEmpty())
          numRemovePathParts = pathParts.Size() - 1;
        break;
      }
      /*
      case NExtract::NPathMode::kFullPaths:
      case NExtract::NPathMode::kAbsPaths:
        break;
      */
    }
    
    pathParts.DeleteFrontal(numRemovePathParts);
  }

  #ifndef _SFX

  if (ExtractToStreamCallback)
  {
    if (!GetProp)
    {
      GetProp_Spec = new CGetProp;
      GetProp = GetProp_Spec;
    }
    GetProp_Spec->Arc = _arc;
    GetProp_Spec->IndexInArc = index;
    UString name (MakePathFromParts(pathParts));
    
    #ifdef SUPPORT_ALT_STREAMS
    if (_item.IsAltStream)
    {
      if (!pathParts.IsEmpty() || (!_removePartsForAltStreams && _pathMode != NExtract::NPathMode::kNoPathsAlt))
        name += ':';
      name += _item.AltStreamName;
    }
    #endif

    return ExtractToStreamCallback->GetStream7(name, BoolToInt(_item.IsDir), outStream, askExtractMode, GetProp);
  }

  #endif

  CMyComPtr<ISequentialOutStream> outStreamLoc;

if (askExtractMode == NArchive::NExtract::NAskMode::kExtract && !_testMode)
{
  if (_stdOutMode)
  {
    outStreamLoc = new CStdOutFileStream;
  }
  else
  {
    {
      NCOM::CPropVariant prop;
      RINOK(archive->GetProperty(index, kpidAttrib, &prop));
      if (prop.vt == VT_UI4)
      {
        _fi.Attrib = prop.ulVal;
        _fi.AttribDefined = true;
      }
      else if (prop.vt == VT_EMPTY)
        _fi.AttribDefined = false;
      else
        return E_FAIL;
    }

    RINOK(GetTime(index, kpidCTime, _fi.CTime, _fi.CTimeDefined));
    RINOK(GetTime(index, kpidATime, _fi.ATime, _fi.ATimeDefined));
    RINOK(GetTime(index, kpidMTime, _fi.MTime, _fi.MTimeDefined));

    bool isAnti = false;
    RINOK(_arc->IsItemAnti(index, isAnti));

    #ifdef SUPPORT_ALT_STREAMS
    if (!_item.IsAltStream
        || !pathParts.IsEmpty()
        || !(_removePartsForAltStreams || _pathMode == NExtract::NPathMode::kNoPathsAlt))
    #endif
      Correct_FsPath(_pathMode == NExtract::NPathMode::kAbsPaths, _keepAndReplaceEmptyDirPrefixes, pathParts, _item.MainIsDir);

    #ifdef SUPPORT_ALT_STREAMS
    
    if (_item.IsAltStream)
    {
      UString s (_item.AltStreamName);
      Correct_AltStream_Name(s);
      bool needColon = true;

      if (pathParts.IsEmpty())
      {
        pathParts.AddNew();
        if (_removePartsForAltStreams || _pathMode == NExtract::NPathMode::kNoPathsAlt)
          needColon = false;
      }
      else if (_pathMode == NExtract::NPathMode::kAbsPaths &&
          NWildcard::GetNumPrefixParts_if_DrivePath(pathParts) == pathParts.Size())
        pathParts.AddNew();

      UString &name = pathParts.Back();
      if (needColon)
        name += (char)(_ntOptions.ReplaceColonForAltStream ? '_' : ':');
      name += s;
    }
    
    #endif

    UString processedPath (MakePathFromParts(pathParts));
    
    if (!isAnti)
    {
      if (!_item.IsDir)
      {
        if (!pathParts.IsEmpty())
          pathParts.DeleteBack();
      }
    
      if (!pathParts.IsEmpty())
      {
        FString fullPathNew;
        CreateComplexDirectory(pathParts, fullPathNew);
        
        if (_item.IsDir)
        {
          CDirPathTime &pt = _extractedFolders.AddNew();
          
          pt.CTime = _fi.CTime;
          pt.CTimeDefined = (WriteCTime && _fi.CTimeDefined);
          
          pt.ATime = _fi.ATime;
          pt.ATimeDefined = (WriteATime && _fi.ATimeDefined);
          
          pt.MTimeDefined = false;

          if (WriteMTime)
          {
            if (_fi.MTimeDefined)
            {
              pt.MTime = _fi.MTime;
              pt.MTimeDefined = true;
            }
            else if (_arc->MTimeDefined)
            {
              pt.MTime = _arc->MTime;
              pt.MTimeDefined = true;
            }
          }

          pt.Path = fullPathNew;

          pt.SetDirTime();
        }
      }
    }


    FString fullProcessedPath (us2fs(processedPath));
    if (_pathMode != NExtract::NPathMode::kAbsPaths
        || !NName::IsAbsolutePath(processedPath))
    {
       fullProcessedPath = MakePath_from_2_Parts(_dirPathPrefix, fullProcessedPath);
    }

    #ifdef SUPPORT_ALT_STREAMS
    
    if (_item.IsAltStream && _item.ParentIndex != (UInt32)(Int32)-1)
    {
      int renIndex = _renamedFiles.FindInSorted(CIndexToPathPair(_item.ParentIndex));
      if (renIndex >= 0)
      {
        const CIndexToPathPair &pair = _renamedFiles[renIndex];
        fullProcessedPath = pair.Path;
        fullProcessedPath += ':';
        UString s (_item.AltStreamName);
        Correct_AltStream_Name(s);
        fullProcessedPath += us2fs(s);
      }
    }
    
    #endif

    bool isRenamed = false;

    if (_item.IsDir)
    {
      _diskFilePath = fullProcessedPath;
      if (isAnti)
        RemoveDir(_diskFilePath);
      #ifdef SUPPORT_LINKS
      if (linkPath.IsEmpty())
      #endif
        return S_OK;
    }
    else if (!_isSplit)
    {
    
    // ----- Is file (not split) -----
    NFind::CFileInfo fileInfo;
    if (fileInfo.Find(fullProcessedPath))
    {
      switch (_overwriteMode)
      {
        case NExtract::NOverwriteMode::kSkip:
          return S_OK;
        case NExtract::NOverwriteMode::kAsk:
        {
          int slashPos = fullProcessedPath.ReverseFind_PathSepar();
          FString realFullProcessedPath (fullProcessedPath.Left(slashPos + 1) + fileInfo.Name);

          Int32 overwriteResult;
          RINOK(_extractCallback2->AskOverwrite(
              fs2us(realFullProcessedPath), &fileInfo.MTime, &fileInfo.Size, _item.Path,
              _fi.MTimeDefined ? &_fi.MTime : NULL,
              _curSizeDefined ? &_curSize : NULL,
              &overwriteResult))

          switch (overwriteResult)
          {
            case NOverwriteAnswer::kCancel: return E_ABORT;
            case NOverwriteAnswer::kNo: return S_OK;
            case NOverwriteAnswer::kNoToAll: _overwriteMode = NExtract::NOverwriteMode::kSkip; return S_OK;
            case NOverwriteAnswer::kYes: break;
            case NOverwriteAnswer::kYesToAll: _overwriteMode = NExtract::NOverwriteMode::kOverwrite; break;
            case NOverwriteAnswer::kAutoRename: _overwriteMode = NExtract::NOverwriteMode::kRename; break;
            default:
              return E_FAIL;
          }
        }
      }
      if (_overwriteMode == NExtract::NOverwriteMode::kRename)
      {
        if (!AutoRenamePath(fullProcessedPath))
        {
          RINOK(SendMessageError(kCantAutoRename, fullProcessedPath));
          return E_FAIL;
        }
        isRenamed = true;
      }
      else if (_overwriteMode == NExtract::NOverwriteMode::kRenameExisting)
      {
        FString existPath (fullProcessedPath);
        if (!AutoRenamePath(existPath))
        {
          RINOK(SendMessageError(kCantAutoRename, fullProcessedPath));
          return E_FAIL;
        }
        // MyMoveFile can raname folders. So it's OK to use it for folders too
        if (!MyMoveFile(fullProcessedPath, existPath))
        {
          RINOK(SendMessageError2(kCantRenameFile, existPath, fullProcessedPath));
          return E_FAIL;
        }
      }
      else
      {
        if (fileInfo.IsDir())
        {
          // do we need to delete all files in folder?
          if (!RemoveDir(fullProcessedPath))
          {
            RINOK(SendMessageError_with_LastError(kCantDeleteOutputDir, fullProcessedPath));
            return S_OK;
          }
        }
        else
        {
          bool needDelete = true;
          if (needDelete)
          {
            if (NFind::DoesFileExist(fullProcessedPath))
            if (!DeleteFileAlways(fullProcessedPath))
            if (GetLastError() != ERROR_FILE_NOT_FOUND)
            {
              RINOK(SendMessageError_with_LastError(kCantDeleteOutputFile, fullProcessedPath));
              return S_OK;
              // return E_FAIL;
            }
          }
        }
      }
    }
    else // not Find(fullProcessedPath)
    {
      // we need to clear READ-ONLY of parent before creating alt stream
      #if defined(_WIN32) && !defined(UNDER_CE)
      int colonPos = NName::FindAltStreamColon(fullProcessedPath);
      if (colonPos >= 0 && fullProcessedPath[(unsigned)colonPos + 1] != 0)
      {
        FString parentFsPath (fullProcessedPath);
        parentFsPath.DeleteFrom(colonPos);
        NFind::CFileInfo parentFi;
        if (parentFi.Find(parentFsPath))
        {
          if (parentFi.IsReadOnly())
            SetFileAttrib(parentFsPath, parentFi.Attrib & ~FILE_ATTRIBUTE_READONLY);
        }
      }
      #endif
    }
    // ----- END of code for    Is file (not split) -----

    }
    _diskFilePath = fullProcessedPath;
    

    if (!isAnti)
    {
      #ifdef SUPPORT_LINKS

      if (!linkPath.IsEmpty())
      {
        #ifndef UNDER_CE

        UString relatPath;
        if (isRelative)
          relatPath = GetDirPrefixOf(_item.Path);
        relatPath += linkPath;
        
        if (!IsSafePath(relatPath))
        {
          RINOK(SendMessageError("Dangerous link path was ignored", us2fs(relatPath)));
        }
        else
        {
          FString existPath;
          if (isHardLink /* || isCopyLink */ || !isRelative)
          {
            if (!NName::GetFullPath(_dirPathPrefix_Full, us2fs(relatPath), existPath))
            {
              RINOK(SendMessageError("Incorrect path", us2fs(relatPath)));
            }
          }
          else
          {
            existPath = us2fs(linkPath);
          }
          
          if (!existPath.IsEmpty())
          {
            if (isHardLink /* || isCopyLink */)
            {
              // if (isHardLink)
              {
                if (!MyCreateHardLink(fullProcessedPath, existPath))
                {
                  RINOK(SendMessageError2(kCantCreateHardLink, fullProcessedPath, existPath));
                  // return S_OK;
                }
              }
              /*
              else
              {
                NFind::CFileInfo fi;
                if (!fi.Find(existPath))
                {
                  RINOK(SendMessageError2("Can not find the file for copying", existPath, fullProcessedPath));
                }
                else
                {
                  if (_curSizeDefined && _curSize == fi.Size)
                    _CopyFile_Path = existPath;
                  else
                  {
                    RINOK(SendMessageError2("File size collision for file copying", existPath, fullProcessedPath));
                  }

                  // RINOK(MyCopyFile(existPath, fullProcessedPath));
                }
              }
              */
            }
            else if (_ntOptions.SymLinks.Val)
            {
              // bool isSymLink = true; // = false for junction
              if (_item.IsDir && !isRelative)
              {
                // if it's before Vista we use Junction Point
                // isJunction = true;
                // convertToAbs = true;
              }
              
              CByteBuffer data;
              if (FillLinkData(data, fs2us(existPath), !isJunction))
              {
                CReparseAttr attr;
                DWORD errorCode = 0;
                if (!attr.Parse(data, data.Size(), errorCode))
                {
                  RINOK(SendMessageError("Internal error for symbolic link file", us2fs(_item.Path)));
                  // return E_FAIL;
                }
                else
                if (!NFile::NIO::SetReparseData(fullProcessedPath, _item.IsDir, data, (DWORD)data.Size()))
                {
                  RINOK(SendMessageError_with_LastError(kCantCreateSymLink, fullProcessedPath));
                }
              }
            }
          }
        }
        
        #endif
      }
      
      if (linkPath.IsEmpty() /* || !_CopyFile_Path.IsEmpty() */)
      #endif // SUPPORT_LINKS
      {
        bool needWriteFile = true;
        
        #ifdef SUPPORT_LINKS
        if (!_hardLinks.IDs.IsEmpty() && !_item.IsAltStream)
        {
          CHardLinkNode h;
          bool defined;
          RINOK(Archive_Get_HardLinkNode(archive, index, h, defined));
          if (defined)
          {
            {
              int linkIndex = _hardLinks.IDs.FindInSorted2(h);
              if (linkIndex >= 0)
              {
                FString &hl = _hardLinks.Links[linkIndex];
                if (hl.IsEmpty())
                  hl = fullProcessedPath;
                else
                {
                  if (!MyCreateHardLink(fullProcessedPath, hl))
                  {
                    RINOK(SendMessageError2(kCantCreateHardLink, fullProcessedPath, hl));
                    return S_OK;
                  }
                  needWriteFile = false;
                }
              }
            }
          }
        }
        #endif
        
        if (needWriteFile)
        {
          _outFileStreamSpec = new COutFileStream;
          CMyComPtr<ISequentialOutStream> outStreamLoc2(_outFileStreamSpec);
          if (!_outFileStreamSpec->Open(fullProcessedPath, _isSplit ? OPEN_ALWAYS: CREATE_ALWAYS))
          {
            // if (::GetLastError() != ERROR_FILE_EXISTS || !isSplit)
            {
              RINOK(SendMessageError_with_LastError(kCantOpenOutFile, fullProcessedPath));
              return S_OK;
            }
          }

          if (_ntOptions.PreAllocateOutFile && !_isSplit && _curSizeDefined && _curSize > (1 << 12))
          {
            // UInt64 ticks = GetCpuTicks();
            bool res = _outFileStreamSpec->File.SetLength(_curSize);
            _fileLengthWasSet = res;

            // ticks = GetCpuTicks() - ticks;
            // printf("\nticks = %10d\n", (unsigned)ticks);
            if (!res)
            {
              RINOK(SendMessageError_with_LastError(kCantSetFileLen, fullProcessedPath));
            }

            /*
            _outFileStreamSpec->File.Close();
            ticks = GetCpuTicks() - ticks;
            printf("\nticks = %10d\n", (unsigned)ticks);
            return S_FALSE;
            */

            /*
              File.SetLength() on FAT (xp64): is fast, but then File.Close() can be slow,
              if we don't write any data.
              File.SetLength() for remote share file (exFAT) can be slow in some cases,
              and the Windows can return "network error" after 1 minute,
              while remote file still can grow.
              We need some way to detect such bad cases and disable PreAllocateOutFile mode.
            */

            res = _outFileStreamSpec->File.SeekToBegin();
            if (!res)
            {
              RINOK(SendMessageError_with_LastError("Can not seek to begin of file", fullProcessedPath));
            }
          }

          #ifdef SUPPORT_ALT_STREAMS
          if (isRenamed && !_item.IsAltStream)
          {
            CIndexToPathPair pair(index, fullProcessedPath);
            unsigned oldSize = _renamedFiles.Size();
            unsigned insertIndex = _renamedFiles.AddToUniqueSorted(pair);
            if (oldSize == _renamedFiles.Size())
              _renamedFiles[insertIndex].Path = fullProcessedPath;
          }
          #endif

          if (_isSplit)
          {
            RINOK(_outFileStreamSpec->Seek(_position, STREAM_SEEK_SET, NULL));
          }
         
          _outFileStream = outStreamLoc2;
        }
      }
    }
    
    outStreamLoc = _outFileStream;
  }
}

  #ifndef _SFX

  if (_hashStream)
  {
    if (askExtractMode == NArchive::NExtract::NAskMode::kExtract ||
        askExtractMode == NArchive::NExtract::NAskMode::kTest)
    {
      _hashStreamSpec->SetStream(outStreamLoc);
      outStreamLoc = _hashStream;
      _hashStreamSpec->Init(true);
      _hashStreamWasUsed = true;
    }
  }

  #endif

  
  if (outStreamLoc)
  {
    /*
    #ifdef SUPPORT_LINKS
    
    if (!_CopyFile_Path.IsEmpty())
    {
      RINOK(PrepareOperation(askExtractMode));
      RINOK(MyCopyFile(outStreamLoc));
      return SetOperationResult(NArchive::NExtract::NOperationResult::kOK);
    }

    if (isCopyLink && _testMode)
      return S_OK;
    
    #endif
    */

    *outStream = outStreamLoc.Detach();
  }
  
  return S_OK;

  COM_TRY_END
}


STDMETHODIMP CArchiveExtractCallback::PrepareOperation(Int32 askExtractMode)
{
  COM_TRY_BEGIN

  #ifndef _SFX
  if (ExtractToStreamCallback)
    return ExtractToStreamCallback->PrepareOperation7(askExtractMode);
  #endif
  
  _extractMode = false;
  
  switch (askExtractMode)
  {
    case NArchive::NExtract::NAskMode::kExtract:
      if (_testMode)
        askExtractMode = NArchive::NExtract::NAskMode::kTest;
      else
        _extractMode = true;
      break;
  };
  
  return _extractCallback2->PrepareOperation(_item.Path, BoolToInt(_item.IsDir),
      askExtractMode, _isSplit ? &_position: 0);
  
  COM_TRY_END
}


HRESULT CArchiveExtractCallback::CloseFile()
{
  if (!_outFileStream)
    return S_OK;
  
  HRESULT hres = S_OK;
  _outFileStreamSpec->SetTime(
      (WriteCTime && _fi.CTimeDefined) ? &_fi.CTime : NULL,
      (WriteATime && _fi.ATimeDefined) ? &_fi.ATime : NULL,
      (WriteMTime && _fi.MTimeDefined) ? &_fi.MTime : (_arc->MTimeDefined ? &_arc->MTime : NULL));
  
  const UInt64 processedSize = _outFileStreamSpec->ProcessedSize;
  if (_fileLengthWasSet && _curSize > processedSize)
  {
    bool res = _outFileStreamSpec->File.SetLength(processedSize);
    _fileLengthWasSet = res;
    if (!res)
      hres = SendMessageError_with_LastError(kCantSetFileLen, us2fs(_item.Path));
  }
  _curSize = processedSize;
  _curSizeDefined = true;
  RINOK(_outFileStreamSpec->Close());
  _outFileStream.Release();
  return hres;
}


STDMETHODIMP CArchiveExtractCallback::SetOperationResult(Int32 opRes)
{
  COM_TRY_BEGIN

  #ifndef _SFX
  if (ExtractToStreamCallback)
    return ExtractToStreamCallback->SetOperationResult7(opRes, BoolToInt(_encrypted));
  #endif

  #ifndef _SFX

  if (_hashStreamWasUsed)
  {
    _hashStreamSpec->_hash->Final(_item.IsDir,
        #ifdef SUPPORT_ALT_STREAMS
          _item.IsAltStream
        #else
          false
        #endif
        , _item.Path);
    _curSize = _hashStreamSpec->GetSize();
    _curSizeDefined = true;
    _hashStreamSpec->ReleaseStream();
    _hashStreamWasUsed = false;
  }

  #endif

  RINOK(CloseFile());
  
  #ifdef _USE_SECURITY_CODE
  if (!_stdOutMode && _extractMode && _ntOptions.NtSecurity.Val && _arc->GetRawProps)
  {
    const void *data;
    UInt32 dataSize;
    UInt32 propType;
    _arc->GetRawProps->GetRawProp(_index, kpidNtSecure, &data, &dataSize, &propType);
    if (dataSize != 0)
    {
      if (propType != NPropDataType::kRaw)
        return E_FAIL;
      if (CheckNtSecure((const Byte *)data, dataSize))
      {
        SECURITY_INFORMATION securInfo = DACL_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION;
        if (_saclEnabled)
          securInfo |= SACL_SECURITY_INFORMATION;
        ::SetFileSecurityW(fs2us(_diskFilePath), securInfo, (PSECURITY_DESCRIPTOR)(void *)data);
      }
    }
  }
  #endif

  if (!_curSizeDefined)
    GetUnpackSize();
  
  if (_curSizeDefined)
  {
    #ifdef SUPPORT_ALT_STREAMS
    if (_item.IsAltStream)
      AltStreams_UnpackSize += _curSize;
    else
    #endif
      UnpackSize += _curSize;
  }
    
  if (_item.IsDir)
    NumFolders++;
  #ifdef SUPPORT_ALT_STREAMS
  else if (_item.IsAltStream)
    NumAltStreams++;
  #endif
  else
    NumFiles++;

  if (!_stdOutMode && _extractMode && _fi.AttribDefined)
    SetFileAttrib_PosixHighDetect(_diskFilePath, _fi.Attrib);
  
  RINOK(_extractCallback2->SetOperationResult(opRes, BoolToInt(_encrypted)));
  
  return S_OK;
  
  COM_TRY_END
}

STDMETHODIMP CArchiveExtractCallback::ReportExtractResult(UInt32 indexType, UInt32 index, Int32 opRes)
{
  if (_folderArchiveExtractCallback2)
  {
    bool isEncrypted = false;
    UString s;
    
    if (indexType == NArchive::NEventIndexType::kInArcIndex && index != (UInt32)(Int32)-1)
    {
      CReadArcItem item;
      RINOK(_arc->GetItem(index, item));
      s = item.Path;
      RINOK(Archive_GetItemBoolProp(_arc->Archive, index, kpidEncrypted, isEncrypted));
    }
    else
    {
      s = '#';
      s.Add_UInt32(index);
      // if (indexType == NArchive::NEventIndexType::kBlockIndex) {}
    }
    
    return _folderArchiveExtractCallback2->ReportExtractResult(opRes, isEncrypted, s);
  }

  return S_OK;
}


STDMETHODIMP CArchiveExtractCallback::CryptoGetTextPassword(BSTR *password)
{
  COM_TRY_BEGIN
  if (!_cryptoGetTextPassword)
  {
    RINOK(_extractCallback2.QueryInterface(IID_ICryptoGetTextPassword,
        &_cryptoGetTextPassword));
  }
  return _cryptoGetTextPassword->CryptoGetTextPassword(password);
  COM_TRY_END
}


void CDirPathSortPair::SetNumSlashes(const FChar *s)
{
  for (unsigned numSlashes = 0;;)
  {
    FChar c = *s++;
    if (c == 0)
    {
      Len = numSlashes;
      return;
    }
    if (IS_PATH_SEPAR(c))
      numSlashes++;
  }
}


bool CDirPathTime::SetDirTime()
{
  return NDir::SetDirTime(Path,
      CTimeDefined ? &CTime : NULL,
      ATimeDefined ? &ATime : NULL,
      MTimeDefined ? &MTime : NULL);
}


HRESULT CArchiveExtractCallback::SetDirsTimes()
{
  if (!_arc)
    return S_OK;

  CRecordVector<CDirPathSortPair> pairs;
  pairs.ClearAndSetSize(_extractedFolders.Size());
  unsigned i;
  
  for (i = 0; i < _extractedFolders.Size(); i++)
  {
    CDirPathSortPair &pair = pairs[i];
    pair.Index = i;
    pair.SetNumSlashes(_extractedFolders[i].Path);
  }
  
  pairs.Sort2();
  
  for (i = 0; i < pairs.Size(); i++)
  {
    _extractedFolders[pairs[i].Index].SetDirTime();
    // if (!) return GetLastError();
  }

  ClearExtractedDirsInfo();
  return S_OK;
}


HRESULT CArchiveExtractCallback::CloseArc()
{
  HRESULT res = CloseFile();
  HRESULT res2 = SetDirsTimes();
  if (res == S_OK)
    res = res2;
  _arc = NULL;
  return res;
}
