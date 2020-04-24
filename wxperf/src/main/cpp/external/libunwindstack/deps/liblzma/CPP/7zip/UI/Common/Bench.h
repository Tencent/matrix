// Bench.h

#ifndef __7ZIP_BENCH_H
#define __7ZIP_BENCH_H

#include "../../../Windows/System.h"

#include "../../Common/CreateCoder.h"
#include "../../UI/Common/Property.h"

struct CBenchInfo
{
  UInt64 GlobalTime;
  UInt64 GlobalFreq;
  UInt64 UserTime;
  UInt64 UserFreq;
  UInt64 UnpackSize;
  UInt64 PackSize;
  UInt64 NumIterations;
  
  CBenchInfo(): NumIterations(0) {}
  UInt64 GetUsage() const;
  UInt64 GetRatingPerUsage(UInt64 rating) const;
  UInt64 GetSpeed(UInt64 numCommands) const;
};

struct IBenchCallback
{
  virtual HRESULT SetFreq(bool showFreq, UInt64 cpuFreq) = 0;
  virtual HRESULT SetEncodeResult(const CBenchInfo &info, bool final) = 0;
  virtual HRESULT SetDecodeResult(const CBenchInfo &info, bool final) = 0;
};

UInt64 GetCompressRating(UInt32 dictSize, UInt64 elapsedTime, UInt64 freq, UInt64 size);
UInt64 GetDecompressRating(UInt64 elapsedTime, UInt64 freq, UInt64 outSize, UInt64 inSize, UInt64 numIterations);

const unsigned kBenchMinDicLogSize = 18;

UInt64 GetBenchMemoryUsage(UInt32 numThreads, UInt32 dictionary, bool totalBench = false);

struct IBenchPrintCallback
{
  virtual void Print(const char *s) = 0;
  virtual void NewLine() = 0;
  virtual HRESULT CheckBreak() = 0;
};

/*
struct IBenchFreqCallback
{
  virtual void AddCpuFreq(UInt64 freq) = 0;
};
*/

HRESULT Bench(
    DECL_EXTERNAL_CODECS_LOC_VARS
    IBenchPrintCallback *printCallback,
    IBenchCallback *benchCallback,
    // IBenchFreqCallback *freqCallback,
    const CObjectVector<CProperty> &props,
    UInt32 numIterations,
    bool multiDict
    );

AString GetProcessThreadsInfo(const NWindows::NSystem::CProcessAffinity &ti);

void GetSysInfo(AString &s1, AString &s2);
void GetCpuName(AString &s);
void GetCpuFeatures(AString &s);

#ifdef _7ZIP_LARGE_PAGES
void Add_LargePages_String(AString &s);
#else
// #define Add_LargePages_String
#endif

#endif
