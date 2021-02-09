// Main.cpp

#include "StdAfx.h"

#include "../../../Common/MyWindows.h"

#ifdef _WIN32
#include <Psapi.h>
#endif

#include "../../../../C/CpuArch.h"

#include "../../../Common/MyInitGuid.h"

#include "../../../Common/CommandLineParser.h"
#include "../../../Common/IntToString.h"
#include "../../../Common/MyException.h"
#include "../../../Common/StringConvert.h"
#include "../../../Common/StringToInt.h"
#include "../../../Common/UTFConvert.h"

#include "../../../Windows/ErrorMsg.h"

#include "../../../Windows/TimeUtils.h"

#include "../Common/ArchiveCommandLine.h"
#include "../Common/Bench.h"
#include "../Common/ExitCode.h"
#include "../Common/Extract.h"

#ifdef EXTERNAL_CODECS
#include "../Common/LoadCodecs.h"
#endif

#include "../../Common/RegisterCodec.h"

#include "BenchCon.h"
#include "ConsoleClose.h"
#include "ExtractCallbackConsole.h"
#include "List.h"
#include "OpenCallbackConsole.h"
#include "UpdateCallbackConsole.h"

#include "HashCon.h"

#ifdef PROG_VARIANT_R
#include "../../../../C/7zVersion.h"
#else
#include "../../MyVersion.h"
#endif

using namespace NWindows;
using namespace NFile;
using namespace NCommandLineParser;

#ifdef _WIN32
HINSTANCE g_hInstance = 0;
#endif

extern CStdOutStream *g_StdStream;
extern CStdOutStream *g_ErrStream;

extern unsigned g_NumCodecs;
extern const CCodecInfo *g_Codecs[];

extern unsigned g_NumHashers;
extern const CHasherInfo *g_Hashers[];

static const char * const kCopyrightString = "\n7-Zip"
  #ifndef EXTERNAL_CODECS
    #ifdef PROG_VARIANT_R
      " (r)"
    #else
      " (a)"
    #endif
  #endif

  " " MY_VERSION_CPU
  " : " MY_COPYRIGHT_DATE "\n\n";

static const char * const kHelpString =
    "Usage: 7z"
#ifndef EXTERNAL_CODECS
#ifdef PROG_VARIANT_R
    "r"
#else
    "a"
#endif
#endif
    " <command> [<switches>...] <archive_name> [<file_names>...]\n"
    "\n"
    "<Commands>\n"
    "  a : Add files to archive\n"
    "  b : Benchmark\n"
    "  d : Delete files from archive\n"
    "  e : Extract files from archive (without using directory names)\n"
    "  h : Calculate hash values for files\n"
    "  i : Show information about supported formats\n"
    "  l : List contents of archive\n"
    "  rn : Rename files in archive\n"
    "  t : Test integrity of archive\n"
    "  u : Update files to archive\n"
    "  x : eXtract files with full paths\n"
    "\n"
    "<Switches>\n"
    "  -- : Stop switches parsing\n"
    "  @listfile : set path to listfile that contains file names\n"
    "  -ai[r[-|0]]{@listfile|!wildcard} : Include archives\n"
    "  -ax[r[-|0]]{@listfile|!wildcard} : eXclude archives\n"
    "  -ao{a|s|t|u} : set Overwrite mode\n"
    "  -an : disable archive_name field\n"
    "  -bb[0-3] : set output log level\n"
    "  -bd : disable progress indicator\n"
    "  -bs{o|e|p}{0|1|2} : set output stream for output/error/progress line\n"
    "  -bt : show execution time statistics\n"
    "  -i[r[-|0]]{@listfile|!wildcard} : Include filenames\n"
    "  -m{Parameters} : set compression Method\n"
    "    -mmt[N] : set number of CPU threads\n"
    "    -mx[N] : set compression level: -mx1 (fastest) ... -mx9 (ultra)\n"
    "  -o{Directory} : set Output directory\n"
    #ifndef _NO_CRYPTO
    "  -p{Password} : set Password\n"
    #endif
    "  -r[-|0] : Recurse subdirectories\n"
    "  -sa{a|e|s} : set Archive name mode\n"
    "  -scc{UTF-8|WIN|DOS} : set charset for for console input/output\n"
    "  -scs{UTF-8|UTF-16LE|UTF-16BE|WIN|DOS|{id}} : set charset for list files\n"
    "  -scrc[CRC32|CRC64|SHA1|SHA256|*] : set hash function for x, e, h commands\n"
    "  -sdel : delete files after compression\n"
    "  -seml[.] : send archive by email\n"
    "  -sfx[{name}] : Create SFX archive\n"
    "  -si[{name}] : read data from stdin\n"
    "  -slp : set Large Pages mode\n"
    "  -slt : show technical information for l (List) command\n"
    "  -snh : store hard links as links\n"
    "  -snl : store symbolic links as links\n"
    "  -sni : store NT security information\n"
    "  -sns[-] : store NTFS alternate streams\n"
    "  -so : write data to stdout\n"
    "  -spd : disable wildcard matching for file names\n"
    "  -spe : eliminate duplication of root folder for extract command\n"
    "  -spf : use fully qualified file paths\n"
    "  -ssc[-] : set sensitive case mode\n"
    "  -sse : stop archive creating, if it can't open some input file\n"
    "  -ssw : compress shared files\n"
    "  -stl : set archive timestamp from the most recently modified file\n"
    "  -stm{HexMask} : set CPU thread affinity mask (hexadecimal number)\n"
    "  -stx{Type} : exclude archive type\n"
    "  -t{Type} : Set type of archive\n"
    "  -u[-][p#][q#][r#][x#][y#][z#][!newArchiveName] : Update options\n"
    "  -v{Size}[b|k|m|g] : Create volumes\n"
    "  -w[{path}] : assign Work directory. Empty path means a temporary directory\n"
    "  -x[r[-|0]]{@listfile|!wildcard} : eXclude filenames\n"
    "  -y : assume Yes on all queries\n";

