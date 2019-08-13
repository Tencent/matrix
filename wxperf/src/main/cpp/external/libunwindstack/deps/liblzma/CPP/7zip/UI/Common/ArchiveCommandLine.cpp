// ArchiveCommandLine.cpp

#include "StdAfx.h"
#undef printf
#undef sprintf

#ifdef _WIN32
#ifndef UNDER_CE
#include <io.h>
#endif
#endif
#include <stdio.h>

#include "../../../Common/ListFileUtils.h"
#include "../../../Common/StringConvert.h"
#include "../../../Common/StringToInt.h"

#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileName.h"
#ifdef _WIN32
#include "../../../Windows/FileMapping.h"
#include "../../../Windows/Synchronization.h"
#endif

#include "ArchiveCommandLine.h"
#include "EnumDirItems.h"
#include "SortUtils.h"
#include "Update.h"
#include "UpdateAction.h"

extern bool g_CaseSensitive;

#ifdef UNDER_CE

#define MY_IS_TERMINAL(x) false;

#else

#if _MSC_VER >= 1400
#define MY_isatty_fileno(x) _isatty(_fileno(x))
#else
#define MY_isatty_fileno(x) isatty(fileno(x))
#endif

#define MY_IS_TERMINAL(x) (MY_isatty_fileno(x) != 0);

#endif

using namespace NCommandLineParser;
using namespace NWindows;
using namespace NFile;

static bool StringToUInt32(const wchar_t *s, UInt32 &v)
{
  if (*s == 0)
    return false;
  const wchar_t *end;
  v = ConvertStringToUInt32(s, &end);
  return *end == 0;
}

CArcCmdLineException::CArcCmdLineException(const char *a, const wchar_t *u)
{
  (*this) += MultiByteToUnicodeString(a);
  if (u)
  {
    this->Add_LF();
    (*this) += u;
  }
}

int g_CodePage = -1;

namespace NKey {
enum Enum
{
  kHelp1 = 0,
  kHelp2,
  kHelp3,
  
  kDisableHeaders,
  kDisablePercents,
  kShowTime,
  kLogLevel,

  kOutStream,
  kErrStream,
  kPercentStream,

  kYes,

  kShowDialog,
  kOverwrite,

  kArchiveType,
  kExcludedArcType,

  kProperty,
  kOutputDir,
  kWorkingDir,
  
  kInclude,
  kExclude,
  kArInclude,
  kArExclude,
  kNoArName,

  kUpdate,
  kVolume,
  kRecursed,

  kAffinity,
  kSfx,
  kEmail,
  kHash,
 
  kStdIn,
  kStdOut,

  kLargePages,
  kListfileCharSet,
  kConsoleCharSet,
  kTechMode,
  
  kShareForWrite,
  kCaseSensitive,
  kArcNameMode,

  kDisableWildcardParsing,
  kElimDup,
  kFullPathMode,
  
  kHardLinks,
  kSymLinks,
  kNtSecurity,
  kAltStreams,
  kReplaceColonForAltStream,
  kWriteToAltStreamIfColon,

  kDeleteAfterCompressing,
  kSetArcMTime

  #ifndef _NO_CRYPTO
  , kPassword
  #endif
};

}


static const wchar_t kRecursedIDChar = 'r';
static const char *kRecursedPostCharSet = "0-";

static const char *k_ArcNameMode_PostCharSet = "sea";

static const char *k_Stream_PostCharSet = "012";

static inline const EArcNameMode ParseArcNameMode(int postCharIndex)
{
  switch (postCharIndex)
  {
    case 1: return k_ArcNameMode_Exact;
    case 2: return k_ArcNameMode_Add;
    default: return k_ArcNameMode_Smart;
  }
}

namespace NRecursedPostCharIndex {
  enum EEnum
  {
    kWildcardRecursionOnly = 0,
    kNoRecursion = 1
  };
}

static const char kImmediateNameID = '!';
static const char kMapNameID = '#';
static const char kFileListID = '@';

static const char kSomeCludePostStringMinSize = 2; // at least <@|!><N>ame must be
static const char kSomeCludeAfterRecursedPostStringMinSize = 2; // at least <@|!><N>ame must be

static const char *kOverwritePostCharSet = "asut";

static const NExtract::NOverwriteMode::EEnum k_OverwriteModes[] =
{
  NExtract::NOverwriteMode::kOverwrite,
  NExtract::NOverwriteMode::kSkip,
  NExtract::NOverwriteMode::kRename,
  NExtract::NOverwriteMode::kRenameExisting
};

static const CSwitchForm kSwitchForms[] =
{
  { "?" },
  { "h" },
  { "-help" },
  
  { "ba" },
  { "bd" },
  { "bt" },
  { "bb", NSwitchType::kString, false, 0 },

  { "bso", NSwitchType::kChar, false, 1, k_Stream_PostCharSet },
  { "bse", NSwitchType::kChar, false, 1, k_Stream_PostCharSet },
  { "bsp", NSwitchType::kChar, false, 1, k_Stream_PostCharSet },
  
  { "y" },
  
  { "ad" },
  { "ao", NSwitchType::kChar, false, 1, kOverwritePostCharSet},

  { "t",  NSwitchType::kString, false, 1 },
  { "stx", NSwitchType::kString, true, 1 },
  
  { "m",  NSwitchType::kString, true, 1 },
  { "o",  NSwitchType::kString, false, 1 },
  { "w",  NSwitchType::kString },

  { "i",  NSwitchType::kString, true, kSomeCludePostStringMinSize},
  { "x",  NSwitchType::kString, true, kSomeCludePostStringMinSize},
  { "ai", NSwitchType::kString, true, kSomeCludePostStringMinSize},
  { "ax", NSwitchType::kString, true, kSomeCludePostStringMinSize},
  { "an" },
  
  { "u",  NSwitchType::kString, true, 1},
  { "v",  NSwitchType::kString, true, 1},
  { "r",  NSwitchType::kChar, false, 0, kRecursedPostCharSet },
  
  { "stm", NSwitchType::kString },
  { "sfx", NSwitchType::kString },
  { "seml", NSwitchType::kString, false, 0},
  { "scrc", NSwitchType::kString, true, 0 },
  
  { "si", NSwitchType::kString },
  { "so" },

  { "slp", NSwitchType::kMinus },
  { "scs", NSwitchType::kString },
  { "scc", NSwitchType::kString },
  { "slt" },

  { "ssw" },
  { "ssc", NSwitchType::kMinus },
  { "sa",  NSwitchType::kChar, false, 1, k_ArcNameMode_PostCharSet },
  
  { "spd" },
  { "spe", NSwitchType::kMinus },
  { "spf", NSwitchType::kString, false, 0 },
  
  { "snh", NSwitchType::kMinus },
  { "snl", NSwitchType::kMinus },
  { "sni" },
  { "sns", NSwitchType::kMinus },
  { "snr" },
  { "snc" },
  
  { "sdel" },
  { "stl" }

  #ifndef _NO_CRYPTO
  , { "p",  NSwitchType::kString }
  #endif
};

