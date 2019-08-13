// Bench.h

#ifndef __7ZIP_BENCH_H
#define __7ZIP_BENCH_H

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

HRESULT Bench(
    DECL_EXTERNAL_CODECS_LOC_VARS
    IBenchPrintCallback *printCallback,
    IBenchCallback *benchCallback,
    const CObjectVector<CProperty> &props,
    UInt32 numIterations,
    bool multiDict
    );

#endif