// ---------------------------
// exception messages

static const char * const kEverythingIsOk = "Everything is Ok";
static const char * const kUserErrorMessage = "Incorrect command line";
static const char * const kNoFormats = "7-Zip cannot find the code that works with archives.";
static const char * const kUnsupportedArcTypeMessage = "Unsupported archive type";
// static const char * const kUnsupportedUpdateArcType = "Can't create archive for that type";

#define kDefaultSfxModule "7zCon.sfx"

static void ShowMessageAndThrowException(LPCSTR message, NExitCode::EEnum code)
{
  if (g_ErrStream)
    *g_ErrStream << endl << "ERROR: " << message << endl;
  throw code;
}

#ifndef _WIN32
static void GetArguments(int numArgs, const char *args[], UStringVector &parts)
{
  parts.Clear();
  for (int i = 0; i < numArgs; i++)
  {
    UString s = MultiByteToUnicodeString(args[i]);
    parts.Add(s);
  }
}
#endif

static void ShowCopyrightAndHelp(CStdOutStream *so, bool needHelp)
{
  if (!so)
    return;
  *so << kCopyrightString;
  // *so << "# CPUs: " << (UInt64)NWindows::NSystem::GetNumberOfProcessors() << endl;
  if (needHelp)
    *so << kHelpString;
}


static void PrintStringRight(CStdOutStream &so, const char *s, unsigned size)
{
  unsigned len = MyStringLen(s);
  for (unsigned i = len; i < size; i++)
    so << ' ';
  so << s;
}

static void PrintUInt32(CStdOutStream &so, UInt32 val, unsigned size)
{
  char s[16];
  ConvertUInt32ToString(val, s);
  PrintStringRight(so, s, size);
}

static void PrintLibIndex(CStdOutStream &so, int libIndex)
{
  if (libIndex >= 0)
    PrintUInt32(so, libIndex, 2);
  else
    so << "  ";
  so << ' ';
}

static void PrintString(CStdOutStream &so, const UString &s, unsigned size)
{
  unsigned len = s.Len();
  so << s;
  for (unsigned i = len; i < size; i++)
    so << ' ';
}

static inline char GetHex(unsigned val)
{
  return (char)((val < 10) ? ('0' + val) : ('A' + (val - 10)));
}

static void PrintWarningsPaths(const CErrorPathCodes &pc, CStdOutStream &so)
{
  FOR_VECTOR(i, pc.Paths)
  {
    so.NormalizePrint_UString(fs2us(pc.Paths[i]));
    so << " : ";
    so << NError::MyFormatMessage(pc.Codes[i]) << endl;
  }
  so << "----------------" << endl;
}