static const wchar_t *kUniversalWildcard = L"*";
static const unsigned kMinNonSwitchWords = 1;
static const unsigned kCommandIndex = 0;

// static const char *kUserErrorMessage  = "Incorrect command line";
static const char *kCannotFindListFile = "Cannot find listfile";
static const char *kIncorrectListFile = "Incorrect item in listfile.\nCheck charset encoding and -scs switch.";
static const char *kTerminalOutError = "I won't write compressed data to a terminal";
static const char *kSameTerminalError = "I won't write data and program's messages to same stream";
static const char *kEmptyFilePath = "Empty file path";
static const char *kCannotFindArchive = "Cannot find archive";

bool CArcCommand::IsFromExtractGroup() const
{
  switch (CommandType)
  {
    case NCommandType::kTest:
    case NCommandType::kExtract:
    case NCommandType::kExtractFull:
      return true;
  }
  return false;
}

NExtract::NPathMode::EEnum CArcCommand::GetPathMode() const
{
  switch (CommandType)
  {
    case NCommandType::kTest:
    case NCommandType::kExtractFull:
      return NExtract::NPathMode::kFullPaths;
  }
  return NExtract::NPathMode::kNoPaths;
}

bool CArcCommand::IsFromUpdateGroup() const
{
  switch (CommandType)
  {
    case NCommandType::kAdd:
    case NCommandType::kUpdate:
    case NCommandType::kDelete:
    case NCommandType::kRename:
      return true;
  }
  return false;
}

static NRecursedType::EEnum GetRecursedTypeFromIndex(int index)
{
  switch (index)
  {
    case NRecursedPostCharIndex::kWildcardRecursionOnly:
      return NRecursedType::kWildcardOnlyRecursed;
    case NRecursedPostCharIndex::kNoRecursion:
      return NRecursedType::kNonRecursed;
    default:
      return NRecursedType::kRecursed;
  }
}

static const char *g_Commands = "audtexlbih";

static bool ParseArchiveCommand(const UString &commandString, CArcCommand &command)
{
  UString s = commandString;
  s.MakeLower_Ascii();
  if (s.Len() == 1)
  {
    if (s[0] > 0x7F)
      return false;
    int index = FindCharPosInString(g_Commands, (char)s[0]);
    if (index < 0)
      return false;
    command.CommandType = (NCommandType::EEnum)index;
    return true;
  }
  if (s.Len() == 2 && s[0] == 'r' && s[1] == 'n')
  {
    command.CommandType = (NCommandType::kRename);
    return true;
  }
  return false;
}

// ------------------------------------------------------------------
// filenames functions

static void AddNameToCensor(NWildcard::CCensor &censor,
    const UString &name, bool include, NRecursedType::EEnum type, bool wildcardMatching)
{
  bool recursed = false;

  switch (type)
  {
    case NRecursedType::kWildcardOnlyRecursed:
      recursed = DoesNameContainWildcard(name);
      break;
    case NRecursedType::kRecursed:
      recursed = true;
      break;
  }
  censor.AddPreItem(include, name, recursed, wildcardMatching);
}

static void AddRenamePair(CObjectVector<CRenamePair> *renamePairs,
    const UString &oldName, const UString &newName, NRecursedType::EEnum type,
    bool wildcardMatching)
{
  CRenamePair &pair = renamePairs->AddNew();
  pair.OldName = oldName;
  pair.NewName = newName;
  pair.RecursedType = type;
  pair.WildcardParsing = wildcardMatching;

  if (!pair.Prepare())
  {
    UString val;
    val += pair.OldName;
    val.Add_LF();
    val += pair.NewName;
    val.Add_LF();
    if (type == NRecursedType::kRecursed)
      val.AddAscii("-r");
    else if (type == NRecursedType::kWildcardOnlyRecursed)
      val.AddAscii("-r0");
    throw CArcCmdLineException("Unsupported rename command:", val);
  }
}

static void AddToCensorFromListFile(
    CObjectVector<CRenamePair> *renamePairs,
    NWildcard::CCensor &censor,
    LPCWSTR fileName, bool include, NRecursedType::EEnum type, bool wildcardMatching, Int32 codePage)
{
  UStringVector names;
  if (!NFind::DoesFileExist(us2fs(fileName)))
    throw CArcCmdLineException(kCannotFindListFile, fileName);
  if (!ReadNamesFromListFile(us2fs(fileName), names, codePage))
    throw CArcCmdLineException(kIncorrectListFile, fileName);
  if (renamePairs)
  {
    if ((names.Size() & 1) != 0)
      throw CArcCmdLineException(kIncorrectListFile, fileName);
    for (unsigned i = 0; i < names.Size(); i += 2)
    {
      // change type !!!!
      AddRenamePair(renamePairs, names[i], names[i + 1], type, wildcardMatching);
    }
  }
  else
    FOR_VECTOR (i, names)
      AddNameToCensor(censor, names[i], include, type, wildcardMatching);
}