static int WarningsCheck(HRESULT result, const CCallbackConsoleBase &callback,
    const CUpdateErrorInfo &errorInfo,
    CStdOutStream *so,
    CStdOutStream *se,
    bool showHeaders)
{
  int exitCode = NExitCode::kSuccess;
  
  if (callback.ScanErrors.Paths.Size() != 0)
  {
    if (se)
    {
      *se << endl;
      *se << "Scan WARNINGS for files and folders:" << endl << endl;
      PrintWarningsPaths(callback.ScanErrors, *se);
      *se << "Scan WARNINGS: " << callback.ScanErrors.Paths.Size();
      *se << endl;
    }
    exitCode = NExitCode::kWarning;
  }
  
  if (result != S_OK || errorInfo.ThereIsError())
  {
    if (se)
    {
      UString message;
      if (!errorInfo.Message.IsEmpty())
      {
        message += errorInfo.Message.Ptr();
        message.Add_LF();
      }
      {
        FOR_VECTOR(i, errorInfo.FileNames)
        {
          message += fs2us(errorInfo.FileNames[i]);
          message.Add_LF();
        }
      }
      if (errorInfo.SystemError != 0)
      {
        message += NError::MyFormatMessage(errorInfo.SystemError);
        message.Add_LF();
      }
      if (!message.IsEmpty())
        *se << L"\nError:\n" << message;
    }

    // we will work with (result) later
    // throw CSystemException(result);
    return NExitCode::kFatalError;
  }

  unsigned numErrors = callback.FailedFiles.Paths.Size();
  if (numErrors == 0)
  {
    if (showHeaders)
      if (callback.ScanErrors.Paths.Size() == 0)
        if (so)
        {
          if (se)
            se->Flush();
          *so << kEverythingIsOk << endl;
        }
  }
  else
  {
    if (se)
    {
      *se << endl;
      *se << "WARNINGS for files:" << endl << endl;
      PrintWarningsPaths(callback.FailedFiles, *se);
      *se << "WARNING: Cannot open " << numErrors << " file";
      if (numErrors > 1)
        *se << 's';
      *se << endl;
    }
    exitCode = NExitCode::kWarning;
  }
  
  return exitCode;
}

static void ThrowException_if_Error(HRESULT res)
{
  if (res != S_OK)
    throw CSystemException(res);
}


static void PrintNum(UInt64 val, unsigned numDigits, char c = ' ')
{
  char temp[64];
  char *p = temp + 32;
  ConvertUInt64ToString(val, p);
  unsigned len = MyStringLen(p);
  for (; len < numDigits; len++)
    *--p = c;
  *g_StdStream << p;
}

static void PrintTime(const char *s, UInt64 val, UInt64 total)
{
  *g_StdStream << endl << s << " Time =";
  const UInt32 kFreq = 10000000;
  UInt64 sec = val / kFreq;
  PrintNum(sec, 6);
  *g_StdStream << '.';
  UInt32 ms = (UInt32)(val - (sec * kFreq)) / (kFreq / 1000);
  PrintNum(ms, 3, '0');
  
  while (val > ((UInt64)1 << 56))
  {
    val >>= 1;
    total >>= 1;
  }

  UInt64 percent = 0;
  if (total != 0)
    percent = val * 100 / total;
  *g_StdStream << " =";
  PrintNum(percent, 5);
  *g_StdStream << '%';
}

#ifndef UNDER_CE

#define SHIFT_SIZE_VALUE(x, num) (((x) + (1 << (num)) - 1) >> (num))

static void PrintMemUsage(const char *s, UInt64 val)
{
  *g_StdStream << "    " << s << " Memory =";
  PrintNum(SHIFT_SIZE_VALUE(val, 20), 7);
  *g_StdStream << " MB";

  #ifdef _7ZIP_LARGE_PAGES
  AString lp;
  Add_LargePages_String(lp);
  if (!lp.IsEmpty())
    *g_StdStream << lp;
  #endif
}

EXTERN_C_BEGIN
typedef BOOL (WINAPI *Func_GetProcessMemoryInfo)(HANDLE Process,
    PPROCESS_MEMORY_COUNTERS ppsmemCounters, DWORD cb);
typedef BOOL (WINAPI *Func_QueryProcessCycleTime)(HANDLE Process, PULONG64 CycleTime);
EXTERN_C_END

#endif

static inline UInt64 GetTime64(const FILETIME &t) { return ((UInt64)t.dwHighDateTime << 32) | t.dwLowDateTime; }