static void AddToCensorFromNonSwitchesStrings(
    CObjectVector<CRenamePair> *renamePairs,
    unsigned startIndex,
    NWildcard::CCensor &censor,
    const UStringVector &nonSwitchStrings, NRecursedType::EEnum type,
    bool wildcardMatching,
    bool thereAreSwitchIncludes, Int32 codePage)
{
  if ((renamePairs || nonSwitchStrings.Size() == startIndex) && !thereAreSwitchIncludes)
    AddNameToCensor(censor, kUniversalWildcard, true, type,
        true // wildcardMatching
        );

  int oldIndex = -1;
  
  for (unsigned i = startIndex; i < nonSwitchStrings.Size(); i++)
  {
    const UString &s = nonSwitchStrings[i];
    if (s.IsEmpty())
      throw CArcCmdLineException(kEmptyFilePath);
    if (s[0] == kFileListID)
      AddToCensorFromListFile(renamePairs, censor, s.Ptr(1), true, type, wildcardMatching, codePage);
    else if (renamePairs)
    {
      if (oldIndex == -1)
        oldIndex = i;
      else
      {
        // NRecursedType::EEnum type is used for global wildcard (-i! switches)
        AddRenamePair(renamePairs, nonSwitchStrings[oldIndex], s, NRecursedType::kNonRecursed, wildcardMatching);
        // AddRenamePair(renamePairs, nonSwitchStrings[oldIndex], s, type);
        oldIndex = -1;
      }
    }
    else
      AddNameToCensor(censor, s, true, type, wildcardMatching);
  }
  
  if (oldIndex != -1)
  {
    throw CArcCmdLineException("There is no second file name for rename pair:", nonSwitchStrings[oldIndex]);
  }
}

#ifdef _WIN32

struct CEventSetEnd
{
  UString Name;
  
  CEventSetEnd(const wchar_t *name): Name(name) {}
  ~CEventSetEnd()
  {
    NSynchronization::CManualResetEvent event;
    if (event.Open(EVENT_MODIFY_STATE, false, GetSystemString(Name)) == 0)
      event.Set();
  }
};

const char *k_IncorrectMapCommand = "Incorrect Map command";

static const char *ParseMapWithPaths(
    NWildcard::CCensor &censor,
    const UString &s2, bool include,
    NRecursedType::EEnum commonRecursedType,
    bool wildcardMatching)
{
  UString s = s2;
  int pos = s.Find(L':');
  if (pos < 0)
    return k_IncorrectMapCommand;
  int pos2 = s.Find(L':', pos + 1);
  if (pos2 < 0)
    return k_IncorrectMapCommand;

  CEventSetEnd eventSetEnd((const wchar_t *)s + ((unsigned)pos2 + 1));
  s.DeleteFrom(pos2);
  UInt32 size;
  if (!StringToUInt32(s.Ptr(pos + 1), size)
      || size < sizeof(wchar_t)
      || size > ((UInt32)1 << 31)
      || size % sizeof(wchar_t) != 0)
    return "Unsupported Map data size";

  s.DeleteFrom(pos);
  CFileMapping map;
  if (map.Open(FILE_MAP_READ, GetSystemString(s)) != 0)
    return "Can not open mapping";
  LPVOID data = map.Map(FILE_MAP_READ, 0, size);
  if (!data)
    return "MapViewOfFile error";
  CFileUnmapper unmapper(data);

  UString name;
  const wchar_t *p = (const wchar_t *)data;
  if (*p != 0) // data format marker
    return "Unsupported Map data";
  UInt32 numChars = size / sizeof(wchar_t);
  for (UInt32 i = 1; i < numChars; i++)
  {
    wchar_t c = p[i];
    if (c == 0)
    {
      // MessageBoxW(0, name, L"7-Zip", 0);
      AddNameToCensor(censor, name, include, commonRecursedType, wildcardMatching);
      name.Empty();
    }
    else
      name += c;
  }
  if (!name.IsEmpty())
    return "Map data error";

  return NULL;
}

#endif

static void AddSwitchWildcardsToCensor(
    NWildcard::CCensor &censor,
    const UStringVector &strings, bool include,
    NRecursedType::EEnum commonRecursedType,
    bool wildcardMatching,
    Int32 codePage)
{
  const char *errorMessage = NULL;
  unsigned i;
  for (i = 0; i < strings.Size(); i++)
  {
    const UString &name = strings[i];
    NRecursedType::EEnum recursedType;
    unsigned pos = 0;
    
    if (name.Len() < kSomeCludePostStringMinSize)
    {
      errorMessage = "Too short switch";
      break;
    }
    
    if (::MyCharLower_Ascii(name[pos]) == kRecursedIDChar)
    {
      pos++;
      wchar_t c = name[pos];
      int index = -1;
      if (c <= 0x7F)
        index = FindCharPosInString(kRecursedPostCharSet, (char)c);
      recursedType = GetRecursedTypeFromIndex(index);
      if (index >= 0)
        pos++;
    }
    else
      recursedType = commonRecursedType;
    
    if (name.Len() < pos + kSomeCludeAfterRecursedPostStringMinSize)
    {
      errorMessage = "Too short switch";
      break;
    }
    
    UString tail = name.Ptr(pos + 1);
    
    if (name[pos] == kImmediateNameID)
      AddNameToCensor(censor, tail, include, recursedType, wildcardMatching);
    else if (name[pos] == kFileListID)
      AddToCensorFromListFile(NULL, censor, tail, include, recursedType, wildcardMatching, codePage);
    #ifdef _WIN32
    else if (name[pos] == kMapNameID)
    {
      errorMessage = ParseMapWithPaths(censor, tail, include, recursedType, wildcardMatching);
      if (errorMessage)
        break;
    }
    #endif
    else
    {
      errorMessage = "Incorrect wildcard type marker";
      break;
    }
  }
  if (i != strings.Size())
    throw CArcCmdLineException(errorMessage, strings[i]);
}

#ifdef _WIN32

// This code converts all short file names to long file names.

static void ConvertToLongName(const UString &prefix, UString &name)
{
  if (name.IsEmpty() || DoesNameContainWildcard(name))
    return;
  NFind::CFileInfo fi;
  const FString path = us2fs(prefix + name);
  #ifndef UNDER_CE
  if (NFile::NName::IsDevicePath(path))
    return;
  #endif
  if (fi.Find(path))
    name = fs2us(fi.Name);
}

static void ConvertToLongNames(const UString &prefix, CObjectVector<NWildcard::CItem> &items)
{
  FOR_VECTOR (i, items)
  {
    NWildcard::CItem &item = items[i];
    if (item.Recursive || item.PathParts.Size() != 1)
      continue;
    if (prefix.IsEmpty() && item.IsDriveItem())
      continue;
    ConvertToLongName(prefix, item.PathParts.Front());
  }
}

static void ConvertToLongNames(const UString &prefix, NWildcard::CCensorNode &node)
{
  ConvertToLongNames(prefix, node.IncludeItems);
  ConvertToLongNames(prefix, node.ExcludeItems);
  unsigned i;
  for (i = 0; i < node.SubNodes.Size(); i++)
  {
    UString &name = node.SubNodes[i].Name;
    if (prefix.IsEmpty() && NWildcard::IsDriveColonName(name))
      continue;
    ConvertToLongName(prefix, name);
  }
  // mix folders with same name
  for (i = 0; i < node.SubNodes.Size(); i++)
  {
    NWildcard::CCensorNode &nextNode1 = node.SubNodes[i];
    for (unsigned j = i + 1; j < node.SubNodes.Size();)
    {
      const NWildcard::CCensorNode &nextNode2 = node.SubNodes[j];
      if (nextNode1.Name.IsEqualTo_NoCase(nextNode2.Name))
      {
        nextNode1.IncludeItems += nextNode2.IncludeItems;
        nextNode1.ExcludeItems += nextNode2.ExcludeItems;
        node.SubNodes.Delete(j);
      }
      else
        j++;
    }
  }
  for (i = 0; i < node.SubNodes.Size(); i++)
  {
    NWildcard::CCensorNode &nextNode = node.SubNodes[i];
    ConvertToLongNames(prefix + nextNode.Name + WCHAR_PATH_SEPARATOR, nextNode);
  }
}

void ConvertToLongNames(NWildcard::CCensor &censor)
{
  FOR_VECTOR (i, censor.Pairs)
  {
    NWildcard::CPair &pair = censor.Pairs[i];
    ConvertToLongNames(pair.Prefix, pair.Head);
  }
}

#endif

/*
static NUpdateArchive::NPairAction::EEnum GetUpdatePairActionType(int i)
{
  switch (i)
  {
    case NUpdateArchive::NPairAction::kIgnore: return NUpdateArchive::NPairAction::kIgnore;
    case NUpdateArchive::NPairAction::kCopy: return NUpdateArchive::NPairAction::kCopy;
    case NUpdateArchive::NPairAction::kCompress: return NUpdateArchive::NPairAction::kCompress;
    case NUpdateArchive::NPairAction::kCompressAsAnti: return NUpdateArchive::NPairAction::kCompressAsAnti;
  }
  throw 98111603;
}
*/

static const wchar_t *kUpdatePairStateIDSet = L"pqrxyzw";
static const int kUpdatePairStateNotSupportedActions[] = {2, 2, 1, -1, -1, -1, -1};

static const unsigned kNumUpdatePairActions = 4;
static const char *kUpdateIgnoreItselfPostStringID = "-";
static const wchar_t kUpdateNewArchivePostCharID = '!';


static bool ParseUpdateCommandString2(const UString &command,
    NUpdateArchive::CActionSet &actionSet, UString &postString)
{
  for (unsigned i = 0; i < command.Len();)
  {
    wchar_t c = MyCharLower_Ascii(command[i]);
    int statePos = FindCharPosInString(kUpdatePairStateIDSet, c);
    if (statePos < 0)
    {
      postString = command.Ptr(i);
      return true;
    }
    i++;
    if (i >= command.Len())
      return false;
    c = command[i];
    if (c < '0' || c >= '0' + kNumUpdatePairActions)
      return false;
    unsigned actionPos = c - '0';
    actionSet.StateActions[(unsigned)statePos] = (NUpdateArchive::NPairAction::EEnum)(actionPos);
    if (kUpdatePairStateNotSupportedActions[(unsigned)statePos] == (int)actionPos)
      return false;
    i++;
  }
  postString.Empty();
  return true;
}