static void PrintStat()
{
  FILETIME creationTimeFT, exitTimeFT, kernelTimeFT, userTimeFT;
  if (!
      #ifdef UNDER_CE
        ::GetThreadTimes(::GetCurrentThread()
      #else
        // NT 3.5
        ::GetProcessTimes(::GetCurrentProcess()
      #endif
      , &creationTimeFT, &exitTimeFT, &kernelTimeFT, &userTimeFT))
    return;
  FILETIME curTimeFT;
  NTime::GetCurUtcFileTime(curTimeFT);

  #ifndef UNDER_CE
  
  PROCESS_MEMORY_COUNTERS m;
  memset(&m, 0, sizeof(m));
  BOOL memDefined = FALSE;
  BOOL cycleDefined = FALSE;
  ULONG64 cycleTime = 0;
  {
    /* NT 4.0: GetProcessMemoryInfo() in Psapi.dll
       Win7: new function K32GetProcessMemoryInfo() in kernel32.dll
       It's faster to call kernel32.dll code than Psapi.dll code
       GetProcessMemoryInfo() requires Psapi.lib
       Psapi.lib in SDK7+ can link to K32GetProcessMemoryInfo in kernel32.dll
       The program with K32GetProcessMemoryInfo will not work on systems before Win7
       // memDefined = GetProcessMemoryInfo(GetCurrentProcess(), &m, sizeof(m));
    */

    HMODULE kern = ::GetModuleHandleW(L"kernel32.dll");
    Func_GetProcessMemoryInfo my_GetProcessMemoryInfo = (Func_GetProcessMemoryInfo)
        ::GetProcAddress(kern, "K32GetProcessMemoryInfo");
    if (!my_GetProcessMemoryInfo)
    {
      HMODULE lib = LoadLibraryW(L"Psapi.dll");
      if (lib)
        my_GetProcessMemoryInfo = (Func_GetProcessMemoryInfo)::GetProcAddress(lib, "GetProcessMemoryInfo");
    }
    if (my_GetProcessMemoryInfo)
      memDefined = my_GetProcessMemoryInfo(GetCurrentProcess(), &m, sizeof(m));
    // FreeLibrary(lib);

    Func_QueryProcessCycleTime my_QueryProcessCycleTime = (Func_QueryProcessCycleTime)
        ::GetProcAddress(kern, "QueryProcessCycleTime");
    if (my_QueryProcessCycleTime)
      cycleDefined = my_QueryProcessCycleTime(GetCurrentProcess(), &cycleTime);
  }

  #endif

  UInt64 curTime = GetTime64(curTimeFT);
  UInt64 creationTime = GetTime64(creationTimeFT);
  UInt64 kernelTime = GetTime64(kernelTimeFT);
  UInt64 userTime = GetTime64(userTimeFT);

  UInt64 totalTime = curTime - creationTime;
  
  PrintTime("Kernel ", kernelTime, totalTime);

  #ifndef UNDER_CE
  if (cycleDefined)
  {
    *g_StdStream << " ";
    PrintNum(cycleTime / 1000000, 22);
    *g_StdStream << " MCycles";
  }
  #endif

  PrintTime("User   ", userTime, totalTime);
  
  PrintTime("Process", kernelTime + userTime, totalTime);
  #ifndef UNDER_CE
  if (memDefined) PrintMemUsage("Virtual ", m.PeakPagefileUsage);
  #endif
  
  PrintTime("Global ", totalTime, totalTime);
  #ifndef UNDER_CE
  if (memDefined) PrintMemUsage("Physical", m.PeakWorkingSetSize);
  #endif
  
  *g_StdStream << endl;
}

static void PrintHexId(CStdOutStream &so, UInt64 id)
{
  char s[32];
  ConvertUInt64ToHex(id, s);
  PrintStringRight(so, s, 8);
}


int Main2(
  #ifndef _WIN32
  int numArgs, char *args[]
  #endif
)
{
  #if defined(_WIN32) && !defined(UNDER_CE)
  SetFileApisToOEM();
  #endif

  UStringVector commandStrings;
  
  #ifdef _WIN32
  NCommandLineParser::SplitCommandLine(GetCommandLineW(), commandStrings);
  #else
  GetArguments(numArgs, args, commandStrings);
  #endif

  #ifndef UNDER_CE
  if (commandStrings.Size() > 0)
    commandStrings.Delete(0);
  #endif

  if (commandStrings.Size() == 0)
  {
    ShowCopyrightAndHelp(g_StdStream, true);
    return 0;
  }

  CArcCmdLineOptions options;

  CArcCmdLineParser parser;

  parser.Parse1(commandStrings, options);

  g_StdOut.IsTerminalMode = options.IsStdOutTerminal;
  g_StdErr.IsTerminalMode = options.IsStdErrTerminal;

  if (options.Number_for_Out != k_OutStream_stdout)
    g_StdStream = (options.Number_for_Out == k_OutStream_stderr ? &g_StdErr : NULL);

  if (options.Number_for_Errors != k_OutStream_stderr)
    g_ErrStream = (options.Number_for_Errors == k_OutStream_stdout ? &g_StdOut : NULL);

  CStdOutStream *percentsStream = NULL;
  if (options.Number_for_Percents != k_OutStream_disabled)
    percentsStream = (options.Number_for_Percents == k_OutStream_stderr) ? &g_StdErr : &g_StdOut;;
  
  if (options.HelpMode)
  {
    ShowCopyrightAndHelp(g_StdStream, true);
    return 0;
  }

  if (options.EnableHeaders)
    ShowCopyrightAndHelp(g_StdStream, false);

  parser.Parse2(options);

  unsigned percentsNameLevel = 1;
  if (options.LogLevel == 0 || options.Number_for_Percents != options.Number_for_Out)
    percentsNameLevel = 2;

  unsigned consoleWidth = 80;

  if (percentsStream)
  {
    #ifdef _WIN32
    
    #if !defined(UNDER_CE)
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &consoleInfo))
      consoleWidth = consoleInfo.dwSize.X;
    #endif
    
    #else
    
    struct winsize w;
    if (ioctl(0, TIOCGWINSZ, &w) == )
      consoleWidth = w.ws_col;
    
    #endif
  }

  CREATE_CODECS_OBJECT

  codecs->CaseSensitiveChange = options.CaseSensitiveChange;
  codecs->CaseSensitive = options.CaseSensitive;
  ThrowException_if_Error(codecs->Load());

  bool isExtractGroupCommand = options.Command.IsFromExtractGroup();

  if (codecs->Formats.Size() == 0 &&
        (isExtractGroupCommand
        || options.Command.CommandType == NCommandType::kList
        || options.Command.IsFromUpdateGroup()))
  {
    #ifdef EXTERNAL_CODECS
    if (!codecs->MainDll_ErrorPath.IsEmpty())
    {
      UString s ("Can't load module: ");
      s += fs2us(codecs->MainDll_ErrorPath);
      throw s;
    }
    #endif
    
    throw kNoFormats;
  }

  CObjectVector<COpenType> types;
  if (!ParseOpenTypes(*codecs, options.ArcType, types))
    throw kUnsupportedArcTypeMessage;

  CIntVector excludedFormats;
  FOR_VECTOR (k, options.ExcludedArcTypes)
  {
    CIntVector tempIndices;
    if (!codecs->FindFormatForArchiveType(options.ExcludedArcTypes[k], tempIndices)
        || tempIndices.Size() != 1)
      throw kUnsupportedArcTypeMessage;
    excludedFormats.AddToUniqueSorted(tempIndices[0]);
    // excludedFormats.Sort();
  }

  
  #ifdef EXTERNAL_CODECS
  if (isExtractGroupCommand
      || options.Command.CommandType == NCommandType::kHash
      || options.Command.CommandType == NCommandType::kBenchmark)
    ThrowException_if_Error(__externalCodecs.Load());
  #endif

  int retCode = NExitCode::kSuccess;
  HRESULT hresultMain = S_OK;

  // bool showStat = options.ShowTime;
  
  /*
  if (!options.EnableHeaders ||
      options.TechMode)
    showStat = false;
  */
  

  if (options.Command.CommandType == NCommandType::kInfo)
  {
    CStdOutStream &so = (g_StdStream ? *g_StdStream : g_StdOut);
    unsigned i;

    #ifdef EXTERNAL_CODECS
    so << endl << "Libs:" << endl;
    for (i = 0; i < codecs->Libs.Size(); i++)
    {
      PrintLibIndex(so, i);
      so << ' ' << codecs->Libs[i].Path << endl;
    }
    #endif

    so << endl << "Formats:" << endl;
    
    const char * const kArcFlags = "KSNFMGOPBELH";
    const unsigned kNumArcFlags = (unsigned)strlen(kArcFlags);
    
    for (i = 0; i < codecs->Formats.Size(); i++)
    {
      const CArcInfoEx &arc = codecs->Formats[i];

      #ifdef EXTERNAL_CODECS
      PrintLibIndex(so, arc.LibIndex);
      #else
      so << "  ";
      #endif

      so << (char)(arc.UpdateEnabled ? 'C' : ' ');
      
      for (unsigned b = 0; b < kNumArcFlags; b++)
      {
        so << (char)
          ((arc.Flags & ((UInt32)1 << b)) != 0 ? kArcFlags[b] : ' ');
      }
        
      so << ' ';
      PrintString(so, arc.Name, 8);
      so << ' ';
      UString s;
      
      FOR_VECTOR (t, arc.Exts)
      {
        if (t != 0)
          s.Add_Space();
        const CArcExtInfo &ext = arc.Exts[t];
        s += ext.Ext;
        if (!ext.AddExt.IsEmpty())
        {
          s += " (";
          s += ext.AddExt;
          s += ')';
        }
      }
      
      PrintString(so, s, 13);
      so << ' ';
      
      if (arc.SignatureOffset != 0)
        so << "offset=" << arc.SignatureOffset << ' ';

      FOR_VECTOR(si, arc.Signatures)
      {
        if (si != 0)
          so << "  ||  ";

        const CByteBuffer &sig = arc.Signatures[si];
        
        for (size_t j = 0; j < sig.Size(); j++)
        {
          if (j != 0)
            so << ' ';
          Byte b = sig[j];
          if (b > 0x20 && b < 0x80)
          {
            so << (char)b;
          }
          else
          {
            so << GetHex((b >> 4) & 0xF);
            so << GetHex(b & 0xF);
          }
        }
      }
      so << endl;
    }

    so << endl << "Codecs:" << endl; //  << "Lib          ID Name" << endl;

    for (i = 0; i < g_NumCodecs; i++)
    {
      const CCodecInfo &cod = *g_Codecs[i];

      PrintLibIndex(so, -1);

      if (cod.NumStreams == 1)
        so << ' ';
      else
        so << cod.NumStreams;
      
      so << (char)(cod.CreateEncoder ? 'E' : ' ');
      so << (char)(cod.CreateDecoder ? 'D' : ' ');

      so << ' ';
      PrintHexId(so, cod.Id);
      so << ' ' << cod.Name << endl;
    }


    #ifdef EXTERNAL_CODECS

    UInt32 numMethods;
    if (codecs->GetNumMethods(&numMethods) == S_OK)
    for (UInt32 j = 0; j < numMethods; j++)
    {
      PrintLibIndex(so, codecs->GetCodec_LibIndex(j));

      UInt32 numStreams = codecs->GetCodec_NumStreams(j);
      if (numStreams == 1)
        so << ' ';
      else
        so << numStreams;
      
      so << (char)(codecs->GetCodec_EncoderIsAssigned(j) ? 'E' : ' ');
      so << (char)(codecs->GetCodec_DecoderIsAssigned(j) ? 'D' : ' ');

      so << ' ';
      UInt64 id;
      HRESULT res = codecs->GetCodec_Id(j, id);
      if (res != S_OK)
        id = (UInt64)(Int64)-1;
      PrintHexId(so, id);
      so << ' ' << codecs->GetCodec_Name(j) << endl;
    }

    #endif
    

    so << endl << "Hashers:" << endl; //  << " L Size       ID Name" << endl;

    for (i = 0; i < g_NumHashers; i++)
    {
      const CHasherInfo &codec = *g_Hashers[i];
      PrintLibIndex(so, -1);
      PrintUInt32(so, codec.DigestSize, 4);
      so << ' ';
      PrintHexId(so, codec.Id);
      so << ' ' << codec.Name << endl;
    }

    #ifdef EXTERNAL_CODECS
    
    numMethods = codecs->GetNumHashers();
    for (UInt32 j = 0; j < numMethods; j++)
    {
      PrintLibIndex(so, codecs->GetHasherLibIndex(j));
      PrintUInt32(so, codecs->GetHasherDigestSize(j), 4);
      so << ' ';
      PrintHexId(so, codecs->GetHasherId(j));
      so << ' ' << codecs->GetHasherName(j) << endl;
    }

    #endif
    
  }
  else if (options.Command.CommandType == NCommandType::kBenchmark)
  {
    CStdOutStream &so = (g_StdStream ? *g_StdStream : g_StdOut);
    hresultMain = BenchCon(EXTERNAL_CODECS_VARS_L
        options.Properties, options.NumIterations, (FILE *)so);
    if (hresultMain == S_FALSE)
    {
      if (g_ErrStream)
        *g_ErrStream << "\nDecoding ERROR\n";
      retCode = NExitCode::kFatalError;
      hresultMain = S_OK;
    }
  }
  else if (isExtractGroupCommand || options.Command.CommandType == NCommandType::kList)
  {
    UStringVector ArchivePathsSorted;
    UStringVector ArchivePathsFullSorted;

    if (options.StdInMode)
    {
      ArchivePathsSorted.Add(options.ArcName_for_StdInMode);
      ArchivePathsFullSorted.Add(options.ArcName_for_StdInMode);
    }
    else
    {
      CExtractScanConsole scan;
      
      scan.Init(options.EnableHeaders ? g_StdStream : NULL, g_ErrStream, percentsStream);
      scan.SetWindowWidth(consoleWidth);

      if (g_StdStream && options.EnableHeaders)
        *g_StdStream << "Scanning the drive for archives:" << endl;

      CDirItemsStat st;

      scan.StartScanning();

      hresultMain = EnumerateDirItemsAndSort(
          options.arcCensor,
          NWildcard::k_RelatPath,
          UString(), // addPathPrefix
          ArchivePathsSorted,
          ArchivePathsFullSorted,
          st,
          &scan);

      scan.CloseScanning();

      if (hresultMain == S_OK)
      {
        if (options.EnableHeaders)
          scan.PrintStat(st);
      }
      else
      {
        /*
        if (res != E_ABORT)
        {
          throw CSystemException(res);
          // errorInfo.Message = "Scanning error";
        }
        return res;
        */
      }
    }

    if (hresultMain == S_OK)
    if (isExtractGroupCommand)
    {
      CExtractCallbackConsole *ecs = new CExtractCallbackConsole;
      CMyComPtr<IFolderArchiveExtractCallback> extractCallback = ecs;

      #ifndef _NO_CRYPTO
      ecs->PasswordIsDefined = options.PasswordEnabled;
      ecs->Password = options.Password;
      #endif

      ecs->Init(g_StdStream, g_ErrStream, percentsStream);
      ecs->MultiArcMode = (ArchivePathsSorted.Size() > 1);

      ecs->LogLevel = options.LogLevel;
      ecs->PercentsNameLevel = percentsNameLevel;
      
      if (percentsStream)
        ecs->SetWindowWidth(consoleWidth);

      /*
      COpenCallbackConsole openCallback;
      openCallback.Init(g_StdStream, g_ErrStream);

      #ifndef _NO_CRYPTO
      openCallback.PasswordIsDefined = options.PasswordEnabled;
      openCallback.Password = options.Password;
      #endif
      */

      CExtractOptions eo;
      (CExtractOptionsBase &)eo = options.ExtractOptions;
      
      eo.StdInMode = options.StdInMode;
      eo.StdOutMode = options.StdOutMode;
      eo.YesToAll = options.YesToAll;
      eo.TestMode = options.Command.IsTestCommand();
      
      #ifndef _SFX
      eo.Properties = options.Properties;
      #endif

      UString errorMessage;
      CDecompressStat stat;
      CHashBundle hb;
      IHashCalc *hashCalc = NULL;

      if (!options.HashMethods.IsEmpty())
      {
        hashCalc = &hb;
        ThrowException_if_Error(hb.SetMethods(EXTERNAL_CODECS_VARS_L options.HashMethods));
        // hb.Init();
      }
      
      hresultMain = Extract(
          codecs,
          types,
          excludedFormats,
          ArchivePathsSorted,
          ArchivePathsFullSorted,
          options.Censor.Pairs.Front().Head,
          eo, ecs, ecs, hashCalc, errorMessage, stat);
      
      ecs->ClosePercents();

      if (!errorMessage.IsEmpty())
      {
        if (g_ErrStream)
          *g_ErrStream << endl << "ERROR:" << endl << errorMessage << endl;
        if (hresultMain == S_OK)
          hresultMain = E_FAIL;
      }

      CStdOutStream *so = g_StdStream;

      bool isError = false;

      if (so)
      {
        *so << endl;
        
        if (ecs->NumTryArcs > 1)
        {
          *so << "Archives: " << ecs->NumTryArcs << endl;
          *so << "OK archives: " << ecs->NumOkArcs << endl;
        }
      }

      if (ecs->NumCantOpenArcs != 0)
      {
        isError = true;
        if (so)
          *so << "Can't open as archive: " << ecs->NumCantOpenArcs << endl;
      }
      
      if (ecs->NumArcsWithError != 0)
      {
        isError = true;
        if (so)
          *so << "Archives with Errors: " << ecs->NumArcsWithError << endl;
      }
      
      if (so)
      {
        if (ecs->NumArcsWithWarnings != 0)
          *so << "Archives with Warnings: " << ecs->NumArcsWithWarnings << endl;
        
        if (ecs->NumOpenArcWarnings != 0)
        {
          *so << endl;
          if (ecs->NumOpenArcWarnings != 0)
            *so << "Warnings: " << ecs->NumOpenArcWarnings << endl;
        }
      }
      
      if (ecs->NumOpenArcErrors != 0)
      {
        isError = true;
        if (so)
        {
          *so << endl;
          if (ecs->NumOpenArcErrors != 0)
            *so << "Open Errors: " << ecs->NumOpenArcErrors << endl;
        }
      }

      if (isError)
        retCode = NExitCode::kFatalError;
      
      if (so)
      if (ecs->NumArcsWithError != 0 || ecs->NumFileErrors != 0)
      {
        // if (ecs->NumArchives > 1)
        {
          *so << endl;
          if (ecs->NumFileErrors != 0)
            *so << "Sub items Errors: " << ecs->NumFileErrors << endl;
        }
      }
      else if (hresultMain == S_OK)
      {
        if (stat.NumFolders != 0)
          *so << "Folders: " << stat.NumFolders << endl;
        if (stat.NumFiles != 1 || stat.NumFolders != 0 || stat.NumAltStreams != 0)
          *so << "Files: " << stat.NumFiles << endl;
        if (stat.NumAltStreams != 0)
        {
          *so << "Alternate Streams: " << stat.NumAltStreams << endl;
          *so << "Alternate Streams Size: " << stat.AltStreams_UnpackSize << endl;
        }
        
        *so
          << "Size:       " << stat.UnpackSize << endl
          << "Compressed: " << stat.PackSize << endl;
        if (hashCalc)
        {
          *so << endl;
          PrintHashStat(*so, hb);
        }
      }
    }
    else
    {
      UInt64 numErrors = 0;
      UInt64 numWarnings = 0;
      
      // options.ExtractNtOptions.StoreAltStreams = true, if -sns[-] is not definmed

      hresultMain = ListArchives(
          codecs,
          types,
          excludedFormats,
          options.StdInMode,
          ArchivePathsSorted,
          ArchivePathsFullSorted,
          options.ExtractOptions.NtOptions.AltStreams.Val,
          options.AltStreams.Val, // we don't want to show AltStreams by default
          options.Censor.Pairs.Front().Head,
          options.EnableHeaders,
          options.TechMode,
          #ifndef _NO_CRYPTO
          options.PasswordEnabled,
          options.Password,
          #endif
          &options.Properties,
          numErrors, numWarnings);

      if (options.EnableHeaders)
        if (numWarnings > 0)
          g_StdOut << endl << "Warnings: " << numWarnings << endl;
      
      if (numErrors > 0)
      {
        if (options.EnableHeaders)
          g_StdOut << endl << "Errors: " << numErrors << endl;
        retCode = NExitCode::kFatalError;
      }
    }
  }
  else if (options.Command.IsFromUpdateGroup())
  {
    CUpdateOptions &uo = options.UpdateOptions;
    if (uo.SfxMode && uo.SfxModule.IsEmpty())
      uo.SfxModule = kDefaultSfxModule;

    COpenCallbackConsole openCallback;
    openCallback.Init(g_StdStream, g_ErrStream, percentsStream);

    #ifndef _NO_CRYPTO
    bool passwordIsDefined =
        (options.PasswordEnabled && !options.Password.IsEmpty());
    openCallback.PasswordIsDefined = passwordIsDefined;
    openCallback.Password = options.Password;
    #endif

    CUpdateCallbackConsole callback;
    callback.LogLevel = options.LogLevel;
    callback.PercentsNameLevel = percentsNameLevel;

    if (percentsStream)
      callback.SetWindowWidth(consoleWidth);

    #ifndef _NO_CRYPTO
    callback.PasswordIsDefined = passwordIsDefined;
    callback.AskPassword = (options.PasswordEnabled && options.Password.IsEmpty());
    callback.Password = options.Password;
    #endif

    callback.StdOutMode = uo.StdOutMode;
    callback.Init(
      // NULL,
      g_StdStream, g_ErrStream, percentsStream);

    CUpdateErrorInfo errorInfo;

    /*
    if (!uo.Init(codecs, types, options.ArchiveName))
      throw kUnsupportedUpdateArcType;
    */
    hresultMain = UpdateArchive(codecs,
        types,
        options.ArchiveName,
        options.Censor,
        uo,
        errorInfo, &openCallback, &callback, true);

    callback.ClosePercents2();

    CStdOutStream *se = g_StdStream;
    if (!se)
      se = g_ErrStream;

    retCode = WarningsCheck(hresultMain, callback, errorInfo,
        g_StdStream, se,
        true // options.EnableHeaders
        );
  }
  else if (options.Command.CommandType == NCommandType::kHash)
  {
    const CHashOptions &uo = options.HashOptions;

    CHashCallbackConsole callback;
    if (percentsStream)
      callback.SetWindowWidth(consoleWidth);
  
    callback.Init(g_StdStream, g_ErrStream, percentsStream);
    callback.PrintHeaders = options.EnableHeaders;

    AString errorInfoString;
    hresultMain = HashCalc(EXTERNAL_CODECS_VARS_L
        options.Censor, uo,
        errorInfoString, &callback);
    CUpdateErrorInfo errorInfo;
    errorInfo.Message = errorInfoString;
    CStdOutStream *se = g_StdStream;
    if (!se)
      se = g_ErrStream;
    retCode = WarningsCheck(hresultMain, callback, errorInfo, g_StdStream, se, options.EnableHeaders);
  }
  else
    ShowMessageAndThrowException(kUserErrorMessage, NExitCode::kUserError);

  if (options.ShowTime && g_StdStream)
    PrintStat();

  ThrowException_if_Error(hresultMain);

  return retCode;
}