static void ParseUpdateCommandString(CUpdateOptions &options,
    const UStringVector &updatePostStrings,
    const NUpdateArchive::CActionSet &defaultActionSet)
{
  const char *errorMessage = "incorrect update switch command";
  unsigned i;
  for (i = 0; i < updatePostStrings.Size(); i++)
  {
    const UString &updateString = updatePostStrings[i];
    if (updateString.IsEqualTo(kUpdateIgnoreItselfPostStringID))
    {
      if (options.UpdateArchiveItself)
      {
        options.UpdateArchiveItself = false;
        options.Commands.Delete(0);
      }
    }
    else
    {
      NUpdateArchive::CActionSet actionSet = defaultActionSet;

      UString postString;
      if (!ParseUpdateCommandString2(updateString, actionSet, postString))
        break;
      if (postString.IsEmpty())
      {
        if (options.UpdateArchiveItself)
          options.Commands[0].ActionSet = actionSet;
      }
      else
      {
        if (postString[0] != kUpdateNewArchivePostCharID)
          break;
        CUpdateArchiveCommand uc;
        UString archivePath = postString.Ptr(1);
        if (archivePath.IsEmpty())
          break;
        uc.UserArchivePath = archivePath;
        uc.ActionSet = actionSet;
        options.Commands.Add(uc);
      }
    }
  }
  if (i != updatePostStrings.Size())
    throw CArcCmdLineException(errorMessage, updatePostStrings[i]);
}

bool ParseComplexSize(const wchar_t *s, UInt64 &result);

static void SetAddCommandOptions(
    NCommandType::EEnum commandType,
    const CParser &parser,
    CUpdateOptions &options)
{
  NUpdateArchive::CActionSet defaultActionSet;
  switch (commandType)
  {
    case NCommandType::kAdd:
      defaultActionSet = NUpdateArchive::k_ActionSet_Add;
      break;
    case NCommandType::kDelete:
      defaultActionSet = NUpdateArchive::k_ActionSet_Delete;
      break;
    default:
      defaultActionSet = NUpdateArchive::k_ActionSet_Update;
  }
  
  options.UpdateArchiveItself = true;
  
  options.Commands.Clear();
  CUpdateArchiveCommand updateMainCommand;
  updateMainCommand.ActionSet = defaultActionSet;
  options.Commands.Add(updateMainCommand);
  if (parser[NKey::kUpdate].ThereIs)
    ParseUpdateCommandString(options, parser[NKey::kUpdate].PostStrings,
        defaultActionSet);
  if (parser[NKey::kWorkingDir].ThereIs)
  {
    const UString &postString = parser[NKey::kWorkingDir].PostStrings[0];
    if (postString.IsEmpty())
      NDir::MyGetTempPath(options.WorkingDir);
    else
      options.WorkingDir = us2fs(postString);
  }
  options.SfxMode = parser[NKey::kSfx].ThereIs;
  if (options.SfxMode)
    options.SfxModule = us2fs(parser[NKey::kSfx].PostStrings[0]);

  if (parser[NKey::kVolume].ThereIs)
  {
    const UStringVector &sv = parser[NKey::kVolume].PostStrings;
    FOR_VECTOR (i, sv)
    {
      UInt64 size;
      if (!ParseComplexSize(sv[i], size) || size == 0)
        throw CArcCmdLineException("Incorrect volume size:", sv[i]);
      options.VolumesSizes.Add(size);
    }
  }
}

static void SetMethodOptions(const CParser &parser, CObjectVector<CProperty> &properties)
{
  if (parser[NKey::kProperty].ThereIs)
  {
    FOR_VECTOR (i, parser[NKey::kProperty].PostStrings)
    {
      CProperty prop;
      prop.Name = parser[NKey::kProperty].PostStrings[i];
      int index = prop.Name.Find(L'=');
      if (index >= 0)
      {
        prop.Value = prop.Name.Ptr(index + 1);
        prop.Name.DeleteFrom(index);
      }
      properties.Add(prop);
    }
  }
}

CArcCmdLineParser::CArcCmdLineParser(): parser(ARRAY_SIZE(kSwitchForms)) {}

static inline void SetStreamMode(const CSwitchResult &sw, unsigned &res)
{
  if (sw.ThereIs)
    res = sw.PostCharIndex;
}

void CArcCmdLineParser::Parse1(const UStringVector &commandStrings,
    CArcCmdLineOptions &options)
{
  if (!parser.ParseStrings(kSwitchForms, commandStrings))
    throw CArcCmdLineException(parser.ErrorMessage, parser.ErrorLine);

  options.IsInTerminal = MY_IS_TERMINAL(stdin);
  options.IsStdOutTerminal = MY_IS_TERMINAL(stdout);
  options.IsStdErrTerminal = MY_IS_TERMINAL(stderr);

  options.HelpMode = parser[NKey::kHelp1].ThereIs || parser[NKey::kHelp2].ThereIs  || parser[NKey::kHelp3].ThereIs;

  options.StdInMode = parser[NKey::kStdIn].ThereIs;
  options.StdOutMode = parser[NKey::kStdOut].ThereIs;
  options.EnableHeaders = !parser[NKey::kDisableHeaders].ThereIs;
  options.TechMode = parser[NKey::kTechMode].ThereIs;
  options.ShowTime = parser[NKey::kShowTime].ThereIs;

  if (parser[NKey::kDisablePercents].ThereIs
      || options.StdOutMode
      || !options.IsStdOutTerminal)
    options.Number_for_Percents = k_OutStream_disabled;

  if (options.StdOutMode)
    options.Number_for_Out = k_OutStream_disabled;

  SetStreamMode(parser[NKey::kOutStream], options.Number_for_Out);
  SetStreamMode(parser[NKey::kErrStream], options.Number_for_Errors);
  SetStreamMode(parser[NKey::kPercentStream], options.Number_for_Percents);

  if (parser[NKey::kLogLevel].ThereIs)
  {
    const UString &s = parser[NKey::kLogLevel].PostStrings[0];
    if (s.IsEmpty())
      options.LogLevel = 1;
    else
    {
      UInt32 v;
      if (!StringToUInt32(s, v))
        throw CArcCmdLineException("Unsupported switch postfix -bb", s);
      options.LogLevel = (unsigned)v;
    }
  }

  if (parser[NKey::kCaseSensitive].ThereIs)
  {
    g_CaseSensitive = !parser[NKey::kCaseSensitive].WithMinus;
    options.CaseSensitiveChange = true;
    options.CaseSensitive = g_CaseSensitive;
  }

  options.LargePages = false;
  if (parser[NKey::kLargePages].ThereIs)
    options.LargePages = !parser[NKey::kLargePages].WithMinus;


  #ifndef UNDER_CE

  if (parser[NKey::kAffinity].ThereIs)
  {
    const UString &s = parser[NKey::kAffinity].PostStrings[0];
    if (!s.IsEmpty())
    {
      UInt32 v = 0;
      AString a;
      a.SetFromWStr_if_Ascii(s);
      if (!a.IsEmpty())
      {
        const char *end;
        v = ConvertHexStringToUInt32(a, &end);
        if (*end != 0)
          a.Empty();
      }
      if (a.IsEmpty())
        throw CArcCmdLineException("Unsupported switch postfix -stm", s);
      
      #ifdef _WIN32
      SetProcessAffinityMask(GetCurrentProcess(), v);
      #endif
    }
  }

  #endif
}

struct CCodePagePair
{
  const char *Name;
  Int32 CodePage;
};

static const unsigned kNumByteOnlyCodePages = 3;

static const CCodePagePair g_CodePagePairs[] =
{
  { "utf-8", CP_UTF8 },
  { "win", CP_ACP },
  { "dos", CP_OEMCP },
  { "utf-16le", MY__CP_UTF16 },
  { "utf-16be", MY__CP_UTF16BE }
};

static Int32 FindCharset(const NCommandLineParser::CParser &parser, unsigned keyIndex,
    bool byteOnlyCodePages, Int32 defaultVal)
{
  if (!parser[keyIndex].ThereIs)
    return defaultVal;

  UString name = parser[keyIndex].PostStrings.Back();
  UInt32 v;
  if (StringToUInt32(name, v))
    if (v < ((UInt32)1 << 16))
      return (Int32)v;
  name.MakeLower_Ascii();
  unsigned num = byteOnlyCodePages ? kNumByteOnlyCodePages : ARRAY_SIZE(g_CodePagePairs);
  for (unsigned i = 0;; i++)
  {
    if (i == num) // to disable warnings from different compilers
      throw CArcCmdLineException("Unsupported charset:", name);
    const CCodePagePair &pair = g_CodePagePairs[i];
    if (name.IsEqualTo(pair.Name))
      return pair.CodePage;
  }
}

HRESULT EnumerateDirItemsAndSort(
    NWildcard::CCensor &censor,
    NWildcard::ECensorPathMode censorPathMode,
    const UString &addPathPrefix,
    UStringVector &sortedPaths,
    UStringVector &sortedFullPaths,
    CDirItemsStat &st,
    IDirItemsCallback *callback)
{
  FStringVector paths;
  
  {
    CDirItems dirItems;
    dirItems.Callback = callback;
    {
      HRESULT res = EnumerateItems(censor, censorPathMode, addPathPrefix, dirItems);
      st = dirItems.Stat;
      RINOK(res);
    }
  
    FOR_VECTOR (i, dirItems.Items)
    {
      const CDirItem &dirItem = dirItems.Items[i];
      if (!dirItem.IsDir())
        paths.Add(dirItems.GetPhyPath(i));
    }
  }
  
  if (paths.Size() == 0)
    throw CArcCmdLineException(kCannotFindArchive);
  
  UStringVector fullPaths;
  
  unsigned i;
  
  for (i = 0; i < paths.Size(); i++)
  {
    FString fullPath;
    NFile::NDir::MyGetFullPathName(paths[i], fullPath);
    fullPaths.Add(fs2us(fullPath));
  }
  
  CUIntVector indices;
  SortFileNames(fullPaths, indices);
  sortedPaths.ClearAndReserve(indices.Size());
  sortedFullPaths.ClearAndReserve(indices.Size());

  for (i = 0; i < indices.Size(); i++)
  {
    unsigned index = indices[i];
    sortedPaths.AddInReserved(fs2us(paths[index]));
    sortedFullPaths.AddInReserved(fullPaths[index]);
    if (i > 0 && CompareFileNames(sortedFullPaths[i], sortedFullPaths[i - 1]) == 0)
      throw CArcCmdLineException("Duplicate archive path:", sortedFullPaths[i]);
  }

  return S_OK;
}

static void SetBoolPair(NCommandLineParser::CParser &parser, unsigned switchID, CBoolPair &bp)
{
  bp.Def = parser[switchID].ThereIs;
  if (bp.Def)
    bp.Val = !parser[switchID].WithMinus;
}

void CArcCmdLineParser::Parse2(CArcCmdLineOptions &options)
{
  const UStringVector &nonSwitchStrings = parser.NonSwitchStrings;
  unsigned numNonSwitchStrings = nonSwitchStrings.Size();
  if (numNonSwitchStrings < kMinNonSwitchWords)
    throw CArcCmdLineException("The command must be specified");

  if (!ParseArchiveCommand(nonSwitchStrings[kCommandIndex], options.Command))
    throw CArcCmdLineException("Unsupported command:", nonSwitchStrings[kCommandIndex]);

  if (parser[NKey::kHash].ThereIs)
    options.HashMethods = parser[NKey::kHash].PostStrings;
  
  if (parser[NKey::kElimDup].ThereIs)
  {
    options.ExtractOptions.ElimDup.Def = true;
    options.ExtractOptions.ElimDup.Val = !parser[NKey::kElimDup].WithMinus;
  }
  
  NWildcard::ECensorPathMode censorPathMode = NWildcard::k_RelatPath;
  bool fullPathMode = parser[NKey::kFullPathMode].ThereIs;
  if (fullPathMode)
  {
    censorPathMode = NWildcard::k_AbsPath;
    const UString &s = parser[NKey::kFullPathMode].PostStrings[0];
    if (!s.IsEmpty())
    {
      if (s == L"2")
        censorPathMode = NWildcard::k_FullPath;
      else
        throw CArcCmdLineException("Unsupported -spf:", s);
    }
  }

  NRecursedType::EEnum recursedType;
  if (parser[NKey::kRecursed].ThereIs)
    recursedType = GetRecursedTypeFromIndex(parser[NKey::kRecursed].PostCharIndex);
  else
    recursedType = NRecursedType::kNonRecursed;

  bool wildcardMatching = true;
  if (parser[NKey::kDisableWildcardParsing].ThereIs)
    wildcardMatching = false;

  g_CodePage = FindCharset(parser, NKey::kConsoleCharSet, true, -1);
  Int32 codePage = FindCharset(parser, NKey::kListfileCharSet, false, CP_UTF8);

  bool thereAreSwitchIncludes = false;
  
  if (parser[NKey::kInclude].ThereIs)
  {
    thereAreSwitchIncludes = true;
    AddSwitchWildcardsToCensor(options.Censor,
        parser[NKey::kInclude].PostStrings, true, recursedType, wildcardMatching, codePage);
  }

  if (parser[NKey::kExclude].ThereIs)
    AddSwitchWildcardsToCensor(options.Censor,
        parser[NKey::kExclude].PostStrings, false, recursedType, wildcardMatching, codePage);
 
  unsigned curCommandIndex = kCommandIndex + 1;
  bool thereIsArchiveName = !parser[NKey::kNoArName].ThereIs &&
      options.Command.CommandType != NCommandType::kBenchmark &&
      options.Command.CommandType != NCommandType::kInfo &&
      options.Command.CommandType != NCommandType::kHash;

  bool isExtractGroupCommand = options.Command.IsFromExtractGroup();
  bool isExtractOrList = isExtractGroupCommand || options.Command.CommandType == NCommandType::kList;
  bool isRename = options.Command.CommandType == NCommandType::kRename;

  if ((isExtractOrList || isRename) && options.StdInMode)
    thereIsArchiveName = false;

  if (parser[NKey::kArcNameMode].ThereIs)
    options.UpdateOptions.ArcNameMode = ParseArcNameMode(parser[NKey::kArcNameMode].PostCharIndex);

  if (thereIsArchiveName)
  {
    if (curCommandIndex >= numNonSwitchStrings)
      throw CArcCmdLineException("Cannot find archive name");
    options.ArchiveName = nonSwitchStrings[curCommandIndex++];
    if (options.ArchiveName.IsEmpty())
      throw CArcCmdLineException("Archive name cannot by empty");
    #ifdef _WIN32
    // options.ArchiveName.Replace(L'/', WCHAR_PATH_SEPARATOR);
    #endif
  }

  AddToCensorFromNonSwitchesStrings(isRename ? &options.UpdateOptions.RenamePairs : NULL,
      curCommandIndex, options.Censor,
      nonSwitchStrings, recursedType, wildcardMatching,
      thereAreSwitchIncludes, codePage);

  options.YesToAll = parser[NKey::kYes].ThereIs;


  #ifndef _NO_CRYPTO
  options.PasswordEnabled = parser[NKey::kPassword].ThereIs;
  if (options.PasswordEnabled)
    options.Password = parser[NKey::kPassword].PostStrings[0];
  #endif

  options.ShowDialog = parser[NKey::kShowDialog].ThereIs;

  if (parser[NKey::kArchiveType].ThereIs)
    options.ArcType = parser[NKey::kArchiveType].PostStrings[0];

  options.ExcludedArcTypes = parser[NKey::kExcludedArcType].PostStrings;

  SetMethodOptions(parser, options.Properties);

  if (parser[NKey::kNtSecurity].ThereIs) options.NtSecurity.SetTrueTrue();

  SetBoolPair(parser, NKey::kAltStreams, options.AltStreams);
  SetBoolPair(parser, NKey::kHardLinks, options.HardLinks);
  SetBoolPair(parser, NKey::kSymLinks, options.SymLinks);

  if (isExtractOrList)
  {
    CExtractOptionsBase &eo = options.ExtractOptions;

    {
      CExtractNtOptions &nt = eo.NtOptions;
      nt.NtSecurity = options.NtSecurity;

      nt.AltStreams = options.AltStreams;
      if (!options.AltStreams.Def)
        nt.AltStreams.Val = true;

      nt.HardLinks = options.HardLinks;
      if (!options.HardLinks.Def)
        nt.HardLinks.Val = true;

      nt.SymLinks = options.SymLinks;
      if (!options.SymLinks.Def)
        nt.SymLinks.Val = true;

      nt.ReplaceColonForAltStream = parser[NKey::kReplaceColonForAltStream].ThereIs;
      nt.WriteToAltStreamIfColon = parser[NKey::kWriteToAltStreamIfColon].ThereIs;
    }
      
    options.Censor.AddPathsToCensor(NWildcard::k_AbsPath);
    options.Censor.ExtendExclude();

    // are there paths that look as non-relative (!Prefix.IsEmpty())
    if (!options.Censor.AllAreRelative())
      throw CArcCmdLineException("Cannot use absolute pathnames for this command");

    NWildcard::CCensor &arcCensor = options.arcCensor;

    if (parser[NKey::kArInclude].ThereIs)
      AddSwitchWildcardsToCensor(arcCensor, parser[NKey::kArInclude].PostStrings, true, NRecursedType::kNonRecursed, wildcardMatching, codePage);
    if (parser[NKey::kArExclude].ThereIs)
      AddSwitchWildcardsToCensor(arcCensor, parser[NKey::kArExclude].PostStrings, false, NRecursedType::kNonRecursed, wildcardMatching, codePage);

    if (thereIsArchiveName)
      AddNameToCensor(arcCensor, options.ArchiveName, true, NRecursedType::kNonRecursed, wildcardMatching);

    arcCensor.AddPathsToCensor(NWildcard::k_RelatPath);

    #ifdef _WIN32
    ConvertToLongNames(arcCensor);
    #endif

    arcCensor.ExtendExclude();

    if (options.StdInMode)
      options.ArcName_for_StdInMode = parser[NKey::kStdIn].PostStrings.Front();
    
    if (isExtractGroupCommand)
    {
      if (options.StdOutMode)
      {
        if (
                  options.Number_for_Percents == k_OutStream_stdout
            // || options.Number_for_Out      == k_OutStream_stdout
            // || options.Number_for_Errors   == k_OutStream_stdout
            ||
            (
              (options.IsStdOutTerminal && options.IsStdErrTerminal)
              &&
              (
                      options.Number_for_Percents != k_OutStream_disabled
                // || options.Number_for_Out      != k_OutStream_disabled
                // || options.Number_for_Errors   != k_OutStream_disabled
              )
            )
           )
          throw CArcCmdLineException(kSameTerminalError);
      }
      
      if (parser[NKey::kOutputDir].ThereIs)
      {
        eo.OutputDir = us2fs(parser[NKey::kOutputDir].PostStrings[0]);
        NFile::NName::NormalizeDirPathPrefix(eo.OutputDir);
      }

      eo.OverwriteMode = NExtract::NOverwriteMode::kAsk;
      if (parser[NKey::kOverwrite].ThereIs)
      {
        eo.OverwriteMode = k_OverwriteModes[(unsigned)parser[NKey::kOverwrite].PostCharIndex];
        eo.OverwriteMode_Force = true;
      }
      else if (options.YesToAll)
      {
        eo.OverwriteMode = NExtract::NOverwriteMode::kOverwrite;
        eo.OverwriteMode_Force = true;
      }
    }

    eo.PathMode = options.Command.GetPathMode();
    if (censorPathMode == NWildcard::k_AbsPath)
    {
      eo.PathMode = NExtract::NPathMode::kAbsPaths;
      eo.PathMode_Force = true;
    }
    else if (censorPathMode == NWildcard::k_FullPath)
    {
      eo.PathMode = NExtract::NPathMode::kFullPaths;
      eo.PathMode_Force = true;
    }
  }
  else if (options.Command.IsFromUpdateGroup())
  {
    if (parser[NKey::kArInclude].ThereIs)
      throw CArcCmdLineException("-ai switch is not supported for this command");

    CUpdateOptions &updateOptions = options.UpdateOptions;

    SetAddCommandOptions(options.Command.CommandType, parser, updateOptions);
    
    updateOptions.MethodMode.Properties = options.Properties;

    if (parser[NKey::kShareForWrite].ThereIs)
      updateOptions.OpenShareForWrite = true;

    updateOptions.PathMode = censorPathMode;

    updateOptions.AltStreams = options.AltStreams;
    updateOptions.NtSecurity = options.NtSecurity;
    updateOptions.HardLinks = options.HardLinks;
    updateOptions.SymLinks = options.SymLinks;

    updateOptions.EMailMode = parser[NKey::kEmail].ThereIs;
    if (updateOptions.EMailMode)
    {
      updateOptions.EMailAddress = parser[NKey::kEmail].PostStrings.Front();
      if (updateOptions.EMailAddress.Len() > 0)
        if (updateOptions.EMailAddress[0] == L'.')
        {
          updateOptions.EMailRemoveAfter = true;
          updateOptions.EMailAddress.Delete(0);
        }
    }

    updateOptions.StdOutMode = options.StdOutMode;
    updateOptions.StdInMode = options.StdInMode;

    updateOptions.DeleteAfterCompressing = parser[NKey::kDeleteAfterCompressing].ThereIs;
    updateOptions.SetArcMTime = parser[NKey::kSetArcMTime].ThereIs;

    if (updateOptions.StdOutMode && updateOptions.EMailMode)
      throw CArcCmdLineException("stdout mode and email mode cannot be combined");
    
    if (updateOptions.StdOutMode)
    {
      if (options.IsStdOutTerminal)
        throw CArcCmdLineException(kTerminalOutError);
      
      if (options.Number_for_Percents == k_OutStream_stdout
          || options.Number_for_Out == k_OutStream_stdout
          || options.Number_for_Errors == k_OutStream_stdout)
        throw CArcCmdLineException(kSameTerminalError);
    }
    
    if (updateOptions.StdInMode)
      updateOptions.StdInFileName = parser[NKey::kStdIn].PostStrings.Front();

    if (options.Command.CommandType == NCommandType::kRename)
      if (updateOptions.Commands.Size() != 1)
        throw CArcCmdLineException("Only one archive can be created with rename command");
  }
  else if (options.Command.CommandType == NCommandType::kBenchmark)
  {
    options.NumIterations = 1;
    if (curCommandIndex < numNonSwitchStrings)
    {
      if (!StringToUInt32(nonSwitchStrings[curCommandIndex], options.NumIterations))
        throw CArcCmdLineException("Incorrect Number of benmchmark iterations", nonSwitchStrings[curCommandIndex]);
      curCommandIndex++;
    }
  }
  else if (options.Command.CommandType == NCommandType::kHash)
  {
    options.Censor.AddPathsToCensor(censorPathMode);
    options.Censor.ExtendExclude();

    CHashOptions &hashOptions = options.HashOptions;
    hashOptions.PathMode = censorPathMode;
    hashOptions.Methods = options.HashMethods;
    if (parser[NKey::kShareForWrite].ThereIs)
      hashOptions.OpenShareForWrite = true;
    hashOptions.StdInMode = options.StdInMode;
    hashOptions.AltStreamsMode = options.AltStreams.Val;
  }
  else if (options.Command.CommandType == NCommandType::kInfo)
  {
  }
  else
    throw 20150919;
}
