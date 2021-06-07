// Bench.cpp

#include "StdAfx.h"

#include <stdio.h>

#ifndef _WIN32
#define USE_POSIX_TIME
#define USE_POSIX_TIME2
#endif

#ifdef USE_POSIX_TIME
#include <time.h>
#ifdef USE_POSIX_TIME2
#include <sys/time.h>
#endif
#endif

#ifdef _WIN32
#define USE_ALLOCA
#endif

#ifdef USE_ALLOCA
#ifdef _WIN32
#include <malloc.h>
#else
#include <stdlib.h>
#endif
#endif

#include "../../../../C/7zCrc.h"
#include "../../../../C/Alloc.h"
#include "../../../../C/CpuArch.h"

#ifndef _7ZIP_ST
#include "../../../Windows/Synchronization.h"
#include "../../../Windows/Thread.h"
#endif

#if defined(_WIN32) || defined(UNIX_USE_WIN_FILE)
#define USE_WIN_FILE
#endif

#ifdef USE_WIN_FILE
#include "../../../Windows/FileIO.h"
#endif


#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"
#include "../../../Common/StringToInt.h"

#include "../../Common/MethodProps.h"
#include "../../Common/StreamUtils.h"

#include "Bench.h"

using namespace NWindows;

static const UInt32 k_LZMA = 0x030101;

static const UInt64 kComplexInCommands = (UInt64)1 <<
  #ifdef UNDER_CE
    31;
  #else
    34;
  #endif

static const UInt32 kComplexInSeconds = 4;

static void SetComplexCommands(UInt32 complexInSeconds,
    bool isSpecifiedFreq, UInt64 cpuFreq, UInt64 &complexInCommands)
{
  complexInCommands = kComplexInCommands;
  const UInt64 kMinFreq = (UInt64)1000000 * 4;
  const UInt64 kMaxFreq = (UInt64)1000000 * 20000;
  if (cpuFreq < kMinFreq && !isSpecifiedFreq)
    cpuFreq = kMinFreq;
  if (cpuFreq < kMaxFreq || isSpecifiedFreq)
  {
    if (complexInSeconds != 0)
      complexInCommands = complexInSeconds * cpuFreq;
    else
      complexInCommands = cpuFreq >> 2;
  }
}

static const unsigned kNumHashDictBits = 17;
static const UInt32 kFilterUnpackSize = (48 << 10);

static const unsigned kOldLzmaDictBits = 30;

static const UInt32 kAdditionalSize = (1 << 16);
static const UInt32 kCompressedAdditionalSize = (1 << 10);
static const UInt32 kMaxLzmaPropSize = 5;

class CBaseRandomGenerator
{
  UInt32 A1;
  UInt32 A2;
public:
  CBaseRandomGenerator() { Init(); }
  void Init() { A1 = 362436069; A2 = 521288629;}
  UInt32 GetRnd()
  {
    return
      ((A1 = 36969 * (A1 & 0xffff) + (A1 >> 16)) << 16) +
      ((A2 = 18000 * (A2 & 0xffff) + (A2 >> 16)) );
  }
};


static const unsigned kBufferAlignment = 1 << 4;

struct CBenchBuffer
{
  size_t BufferSize;

  #ifdef _WIN32

  Byte *Buffer;

  CBenchBuffer(): BufferSize(0), Buffer(NULL) {}
  ~CBenchBuffer() { ::MidFree(Buffer); }
  
  void AllocAlignedMask(size_t size, size_t)
  {
    ::MidFree(Buffer);
    BufferSize = 0;
    Buffer = (Byte *)::MidAlloc(size);
    if (Buffer)
      BufferSize = size;
  }
  
  #else
  
  Byte *Buffer;
  Byte *_bufBase;

  CBenchBuffer(): BufferSize(0), Buffer(NULL), _bufBase(NULL){}
  ~CBenchBuffer() { ::MidFree(_bufBase); }
  
  void AllocAlignedMask(size_t size, size_t alignMask)
  {
    ::MidFree(_bufBase);
    Buffer = NULL;
    BufferSize = 0;
    _bufBase = (Byte *)::MidAlloc(size + alignMask);
    
    if (_bufBase)
    {
      // Buffer = (Byte *)(((uintptr_t)_bufBase + alignMask) & ~(uintptr_t)alignMask);
         Buffer = (Byte *)(((ptrdiff_t)_bufBase + alignMask) & ~(ptrdiff_t)alignMask);
      BufferSize = size;
    }
  }

  #endif

  bool Alloc(size_t size)
  {
    if (Buffer && BufferSize == size)
      return true;
    AllocAlignedMask(size, kBufferAlignment - 1);
    return (Buffer != NULL || size == 0);
  }
};


class CBenchRandomGenerator: public CBenchBuffer
{
  static UInt32 GetVal(UInt32 &res, unsigned numBits)
  {
    UInt32 val = res & (((UInt32)1 << numBits) - 1);
    res >>= numBits;
    return val;
  }
  
  static UInt32 GetLen(UInt32 &r)
  {
    UInt32 len = GetVal(r, 2);
    return GetVal(r, 1 + len);
  }

public:
  
  void GenerateSimpleRandom(CBaseRandomGenerator *_RG_)
  {
    CBaseRandomGenerator rg = *_RG_;
    const size_t bufSize = BufferSize;
    Byte *buf = Buffer;
    for (size_t i = 0; i < bufSize; i++)
      buf[i] = (Byte)rg.GetRnd();
    *_RG_ = rg;
  }

  void GenerateLz(unsigned dictBits, CBaseRandomGenerator *_RG_)
  {
    CBaseRandomGenerator rg = *_RG_;
    UInt32 pos = 0;
    UInt32 rep0 = 1;
    const size_t bufSize = BufferSize;
    Byte *buf = Buffer;
    unsigned posBits = 1;
    
    while (pos < bufSize)
    {
      UInt32 r = rg.GetRnd();
      if (GetVal(r, 1) == 0 || pos < 1024)
        buf[pos++] = (Byte)(r & 0xFF);
      else
      {
        UInt32 len;
        len = 1 + GetLen(r);
        
        if (GetVal(r, 3) != 0)
        {
          len += GetLen(r);

          while (((UInt32)1 << posBits) < pos)
            posBits++;

          unsigned numBitsMax = dictBits;
          if (numBitsMax > posBits)
            numBitsMax = posBits;

          const unsigned kAddBits = 6;
          unsigned numLogBits = 5;
          if (numBitsMax <= (1 << 4) - 1 + kAddBits)
            numLogBits = 4;

          for (;;)
          {
            UInt32 ppp = GetVal(r, numLogBits) + kAddBits;
            r = rg.GetRnd();
            if (ppp > numBitsMax)
              continue;
            rep0 = GetVal(r, ppp);
            if (rep0 < pos)
              break;
            r = rg.GetRnd();
          }
          rep0++;
        }

        {
          UInt32 rem = (UInt32)bufSize - pos;
          if (len > rem)
            len = rem;
        }
        Byte *dest = buf + pos;
        const Byte *src = dest - rep0;
        pos += len;
        for (UInt32 i = 0; i < len; i++)
          *dest++ = *src++;
      }
    }

    *_RG_ = rg;
  }
};


class CBenchmarkInStream:
  public ISequentialInStream,
  public CMyUnknownImp
{
  const Byte *Data;
  size_t Pos;
  size_t Size;
public:
  MY_UNKNOWN_IMP
  void Init(const Byte *data, size_t size)
  {
    Data = data;
    Size = size;
    Pos = 0;
  }
  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);
};

STDMETHODIMP CBenchmarkInStream::Read(void *data, UInt32 size, UInt32 *processedSize)
{
  size_t remain = Size - Pos;
  UInt32 kMaxBlockSize = (1 << 20);
  if (size > kMaxBlockSize)
    size = kMaxBlockSize;
  if (size > remain)
    size = (UInt32)remain;
  for (UInt32 i = 0; i < size; i++)
    ((Byte *)data)[i] = Data[Pos + i];
  Pos += size;
  if (processedSize)
    *processedSize = size;
  return S_OK;
}
  
class CBenchmarkOutStream:
  public ISequentialOutStream,
  public CBenchBuffer,
  public CMyUnknownImp
{
  // bool _overflow;
public:
  size_t Pos;
  bool RealCopy;
  bool CalcCrc;
  UInt32 Crc;

  // CBenchmarkOutStream(): _overflow(false) {}
  void Init(bool realCopy, bool calcCrc)
  {
    Crc = CRC_INIT_VAL;
    RealCopy = realCopy;
    CalcCrc = calcCrc;
    // _overflow = false;
    Pos = 0;
  }

  // void Print() { printf("\n%8d %8d\n", (unsigned)BufferSize, (unsigned)Pos); }

  MY_UNKNOWN_IMP
  STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize);
};

STDMETHODIMP CBenchmarkOutStream::Write(const void *data, UInt32 size, UInt32 *processedSize)
{
  size_t curSize = BufferSize - Pos;
  if (curSize > size)
    curSize = size;
  if (curSize != 0)
  {
    if (RealCopy)
      memcpy(Buffer + Pos, data, curSize);
    if (CalcCrc)
      Crc = CrcUpdate(Crc, data, curSize);
    Pos += curSize;
  }
  if (processedSize)
    *processedSize = (UInt32)curSize;
  if (curSize != size)
  {
    // _overflow = true;
    return E_FAIL;
  }
  return S_OK;
}
  
class CCrcOutStream:
  public ISequentialOutStream,
  public CMyUnknownImp
{
public:
  bool CalcCrc;
  UInt32 Crc;
  MY_UNKNOWN_IMP
    
  CCrcOutStream(): CalcCrc(true) {};
  void Init() { Crc = CRC_INIT_VAL; }
  STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize);
};

STDMETHODIMP CCrcOutStream::Write(const void *data, UInt32 size, UInt32 *processedSize)
{
  if (CalcCrc)
    Crc = CrcUpdate(Crc, data, size);
  if (processedSize)
    *processedSize = size;
  return S_OK;
}
  
static UInt64 GetTimeCount()
{
  #ifdef USE_POSIX_TIME
  #ifdef USE_POSIX_TIME2
  timeval v;
  if (gettimeofday(&v, 0) == 0)
    return (UInt64)(v.tv_sec) * 1000000 + v.tv_usec;
  return (UInt64)time(NULL) * 1000000;
  #else
  return time(NULL);
  #endif
  #else
  /*
  LARGE_INTEGER value;
  if (::QueryPerformanceCounter(&value))
    return value.QuadPart;
  */
  return GetTickCount();
  #endif
}

static UInt64 GetFreq()
{
  #ifdef USE_POSIX_TIME
  #ifdef USE_POSIX_TIME2
  return 1000000;
  #else
  return 1;
  #endif
  #else
  /*
  LARGE_INTEGER value;
  if (::QueryPerformanceFrequency(&value))
    return value.QuadPart;
  */
  return 1000;
  #endif
}

#ifdef USE_POSIX_TIME

struct CUserTime
{
  UInt64 Sum;
  clock_t Prev;
  
  void Init()
  {
    Prev = clock();
    Sum = 0;
  }

  UInt64 GetUserTime()
  {
    clock_t v = clock();
    Sum += v - Prev;
    Prev = v;
    return Sum;
  }
};

#else

static inline UInt64 GetTime64(const FILETIME &t) { return ((UInt64)t.dwHighDateTime << 32) | t.dwLowDateTime; }
UInt64 GetWinUserTime()
{
  FILETIME creationTime, exitTime, kernelTime, userTime;
  if (
  #ifdef UNDER_CE
    ::GetThreadTimes(::GetCurrentThread()
  #else
    ::GetProcessTimes(::GetCurrentProcess()
  #endif
    , &creationTime, &exitTime, &kernelTime, &userTime) != 0)
    return GetTime64(userTime) + GetTime64(kernelTime);
  return (UInt64)GetTickCount() * 10000;
}

struct CUserTime
{
  UInt64 StartTime;

  void Init() { StartTime = GetWinUserTime(); }
  UInt64 GetUserTime() { return GetWinUserTime() - StartTime; }
};

#endif

static UInt64 GetUserFreq()
{
  #ifdef USE_POSIX_TIME
  return CLOCKS_PER_SEC;
  #else
  return 10000000;
  #endif
}

class CBenchProgressStatus
{
  #ifndef _7ZIP_ST
  NSynchronization::CCriticalSection CS;
  #endif
public:
  HRESULT Res;
  bool EncodeMode;
  void SetResult(HRESULT res)
  {
    #ifndef _7ZIP_ST
    NSynchronization::CCriticalSectionLock lock(CS);
    #endif
    Res = res;
  }
  HRESULT GetResult()
  {
    #ifndef _7ZIP_ST
    NSynchronization::CCriticalSectionLock lock(CS);
    #endif
    return Res;
  }
};

struct CBenchInfoCalc
{
  CBenchInfo BenchInfo;
  CUserTime UserTime;

  void SetStartTime();
  void SetFinishTime(CBenchInfo &dest);
};

void CBenchInfoCalc::SetStartTime()
{
  BenchInfo.GlobalFreq = GetFreq();
  BenchInfo.UserFreq = GetUserFreq();
  BenchInfo.GlobalTime = ::GetTimeCount();
  BenchInfo.UserTime = 0;
  UserTime.Init();
}

void CBenchInfoCalc::SetFinishTime(CBenchInfo &dest)
{
  dest = BenchInfo;
  dest.GlobalTime = ::GetTimeCount() - BenchInfo.GlobalTime;
  dest.UserTime = UserTime.GetUserTime();
}

class CBenchProgressInfo:
  public ICompressProgressInfo,
  public CMyUnknownImp,
  public CBenchInfoCalc
{
public:
  CBenchProgressStatus *Status;
  IBenchCallback *Callback;

  CBenchProgressInfo(): Callback(NULL) {}
  MY_UNKNOWN_IMP
  STDMETHOD(SetRatioInfo)(const UInt64 *inSize, const UInt64 *outSize);
};

STDMETHODIMP CBenchProgressInfo::SetRatioInfo(const UInt64 *inSize, const UInt64 *outSize)
{
  HRESULT res = Status->GetResult();
  if (res != S_OK)
    return res;
  if (!Callback)
    return res;
  CBenchInfo info;
  SetFinishTime(info);
  if (Status->EncodeMode)
  {
    info.UnpackSize = BenchInfo.UnpackSize + *inSize;
    info.PackSize = BenchInfo.PackSize + *outSize;
    res = Callback->SetEncodeResult(info, false);
  }
  else
  {
    info.PackSize = BenchInfo.PackSize + *inSize;
    info.UnpackSize = BenchInfo.UnpackSize + *outSize;
    res = Callback->SetDecodeResult(info, false);
  }
  if (res != S_OK)
    Status->SetResult(res);
  return res;
}

static const unsigned kSubBits = 8;

static UInt32 GetLogSize(UInt32 size)
{
  for (unsigned i = kSubBits; i < 32; i++)
    for (UInt32 j = 0; j < (1 << kSubBits); j++)
      if (size <= (((UInt32)1) << i) + (j << (i - kSubBits)))
        return (i << kSubBits) + j;
  return (32 << kSubBits);
}

static void NormalizeVals(UInt64 &v1, UInt64 &v2)
{
  while (v1 > 1000000)
  {
    v1 >>= 1;
    v2 >>= 1;
  }
}

UInt64 CBenchInfo::GetUsage() const
{
  UInt64 userTime = UserTime;
  UInt64 userFreq = UserFreq;
  UInt64 globalTime = GlobalTime;
  UInt64 globalFreq = GlobalFreq;
  NormalizeVals(userTime, userFreq);
  NormalizeVals(globalFreq, globalTime);
  if (userFreq == 0)
    userFreq = 1;
  if (globalTime == 0)
    globalTime = 1;
  return userTime * globalFreq * 1000000 / userFreq / globalTime;
}

UInt64 CBenchInfo::GetRatingPerUsage(UInt64 rating) const
{
  UInt64 userTime = UserTime;
  UInt64 userFreq = UserFreq;
  UInt64 globalTime = GlobalTime;
  UInt64 globalFreq = GlobalFreq;
  NormalizeVals(userFreq, userTime);
  NormalizeVals(globalTime, globalFreq);
  if (globalFreq == 0)
    globalFreq = 1;
  if (userTime == 0)
    userTime = 1;
  return userFreq * globalTime / globalFreq * rating / userTime;
}

static UInt64 MyMultDiv64(UInt64 value, UInt64 elapsedTime, UInt64 freq)
{
  UInt64 elTime = elapsedTime;
  NormalizeVals(freq, elTime);
  if (elTime == 0)
    elTime = 1;
  return value * freq / elTime;
}

UInt64 CBenchInfo::GetSpeed(UInt64 numCommands) const
{
  return MyMultDiv64(numCommands, GlobalTime, GlobalFreq);
}

struct CBenchProps
{
  bool LzmaRatingMode;
  
  UInt32 EncComplex;
  UInt32 DecComplexCompr;
  UInt32 DecComplexUnc;

  CBenchProps(): LzmaRatingMode(false) {}
  void SetLzmaCompexity();

  UInt64 GeComprCommands(UInt64 unpackSize)
  {
    return unpackSize * EncComplex;
  }

  UInt64 GeDecomprCommands(UInt64 packSize, UInt64 unpackSize)
  {
    return (packSize * DecComplexCompr + unpackSize * DecComplexUnc);
  }

  UInt64 GetCompressRating(UInt32 dictSize, UInt64 elapsedTime, UInt64 freq, UInt64 size);
  UInt64 GetDecompressRating(UInt64 elapsedTime, UInt64 freq, UInt64 outSize, UInt64 inSize, UInt64 numIterations);
};

void CBenchProps::SetLzmaCompexity()
{
  EncComplex = 1200;
  DecComplexUnc = 4;
  DecComplexCompr = 190;
  LzmaRatingMode = true;
}

UInt64 CBenchProps::GetCompressRating(UInt32 dictSize, UInt64 elapsedTime, UInt64 freq, UInt64 size)
{
  if (dictSize < (1 << kBenchMinDicLogSize))
    dictSize = (1 << kBenchMinDicLogSize);
  UInt64 encComplex = EncComplex;
  if (LzmaRatingMode)
  {
    UInt64 t = GetLogSize(dictSize) - (kBenchMinDicLogSize << kSubBits);
    encComplex = 870 + ((t * t * 5) >> (2 * kSubBits));
  }
  UInt64 numCommands = (UInt64)size * encComplex;
  return MyMultDiv64(numCommands, elapsedTime, freq);
}

UInt64 CBenchProps::GetDecompressRating(UInt64 elapsedTime, UInt64 freq, UInt64 outSize, UInt64 inSize, UInt64 numIterations)
{
  UInt64 numCommands = (inSize * DecComplexCompr + outSize * DecComplexUnc) * numIterations;
  return MyMultDiv64(numCommands, elapsedTime, freq);
}

UInt64 GetCompressRating(UInt32 dictSize, UInt64 elapsedTime, UInt64 freq, UInt64 size)
{
  CBenchProps props;
  props.SetLzmaCompexity();
  return props.GetCompressRating(dictSize, elapsedTime, freq, size);
}

UInt64 GetDecompressRating(UInt64 elapsedTime, UInt64 freq, UInt64 outSize, UInt64 inSize, UInt64 numIterations)
{
  CBenchProps props;
  props.SetLzmaCompexity();
  return props.GetDecompressRating(elapsedTime, freq, outSize, inSize, numIterations);
}

struct CEncoderInfo;

struct CEncoderInfo
{
  #ifndef _7ZIP_ST
  NWindows::CThread thread[2];
  UInt32 NumDecoderSubThreads;
  #endif
  CMyComPtr<ICompressCoder> _encoder;
  CMyComPtr<ICompressFilter> _encoderFilter;
  CBenchProgressInfo *progressInfoSpec[2];
  CMyComPtr<ICompressProgressInfo> progressInfo[2];
  UInt64 NumIterations;

  #ifdef USE_ALLOCA
  size_t AllocaSize;
  #endif

  Byte _key[32];
  Byte _iv[16];
  Byte _psw[16];
  bool CheckCrc_Enc;
  bool CheckCrc_Dec;

  struct CDecoderInfo
  {
    CEncoderInfo *Encoder;
    UInt32 DecoderIndex;
    bool CallbackMode;
    
    #ifdef USE_ALLOCA
    size_t AllocaSize;
    #endif
  };
  CDecoderInfo decodersInfo[2];

  CMyComPtr<ICompressCoder> _decoders[2];
  CMyComPtr<ICompressFilter> _decoderFilter;

  HRESULT Results[2];
  CBenchmarkOutStream *outStreamSpec;
  CMyComPtr<ISequentialOutStream> outStream;
  IBenchCallback *callback;
  IBenchPrintCallback *printCallback;
  UInt32 crc;
  size_t kBufferSize;
  size_t compressedSize;
  const Byte *uncompressedDataPtr;

  const Byte *fileData;
  CBenchRandomGenerator rg;

  CBenchBuffer rgCopy; // it must be 16-byte aligned !!!
  CBenchmarkOutStream *propStreamSpec;
  CMyComPtr<ISequentialOutStream> propStream;

  // for decode
  COneMethodInfo _method;
  size_t _uncompressedDataSize;

  HRESULT Init(
      const COneMethodInfo &method,
      unsigned generateDictBits,
      CBaseRandomGenerator *rg);
  HRESULT Encode();
  HRESULT Decode(UInt32 decoderIndex);

  CEncoderInfo():
    fileData(NULL),
    CheckCrc_Enc(true),
    CheckCrc_Dec(true),
    outStreamSpec(NULL), callback(NULL), printCallback(NULL), propStreamSpec(NULL) {}

  #ifndef _7ZIP_ST
  
  static THREAD_FUNC_DECL EncodeThreadFunction(void *param)
  {
    HRESULT res;
    CEncoderInfo *encoder = (CEncoderInfo *)param;
    try
    {
      #ifdef USE_ALLOCA
      alloca(encoder->AllocaSize);
      #endif

      res = encoder->Encode();
      encoder->Results[0] = res;
    }
    catch(...)
    {
      res = E_FAIL;
    }
    if (res != S_OK)
      encoder->progressInfoSpec[0]->Status->SetResult(res);
    return 0;
  }
  
  static THREAD_FUNC_DECL DecodeThreadFunction(void *param)
  {
    CDecoderInfo *decoder = (CDecoderInfo *)param;
    
    #ifdef USE_ALLOCA
    alloca(decoder->AllocaSize);
    #endif
    
    CEncoderInfo *encoder = decoder->Encoder;
    encoder->Results[decoder->DecoderIndex] = encoder->Decode(decoder->DecoderIndex);
    return 0;
  }

  HRESULT CreateEncoderThread()
  {
    return thread[0].Create(EncodeThreadFunction, this);
  }

  HRESULT CreateDecoderThread(unsigned index, bool callbackMode
      #ifdef USE_ALLOCA
      , size_t allocaSize
      #endif
      )
  {
    CDecoderInfo &decoder = decodersInfo[index];
    decoder.DecoderIndex = index;
    decoder.Encoder = this;
    
    #ifdef USE_ALLOCA
    decoder.AllocaSize = allocaSize;
    #endif
    
    decoder.CallbackMode = callbackMode;
    return thread[index].Create(DecodeThreadFunction, &decoder);
  }
  
  #endif
};


HRESULT CEncoderInfo::Init(
    const COneMethodInfo &method,
    unsigned generateDictBits,
    CBaseRandomGenerator *rgLoc)
{
  // we need extra space, if input data is already compressed
  const size_t kCompressedBufferSize =
      kCompressedAdditionalSize +
      kBufferSize + kBufferSize / 16;
      // kBufferSize / 2;

  if (kCompressedBufferSize < kBufferSize)
    return E_FAIL;

  uncompressedDataPtr = fileData;
  
  if (!fileData)
  {
    if (!rg.Alloc(kBufferSize))
      return E_OUTOFMEMORY;
    
    // DWORD ttt = GetTickCount();
    if (generateDictBits == 0)
      rg.GenerateSimpleRandom(rgLoc);
    else
      rg.GenerateLz(generateDictBits, rgLoc);
    // printf("\n%d\n            ", GetTickCount() - ttt);

    crc = CrcCalc(rg.Buffer, rg.BufferSize);
    uncompressedDataPtr = rg.Buffer;
  }
  
  if (_encoderFilter)
  {
    if (!rgCopy.Alloc(kBufferSize))
      return E_OUTOFMEMORY;
  }


  outStreamSpec = new CBenchmarkOutStream;
  outStream = outStreamSpec;
  if (!outStreamSpec->Alloc(kCompressedBufferSize))
    return E_OUTOFMEMORY;

  propStreamSpec = 0;
  if (!propStream)
  {
    propStreamSpec = new CBenchmarkOutStream;
    propStream = propStreamSpec;
  }
  if (!propStreamSpec->Alloc(kMaxLzmaPropSize))
    return E_OUTOFMEMORY;
  propStreamSpec->Init(true, false);
  
 
  CMyComPtr<IUnknown> coder;
  if (_encoderFilter)
    coder = _encoderFilter;
  else
    coder = _encoder;
  {
    CMyComPtr<ICompressSetCoderProperties> scp;
    coder.QueryInterface(IID_ICompressSetCoderProperties, &scp);
    if (scp)
    {
      UInt64 reduceSize = kBufferSize;
      RINOK(method.SetCoderProps(scp, &reduceSize));
    }
    else
    {
      if (method.AreThereNonOptionalProps())
        return E_INVALIDARG;
    }

    CMyComPtr<ICompressWriteCoderProperties> writeCoderProps;
    coder.QueryInterface(IID_ICompressWriteCoderProperties, &writeCoderProps);
    if (writeCoderProps)
    {
      RINOK(writeCoderProps->WriteCoderProperties(propStream));
    }

    {
      CMyComPtr<ICryptoSetPassword> sp;
      coder.QueryInterface(IID_ICryptoSetPassword, &sp);
      if (sp)
      {
        RINOK(sp->CryptoSetPassword(_psw, sizeof(_psw)));

        // we must call encoding one time to calculate password key for key cache.
        // it must be after WriteCoderProperties!
        Byte temp[16];
        memset(temp, 0, sizeof(temp));
        
        if (_encoderFilter)
        {
          _encoderFilter->Init();
          _encoderFilter->Filter(temp, sizeof(temp));
        }
        else
        {
          CBenchmarkInStream *inStreamSpec = new CBenchmarkInStream;
          CMyComPtr<ISequentialInStream> inStream = inStreamSpec;
          inStreamSpec->Init(temp, sizeof(temp));
          
          CCrcOutStream *crcStreamSpec = new CCrcOutStream;
          CMyComPtr<ISequentialOutStream> crcStream = crcStreamSpec;
          crcStreamSpec->Init();

          RINOK(_encoder->Code(inStream, crcStream, 0, 0, NULL));
        }
      }
    }
  }

  return S_OK;
}


static void My_FilterBench(ICompressFilter *filter, Byte *data, size_t size)
{
  while (size != 0)
  {
    UInt32 cur = (UInt32)1 << 31;
    if (cur > size)
      cur = (UInt32)size;
    UInt32 processed = filter->Filter(data, cur);
    data += processed;
    // if (processed > size) (in AES filter), we must fill last block with zeros.
    // but it is not important for benchmark. So we just copy that data without filtering.
    if (processed > size || processed == 0)
      break;
    size -= processed;
  }
}


HRESULT CEncoderInfo::Encode()
{
  CBenchInfo &bi = progressInfoSpec[0]->BenchInfo;
  bi.UnpackSize = 0;
  bi.PackSize = 0;
  CMyComPtr<ICryptoProperties> cp;
  CMyComPtr<IUnknown> coder;
  if (_encoderFilter)
    coder = _encoderFilter;
  else
    coder = _encoder;
  coder.QueryInterface(IID_ICryptoProperties, &cp);
  CBenchmarkInStream *inStreamSpec = new CBenchmarkInStream;
  CMyComPtr<ISequentialInStream> inStream = inStreamSpec;
  UInt64 prev = 0;

  UInt32 crcPrev = 0;

  if (cp)
  {
    RINOK(cp->SetKey(_key, sizeof(_key)));
    RINOK(cp->SetInitVector(_iv, sizeof(_iv)));
  }

  for (UInt64 i = 0; i < NumIterations; i++)
  {
    if (printCallback && bi.UnpackSize - prev > (1 << 20))
    {
      RINOK(printCallback->CheckBreak());
      prev = bi.UnpackSize;
    }
    
    bool isLast = (i == NumIterations - 1);
    bool calcCrc = ((isLast || (i & 0x7F) == 0 || CheckCrc_Enc) && NumIterations != 1);
    outStreamSpec->Init(isLast, calcCrc);
    
    if (_encoderFilter)
    {
      memcpy(rgCopy.Buffer, uncompressedDataPtr, kBufferSize);
      _encoderFilter->Init();
      My_FilterBench(_encoderFilter, rgCopy.Buffer, kBufferSize);
      RINOK(WriteStream(outStream, rgCopy.Buffer, kBufferSize));
    }
    else
    {
      inStreamSpec->Init(uncompressedDataPtr, kBufferSize);
      RINOK(_encoder->Code(inStream, outStream, NULL, NULL, progressInfo[0]));
    }

    // outStreamSpec->Print();

    UInt32 crcNew = CRC_GET_DIGEST(outStreamSpec->Crc);
    if (i == 0)
      crcPrev = crcNew;
    else if (calcCrc && crcPrev != crcNew)
      return E_FAIL;
    
    compressedSize = outStreamSpec->Pos;
    bi.UnpackSize += kBufferSize;
    bi.PackSize += compressedSize;
  }
  
  _encoder.Release();
  _encoderFilter.Release();
  return S_OK;
}


HRESULT CEncoderInfo::Decode(UInt32 decoderIndex)
{
  CBenchmarkInStream *inStreamSpec = new CBenchmarkInStream;
  CMyComPtr<ISequentialInStream> inStream = inStreamSpec;
  CMyComPtr<ICompressCoder> &decoder = _decoders[decoderIndex];
  CMyComPtr<IUnknown> coder;
  if (_decoderFilter)
  {
    if (decoderIndex != 0)
      return E_FAIL;
    coder = _decoderFilter;
  }
  else
    coder = decoder;

  CMyComPtr<ICompressSetDecoderProperties2> setDecProps;
  coder.QueryInterface(IID_ICompressSetDecoderProperties2, &setDecProps);
  if (!setDecProps && propStreamSpec->Pos != 0)
    return E_FAIL;

  CCrcOutStream *crcOutStreamSpec = new CCrcOutStream;
  CMyComPtr<ISequentialOutStream> crcOutStream = crcOutStreamSpec;
    
  CBenchProgressInfo *pi = progressInfoSpec[decoderIndex];
  pi->BenchInfo.UnpackSize = 0;
  pi->BenchInfo.PackSize = 0;

  #ifndef _7ZIP_ST
  {
    CMyComPtr<ICompressSetCoderMt> setCoderMt;
    coder.QueryInterface(IID_ICompressSetCoderMt, &setCoderMt);
    if (setCoderMt)
    {
      RINOK(setCoderMt->SetNumberOfThreads(NumDecoderSubThreads));
    }
  }
  #endif

  CMyComPtr<ICompressSetCoderProperties> scp;
  coder.QueryInterface(IID_ICompressSetCoderProperties, &scp);
  if (scp)
  {
    UInt64 reduceSize = _uncompressedDataSize;
    RINOK(_method.SetCoderProps(scp, &reduceSize));
  }

  CMyComPtr<ICryptoProperties> cp;
  coder.QueryInterface(IID_ICryptoProperties, &cp);
  
  if (setDecProps)
  {
    RINOK(setDecProps->SetDecoderProperties2(propStreamSpec->Buffer, (UInt32)propStreamSpec->Pos));
  }

  {
    CMyComPtr<ICryptoSetPassword> sp;
    coder.QueryInterface(IID_ICryptoSetPassword, &sp);
    if (sp)
    {
      RINOK(sp->CryptoSetPassword(_psw, sizeof(_psw)));
    }
  }

  UInt64 prev = 0;
  
  if (cp)
  {
    RINOK(cp->SetKey(_key, sizeof(_key)));
    RINOK(cp->SetInitVector(_iv, sizeof(_iv)));
  }

  for (UInt64 i = 0; i < NumIterations; i++)
  {
    if (printCallback && pi->BenchInfo.UnpackSize - prev > (1 << 20))
    {
      RINOK(printCallback->CheckBreak());
      prev = pi->BenchInfo.UnpackSize;
    }

    inStreamSpec->Init(outStreamSpec->Buffer, compressedSize);
    crcOutStreamSpec->Init();
    
    UInt64 outSize = kBufferSize;
    crcOutStreamSpec->CalcCrc = ((i & 0x7F) == 0 || CheckCrc_Dec);
    
    if (_decoderFilter)
    {
      if (compressedSize > rgCopy.BufferSize)
        return E_FAIL;
      memcpy(rgCopy.Buffer, outStreamSpec->Buffer, compressedSize);
      _decoderFilter->Init();
      My_FilterBench(_decoderFilter, rgCopy.Buffer, compressedSize);
      RINOK(WriteStream(crcOutStream, rgCopy.Buffer, compressedSize));
    }
    else
    {
      RINOK(decoder->Code(inStream, crcOutStream, 0, &outSize, progressInfo[decoderIndex]));
    }
  
    if (crcOutStreamSpec->CalcCrc && CRC_GET_DIGEST(crcOutStreamSpec->Crc) != crc)
      return S_FALSE;
    pi->BenchInfo.UnpackSize += kBufferSize;
    pi->BenchInfo.PackSize += compressedSize;
  }
  
  decoder.Release();
  _decoderFilter.Release();
  return S_OK;
}


static const UInt32 kNumThreadsMax = (1 << 12);

struct CBenchEncoders
{
  CEncoderInfo *encoders;
  CBenchEncoders(UInt32 num): encoders(NULL) { encoders = new CEncoderInfo[num]; }
  ~CBenchEncoders() { delete []encoders; }
};


static UInt64 GetNumIterations(UInt64 numCommands, UInt64 complexInCommands)
{
  if (numCommands < (1 << 4))
    numCommands = (1 << 4);
  UInt64 res = complexInCommands / numCommands;
  return (res == 0 ? 1 : res);
}


static HRESULT MethodBench(
    DECL_EXTERNAL_CODECS_LOC_VARS
    UInt64 complexInCommands,
    bool
      #ifndef _7ZIP_ST
        oldLzmaBenchMode
      #endif
    ,
    UInt32
      #ifndef _7ZIP_ST
        numThreads
      #endif
    ,
    const COneMethodInfo &method2,
    size_t uncompressedDataSize,
    const Byte *fileData,
    unsigned generateDictBits,

    IBenchPrintCallback *printCallback,
    IBenchCallback *callback,
    CBenchProps *benchProps)
{
  COneMethodInfo method = method2;
  UInt64 methodId;
  UInt32 numStreams;
  int codecIndex = FindMethod_Index(
      EXTERNAL_CODECS_LOC_VARS
      method.MethodName, true,
      methodId, numStreams);
  if (codecIndex < 0)
    return E_NOTIMPL;
  if (numStreams != 1)
    return E_INVALIDARG;

  UInt32 numEncoderThreads = 1;
  UInt32 numSubDecoderThreads = 1;
  
  #ifndef _7ZIP_ST
    numEncoderThreads = numThreads;

    if (oldLzmaBenchMode && methodId == k_LZMA)
    {
      if (numThreads == 1 && method.Get_NumThreads() < 0)
        method.AddProp_NumThreads(1);
      const UInt32 numLzmaThreads = method.Get_Lzma_NumThreads();
      if (numThreads > 1 && numLzmaThreads > 1)
      {
        numEncoderThreads = numThreads / 2;
        numSubDecoderThreads = 2;
      }
    }
  #endif

  CBenchEncoders encodersSpec(numEncoderThreads);
  CEncoderInfo *encoders = encodersSpec.encoders;

  UInt32 i;
  
  for (i = 0; i < numEncoderThreads; i++)
  {
    CEncoderInfo &encoder = encoders[i];
    encoder.callback = (i == 0) ? callback : 0;
    encoder.printCallback = printCallback;

    {
      CCreatedCoder cod;
      RINOK(CreateCoder_Index(EXTERNAL_CODECS_LOC_VARS codecIndex, true, encoder._encoderFilter, cod));
      encoder._encoder = cod.Coder;
      if (!encoder._encoder && !encoder._encoderFilter)
        return E_NOTIMPL;
    }

    encoder.CheckCrc_Enc = (benchProps->EncComplex) > 30 ;
    encoder.CheckCrc_Dec = (benchProps->DecComplexCompr + benchProps->DecComplexUnc) > 30 ;

    memset(encoder._iv, 0, sizeof(encoder._iv));
    memset(encoder._key, 0, sizeof(encoder._key));
    memset(encoder._psw, 0, sizeof(encoder._psw));

    for (UInt32 j = 0; j < numSubDecoderThreads; j++)
    {
      CCreatedCoder cod;
      CMyComPtr<ICompressCoder> &decoder = encoder._decoders[j];
      RINOK(CreateCoder_Id(EXTERNAL_CODECS_LOC_VARS methodId, false, encoder._decoderFilter, cod));
      decoder = cod.Coder;
      if (!encoder._decoderFilter && !decoder)
        return E_NOTIMPL;
    }
  }

  CBaseRandomGenerator rg;
  rg.Init();

  UInt32 crc = 0;
  if (fileData)
    crc = CrcCalc(fileData, uncompressedDataSize);

  for (i = 0; i < numEncoderThreads; i++)
  {
    CEncoderInfo &encoder = encoders[i];
    encoder._method = method;
    encoder._uncompressedDataSize = uncompressedDataSize;
    encoder.kBufferSize = uncompressedDataSize;
    encoder.fileData = fileData;
    encoder.crc = crc;

    RINOK(encoders[i].Init(method, generateDictBits, &rg));
  }

  CBenchProgressStatus status;
  status.Res = S_OK;
  status.EncodeMode = true;

  for (i = 0; i < numEncoderThreads; i++)
  {
    CEncoderInfo &encoder = encoders[i];
    encoder.NumIterations = GetNumIterations(benchProps->GeComprCommands(uncompressedDataSize), complexInCommands);

    for (int j = 0; j < 2; j++)
    {
      CBenchProgressInfo *spec = new CBenchProgressInfo;
      encoder.progressInfoSpec[j] = spec;
      encoder.progressInfo[j] = spec;
      spec->Status = &status;
    }
    
    if (i == 0)
    {
      CBenchProgressInfo *bpi = encoder.progressInfoSpec[0];
      bpi->Callback = callback;
      bpi->BenchInfo.NumIterations = numEncoderThreads;
      bpi->SetStartTime();
    }

    #ifndef _7ZIP_ST
    if (numEncoderThreads > 1)
    {
      #ifdef USE_ALLOCA
      encoder.AllocaSize = (i * 16 * 21) & 0x7FF;
      #endif

      RINOK(encoder.CreateEncoderThread())
    }
    else
    #endif
    {
      RINOK(encoder.Encode());
    }
  }
  
  #ifndef _7ZIP_ST
  if (numEncoderThreads > 1)
    for (i = 0; i < numEncoderThreads; i++)
      encoders[i].thread[0].Wait();
  #endif

  RINOK(status.Res);

  CBenchInfo info;

  encoders[0].progressInfoSpec[0]->SetFinishTime(info);
  info.UnpackSize = 0;
  info.PackSize = 0;
  info.NumIterations = encoders[0].NumIterations;
  
  for (i = 0; i < numEncoderThreads; i++)
  {
    CEncoderInfo &encoder = encoders[i];
    info.UnpackSize += encoder.kBufferSize;
    info.PackSize += encoder.compressedSize;
  }
  
  RINOK(callback->SetEncodeResult(info, true));


  status.Res = S_OK;
  status.EncodeMode = false;

  UInt32 numDecoderThreads = numEncoderThreads * numSubDecoderThreads;
  
  for (i = 0; i < numEncoderThreads; i++)
  {
    CEncoderInfo &encoder = encoders[i];

    if (i == 0)
    {
      encoder.NumIterations = GetNumIterations(benchProps->GeDecomprCommands(encoder.compressedSize, encoder.kBufferSize), complexInCommands);
      CBenchProgressInfo *bpi = encoder.progressInfoSpec[0];
      bpi->Callback = callback;
      bpi->BenchInfo.NumIterations = numDecoderThreads;
      bpi->SetStartTime();
    }
    else
      encoder.NumIterations = encoders[0].NumIterations;

    #ifndef _7ZIP_ST
    {
      int numSubThreads = method.Get_NumThreads();
      encoder.NumDecoderSubThreads = (numSubThreads <= 0) ? 1 : numSubThreads;
    }
    if (numDecoderThreads > 1)
    {
      for (UInt32 j = 0; j < numSubDecoderThreads; j++)
      {
        HRESULT res = encoder.CreateDecoderThread(j, (i == 0 && j == 0)
            #ifdef USE_ALLOCA
            , ((i * numSubDecoderThreads + j) * 16 * 21) & 0x7FF
            #endif
            );
        RINOK(res);
      }
    }
    else
    #endif
    {
      RINOK(encoder.Decode(0));
    }
  }
  
  #ifndef _7ZIP_ST
  HRESULT res = S_OK;
  if (numDecoderThreads > 1)
    for (i = 0; i < numEncoderThreads; i++)
      for (UInt32 j = 0; j < numSubDecoderThreads; j++)
      {
        CEncoderInfo &encoder = encoders[i];
        encoder.thread[j].Wait();
        if (encoder.Results[j] != S_OK)
          res = encoder.Results[j];
      }
  RINOK(res);
  #endif
  
  RINOK(status.Res);
  encoders[0].progressInfoSpec[0]->SetFinishTime(info);
  
  #ifndef _7ZIP_ST
  #ifdef UNDER_CE
  if (numDecoderThreads > 1)
    for (i = 0; i < numEncoderThreads; i++)
      for (UInt32 j = 0; j < numSubDecoderThreads; j++)
      {
        FILETIME creationTime, exitTime, kernelTime, userTime;
        if (::GetThreadTimes(encoders[i].thread[j], &creationTime, &exitTime, &kernelTime, &userTime) != 0)
          info.UserTime += GetTime64(userTime) + GetTime64(kernelTime);
      }
  #endif
  #endif
  
  info.UnpackSize = 0;
  info.PackSize = 0;
  info.NumIterations = numSubDecoderThreads * encoders[0].NumIterations;
  
  for (i = 0; i < numEncoderThreads; i++)
  {
    CEncoderInfo &encoder = encoders[i];
    info.UnpackSize += encoder.kBufferSize;
    info.PackSize += encoder.compressedSize;
  }
  
  RINOK(callback->SetDecodeResult(info, false));
  RINOK(callback->SetDecodeResult(info, true));
  
  return S_OK;
}


static inline UInt64 GetLZMAUsage(bool multiThread, UInt32 dictionary)
{
  UInt32 hs = dictionary - 1;
  hs |= (hs >> 1);
  hs |= (hs >> 2);
  hs |= (hs >> 4);
  hs |= (hs >> 8);
  hs >>= 1;
  hs |= 0xFFFF;
  if (hs > (1 << 24))
    hs >>= 1;
  hs++;
  return ((hs + (1 << 16)) + (UInt64)dictionary * 2) * 4 + (UInt64)dictionary * 3 / 2 +
      (1 << 20) + (multiThread ? (6 << 20) : 0);
}

UInt64 GetBenchMemoryUsage(UInt32 numThreads, UInt32 dictionary, bool totalBench)
{
  const UInt32 kBufferSize = dictionary;
  const UInt32 kCompressedBufferSize = kBufferSize; // / 2;
  bool lzmaMt = (totalBench || numThreads > 1);
  UInt32 numBigThreads = numThreads;
  if (!totalBench && lzmaMt)
    numBigThreads /= 2;
  return ((UInt64)kBufferSize + kCompressedBufferSize +
    GetLZMAUsage(lzmaMt, dictionary) + (2 << 20)) * numBigThreads;
}

static HRESULT CrcBig(const void *data, UInt32 size, UInt64 numIterations,
    const UInt32 *checkSum, IHasher *hf,
    IBenchPrintCallback *callback)
{
  Byte hash[64];
  UInt64 i;
  for (i = 0; i < sizeof(hash); i++)
    hash[i] = 0;
  for (i = 0; i < numIterations; i++)
  {
    if (callback && (i & 0xFF) == 0)
    {
      RINOK(callback->CheckBreak());
    }
    hf->Init();
    hf->Update(data, size);
    hf->Final(hash);
    UInt32 hashSize = hf->GetDigestSize();
    if (hashSize > sizeof(hash))
      return S_FALSE;
    UInt32 sum = 0;
    for (UInt32 j = 0; j < hashSize; j += 4)
      sum ^= GetUi32(hash + j);
    if (checkSum && sum != *checkSum)
    {
      return S_FALSE;
    }
  }
  return S_OK;
}

UInt32 g_BenchCpuFreqTemp = 1;

#define YY1 sum += val; sum ^= val;
#define YY3 YY1 YY1 YY1 YY1
#define YY5 YY3 YY3 YY3 YY3
#define YY7 YY5 YY5 YY5 YY5
static const UInt32 kNumFreqCommands = 128;

EXTERN_C_BEGIN

static UInt32 CountCpuFreq(UInt32 sum, UInt32 num, UInt32 val)
{
  for (UInt32 i = 0; i < num; i++)
  {
    YY7
  }
  return sum;
}

EXTERN_C_END


#ifndef _7ZIP_ST

struct CFreqInfo
{
  NWindows::CThread Thread;
  IBenchPrintCallback *Callback;
  HRESULT CallbackRes;
  UInt32 ValRes;
  UInt32 Size;
  UInt64 NumIterations;

  void Wait()
  {
    Thread.Wait();
    Thread.Close();
  }
};

static THREAD_FUNC_DECL FreqThreadFunction(void *param)
{
  CFreqInfo *p = (CFreqInfo *)param;

  UInt32 sum = g_BenchCpuFreqTemp;
  for (UInt64 k = p->NumIterations; k > 0; k--)
  {
    p->CallbackRes = p->Callback->CheckBreak();
    if (p->CallbackRes != S_OK)
      return 0;
    sum = CountCpuFreq(sum, p->Size, g_BenchCpuFreqTemp);
  }
  p->ValRes = sum;
  return 0;
}

struct CFreqThreads
{
  CFreqInfo *Items;
  UInt32 NumThreads;

  CFreqThreads(): Items(NULL), NumThreads(0) {}
  void WaitAll()
  {
    for (UInt32 i = 0; i < NumThreads; i++)
      Items[i].Wait();
    NumThreads = 0;
  }
  ~CFreqThreads()
  {
    WaitAll();
    delete []Items;
  }
};

struct CCrcInfo
{
  NWindows::CThread Thread;
  IBenchPrintCallback *Callback;
  HRESULT CallbackRes;

  const Byte *Data;
  UInt32 Size;
  UInt64 NumIterations;
  bool CheckSumDefined;
  UInt32 CheckSum;
  CMyComPtr<IHasher> Hasher;
  HRESULT Res;

  #ifdef USE_ALLOCA
  size_t AllocaSize;
  #endif

  void Wait()
  {
    Thread.Wait();
    Thread.Close();
  }
};

static THREAD_FUNC_DECL CrcThreadFunction(void *param)
{
  CCrcInfo *p = (CCrcInfo *)param;
  
  #ifdef USE_ALLOCA
  alloca(p->AllocaSize);
  #endif

  p->Res = CrcBig(p->Data, p->Size, p->NumIterations,
      p->CheckSumDefined ? &p->CheckSum : NULL, p->Hasher,
      p->Callback);
  return 0;
}

struct CCrcThreads
{
  CCrcInfo *Items;
  UInt32 NumThreads;

  CCrcThreads(): Items(NULL), NumThreads(0) {}
  void WaitAll()
  {
    for (UInt32 i = 0; i < NumThreads; i++)
      Items[i].Wait();
    NumThreads = 0;
  }
  ~CCrcThreads()
  {
    WaitAll();
    delete []Items;
  }
};

#endif

static UInt32 CrcCalc1(const Byte *buf, UInt32 size)
{
  UInt32 crc = CRC_INIT_VAL;;
  for (UInt32 i = 0; i < size; i++)
    crc = CRC_UPDATE_BYTE(crc, buf[i]);
  return CRC_GET_DIGEST(crc);
}

static void RandGen(Byte *buf, UInt32 size, CBaseRandomGenerator &RG)
{
  for (UInt32 i = 0; i < size; i++)
    buf[i] = (Byte)RG.GetRnd();
}

static UInt32 RandGenCrc(Byte *buf, UInt32 size, CBaseRandomGenerator &RG)
{
  RandGen(buf, size, RG);
  return CrcCalc1(buf, size);
}

bool CrcInternalTest()
{
  CBenchBuffer buffer;
  const UInt32 kBufferSize0 = (1 << 8);
  const UInt32 kBufferSize1 = (1 << 10);
  const UInt32 kCheckSize = (1 << 5);
  if (!buffer.Alloc(kBufferSize0 + kBufferSize1))
    return false;
  Byte *buf = buffer.Buffer;
  UInt32 i;
  for (i = 0; i < kBufferSize0; i++)
    buf[i] = (Byte)i;
  UInt32 crc1 = CrcCalc1(buf, kBufferSize0);
  if (crc1 != 0x29058C73)
    return false;
  CBaseRandomGenerator RG;
  RandGen(buf + kBufferSize0, kBufferSize1, RG);
  for (i = 0; i < kBufferSize0 + kBufferSize1 - kCheckSize; i++)
    for (UInt32 j = 0; j < kCheckSize; j++)
      if (CrcCalc1(buf + i, j) != CrcCalc(buf + i, j))
        return false;
  return true;
}

struct CBenchMethod
{
  unsigned Weight;
  unsigned DictBits;
  UInt32 EncComplex;
  UInt32 DecComplexCompr;
  UInt32 DecComplexUnc;
  const char *Name;
};

static const CBenchMethod g_Bench[] =
{
  { 40, 17,  357,  145,   20, "LZMA:x1" },
  { 80, 24, 1220,  145,   20, "LZMA:x5:mt1" },
  { 80, 24, 1220,  145,   20, "LZMA:x5:mt2" },

  { 10, 16,  124,   40,   14, "Deflate:x1" },
  { 20, 16,  376,   40,   14, "Deflate:x5" },
  { 10, 16, 1082,   40,   14, "Deflate:x7" },
  { 10, 17,  422,   40,   14, "Deflate64:x5" },

  { 10, 15,  590,   69,   69, "BZip2:x1" },
  { 20, 19,  815,  122,  122, "BZip2:x5" },
  { 10, 19,  815,  122,  122, "BZip2:x5:mt2" },
  { 10, 19, 2530,  122,  122, "BZip2:x7" },

  { 10, 18, 1010,    0, 1150, "PPMD:x1" },
  { 10, 22, 1655,    0, 1830, "PPMD:x5" },

  {  2,  0,    6,    0,    6, "Delta:4" },
  {  2,  0,    4,    0,    4, "BCJ" },

  { 10,  0,   24,    0,   24, "AES256CBC:1" },
  {  2,  0,    8,    0,    2, "AES256CBC:2" }
};

struct CBenchHash
{
  unsigned Weight;
  UInt32 Complex;
  UInt32 CheckSum;
  const char *Name;
};

static const CBenchHash g_Hash[] =
{
  {  1,  1820, 0x8F8FEDAB, "CRC32:1" },
  { 10,   558, 0x8F8FEDAB, "CRC32:4" },
  { 10,   339, 0x8F8FEDAB, "CRC32:8" },
  { 10,   512, 0xDF1C17CC, "CRC64" },
  { 10,  5100, 0x2D79FF2E, "SHA256" },
  { 10,  2340, 0x4C25132B, "SHA1" },
  {  2,  5500, 0xE084E913, "BLAKE2sp" }
};

struct CTotalBenchRes
{
  // UInt64 NumIterations1; // for Usage
  UInt64 NumIterations2; // for Rating / RPU

  UInt64 Rating;
  UInt64 Usage;
  UInt64 RPU;
  
  void Init() { /* NumIterations1 = 0; */ NumIterations2 = 0; Rating = 0; Usage = 0; RPU = 0; }

  void SetSum(const CTotalBenchRes &r1, const CTotalBenchRes &r2)
  {
    Rating = (r1.Rating + r2.Rating);
    Usage = (r1.Usage + r2.Usage);
    RPU = (r1.RPU + r2.RPU);
    // NumIterations1 = (r1.NumIterations1 + r2.NumIterations1);
    NumIterations2 = (r1.NumIterations2 + r2.NumIterations2);
  }
};

static void PrintNumber(IBenchPrintCallback &f, UInt64 value, unsigned size)
{
  char s[128];
  unsigned startPos = (unsigned)sizeof(s) - 32;
  memset(s, ' ', startPos);
  ConvertUInt64ToString(value, s + startPos);
  // if (withSpace)
  {
    startPos--;
    size++;
  }
  unsigned len = (unsigned)strlen(s + startPos);
  if (size > len)
  {
    startPos -= (size - len);
    if (startPos < 0)
      startPos = 0;
  }
  f.Print(s + startPos);
}

static const unsigned kFieldSize_Name = 12;
static const unsigned kFieldSize_SmallName = 4;
static const unsigned kFieldSize_Speed = 9;
static const unsigned kFieldSize_Usage = 5;
static const unsigned kFieldSize_RU = 6;
static const unsigned kFieldSize_Rating = 6;
static const unsigned kFieldSize_EU = 5;
static const unsigned kFieldSize_Effec = 5;

static const unsigned kFieldSize_TotalSize = 4 + kFieldSize_Speed + kFieldSize_Usage + kFieldSize_RU + kFieldSize_Rating;
static const unsigned kFieldSize_EUAndEffec = 2 + kFieldSize_EU + kFieldSize_Effec;


static void PrintRating(IBenchPrintCallback &f, UInt64 rating, unsigned size)
{
  PrintNumber(f, (rating + 500000) / 1000000, size);
}


static void PrintPercents(IBenchPrintCallback &f, UInt64 val, UInt64 divider, unsigned size)
{
  PrintNumber(f, (val * 100 + divider / 2) / divider, size);
}

static void PrintChars(IBenchPrintCallback &f, char c, unsigned size)
{
  char s[256];
  memset(s, (Byte)c, size);
  s[size] = 0;
  f.Print(s);
}

static void PrintSpaces(IBenchPrintCallback &f, unsigned size)
{
  PrintChars(f, ' ', size);
}

static void PrintResults(IBenchPrintCallback &f, UInt64 usage, UInt64 rpu, UInt64 rating, bool showFreq, UInt64 cpuFreq)
{
  PrintNumber(f, (usage + 5000) / 10000, kFieldSize_Usage);
  PrintRating(f, rpu, kFieldSize_RU);
  PrintRating(f, rating, kFieldSize_Rating);
  if (showFreq)
  {
    if (cpuFreq == 0)
      PrintSpaces(f, kFieldSize_EUAndEffec);
    else
    {
      UInt64 ddd = cpuFreq * usage / 100;
      if (ddd == 0)
        ddd = 1;
      PrintPercents(f, (rating * 10000), ddd, kFieldSize_EU);
      PrintPercents(f, rating, cpuFreq, kFieldSize_Effec);
    }
  }
}

static void PrintResults(IBenchPrintCallback *f,
    const CBenchInfo &info,
    unsigned weight,
    UInt64 rating,
    bool showFreq, UInt64 cpuFreq,
    CTotalBenchRes *res)
{
  UInt64 speed = info.GetSpeed(info.UnpackSize * info.NumIterations);
  if (f)
  {
    if (speed != 0)
      PrintNumber(*f, speed / 1024, kFieldSize_Speed);
    else
      PrintSpaces(*f, 1 + kFieldSize_Speed);
  }
  UInt64 usage = info.GetUsage();
  UInt64 rpu = info.GetRatingPerUsage(rating);
  if (f)
  {
    PrintResults(*f, usage, rpu, rating, showFreq, cpuFreq);
  }

  if (res)
  {
    // res->NumIterations1++;
    res->NumIterations2 += weight;
    res->RPU += (rpu * weight);
    res->Rating += (rating * weight);
    res->Usage += (usage * weight);
  }
}

static void PrintTotals(IBenchPrintCallback &f, bool showFreq, UInt64 cpuFreq, const CTotalBenchRes &res)
{
  PrintSpaces(f, 1 + kFieldSize_Speed);
  // UInt64 numIterations1 = res.NumIterations1; if (numIterations1 == 0) numIterations1 = 1;
  UInt64 numIterations2 = res.NumIterations2; if (numIterations2 == 0) numIterations2 = 1;
  PrintResults(f, res.Usage / numIterations2, res.RPU / numIterations2, res.Rating / numIterations2, showFreq, cpuFreq);
}


static void PrintHex(AString &s, UInt64 v)
{
  char temp[32];
  ConvertUInt64ToHex(v, temp);
  s += temp;
}

AString GetProcessThreadsInfo(const NSystem::CProcessAffinity &ti)
{
  AString s;
  // s.Add_UInt32(ti.numProcessThreads);
  if (ti.processAffinityMask != ti.systemAffinityMask)
  {
    // if (ti.numProcessThreads != ti.numSysThreads)
    {
      s += " / ";
      s.Add_UInt32(ti.GetNumSystemThreads());
    }
    s += " : ";
    PrintHex(s, ti.processAffinityMask);
    s += " / ";
    PrintHex(s, ti.systemAffinityMask);
  }
  return s;
}


static void PrintSize(AString &s, UInt64 v)
{
  char c = 0;
  if ((v & 0x3FF) == 0) { v >>= 10; c = 'K';
  if ((v & 0x3FF) == 0) { v >>= 10; c = 'M';
  if ((v & 0x3FF) == 0) { v >>= 10; c = 'G';
  if ((v & 0x3FF) == 0) { v >>= 10; c = 'T';
  }}}}
  else
  {
    PrintHex(s, v);
    return;
  }
  char temp[32];
  ConvertUInt64ToString(v, temp);
  s += temp;
  if (c)
    s += c;
}


#ifdef _7ZIP_LARGE_PAGES

extern bool g_LargePagesMode;

extern "C"
{
  extern SIZE_T g_LargePageSize;
}

void Add_LargePages_String(AString &s)
{
  if (g_LargePagesMode || g_LargePageSize != 0)
  {
    s += " (LP-";
    PrintSize(s, g_LargePageSize);
    if (!g_LargePagesMode)
      s += "-NA";
    s += ")";
  }
}

#endif



static void PrintRequirements(IBenchPrintCallback &f, const char *sizeString,
    bool size_Defined, UInt64 size, const char *threadsString, UInt32 numThreads)
{
  f.Print("RAM ");
  f.Print(sizeString);
  if (size_Defined)
    PrintNumber(f, (size >> 20), 6);
  else
    f.Print("      ?");
  f.Print(" MB");
  
  #ifdef _7ZIP_LARGE_PAGES
  {
    AString s;
    Add_LargePages_String(s);
    f.Print(s);
  }
  #endif
  
  f.Print(",  # ");
  f.Print(threadsString);
  PrintNumber(f, numThreads, 3);
}



struct CBenchCallbackToPrint: public IBenchCallback
{
  CBenchProps BenchProps;
  CTotalBenchRes EncodeRes;
  CTotalBenchRes DecodeRes;
  IBenchPrintCallback *_file;
  UInt32 DictSize;

  bool Use2Columns;
  unsigned NameFieldSize;

  bool ShowFreq;
  UInt64 CpuFreq;

  unsigned EncodeWeight;
  unsigned DecodeWeight;

  CBenchCallbackToPrint():
      Use2Columns(false),
      NameFieldSize(0),
      ShowFreq(false),
      CpuFreq(0),
      EncodeWeight(1),
      DecodeWeight(1)
      {}

  void Init() { EncodeRes.Init(); DecodeRes.Init(); }
  void Print(const char *s);
  void NewLine();
  
  HRESULT SetFreq(bool showFreq, UInt64 cpuFreq);
  HRESULT SetEncodeResult(const CBenchInfo &info, bool final);
  HRESULT SetDecodeResult(const CBenchInfo &info, bool final);
};

HRESULT CBenchCallbackToPrint::SetFreq(bool showFreq, UInt64 cpuFreq)
{
  ShowFreq = showFreq;
  CpuFreq = cpuFreq;
  return S_OK;
}

HRESULT CBenchCallbackToPrint::SetEncodeResult(const CBenchInfo &info, bool final)
{
  RINOK(_file->CheckBreak());
  if (final)
  {
    UInt64 rating = BenchProps.GetCompressRating(DictSize, info.GlobalTime, info.GlobalFreq, info.UnpackSize * info.NumIterations);
    PrintResults(_file, info,
        EncodeWeight, rating,
        ShowFreq, CpuFreq, &EncodeRes);
    if (!Use2Columns)
      _file->NewLine();
  }
  return S_OK;
}

static const char * const kSep = "  | ";

HRESULT CBenchCallbackToPrint::SetDecodeResult(const CBenchInfo &info, bool final)
{
  RINOK(_file->CheckBreak());
  if (final)
  {
    UInt64 rating = BenchProps.GetDecompressRating(info.GlobalTime, info.GlobalFreq, info.UnpackSize, info.PackSize, info.NumIterations);
    if (Use2Columns)
      _file->Print(kSep);
    else
      PrintSpaces(*_file, NameFieldSize);
    CBenchInfo info2 = info;
    info2.UnpackSize *= info2.NumIterations;
    info2.PackSize *= info2.NumIterations;
    info2.NumIterations = 1;
    PrintResults(_file, info2,
        DecodeWeight, rating,
        ShowFreq, CpuFreq, &DecodeRes);
  }
  return S_OK;
}

void CBenchCallbackToPrint::Print(const char *s)
{
  _file->Print(s);
}

void CBenchCallbackToPrint::NewLine()
{
  _file->NewLine();
}

void PrintLeft(IBenchPrintCallback &f, const char *s, unsigned size)
{
  f.Print(s);
  int numSpaces = size - MyStringLen(s);
  if (numSpaces > 0)
    PrintSpaces(f, numSpaces);
}

void PrintRight(IBenchPrintCallback &f, const char *s, unsigned size)
{
  int numSpaces = size - MyStringLen(s);
  if (numSpaces > 0)
    PrintSpaces(f, numSpaces);
  f.Print(s);
}

static HRESULT TotalBench(
    DECL_EXTERNAL_CODECS_LOC_VARS
    UInt64 complexInCommands,
    UInt32 numThreads,
    bool forceUnpackSize,
    size_t unpackSize,
    const Byte *fileData,
    IBenchPrintCallback *printCallback, CBenchCallbackToPrint *callback)
{
  for (unsigned i = 0; i < ARRAY_SIZE(g_Bench); i++)
  {
    const CBenchMethod &bench = g_Bench[i];
    PrintLeft(*callback->_file, bench.Name, kFieldSize_Name);
    callback->BenchProps.DecComplexUnc = bench.DecComplexUnc;
    callback->BenchProps.DecComplexCompr = bench.DecComplexCompr;
    callback->BenchProps.EncComplex = bench.EncComplex;
    
    COneMethodInfo method;
    NCOM::CPropVariant propVariant;
    propVariant = bench.Name;
    RINOK(method.ParseMethodFromPROPVARIANT(UString(), propVariant));

    size_t unpackSize2 = unpackSize;
    if (!forceUnpackSize && bench.DictBits == 0)
      unpackSize2 = kFilterUnpackSize;

    callback->EncodeWeight = bench.Weight;
    callback->DecodeWeight = bench.Weight;

    HRESULT res = MethodBench(
        EXTERNAL_CODECS_LOC_VARS
        complexInCommands,
        false, numThreads, method,
        unpackSize2, fileData,
        bench.DictBits,
        printCallback, callback, &callback->BenchProps);
    
    if (res == E_NOTIMPL)
    {
      // callback->Print(" ---");
      // we need additional empty line as line for decompression results
      if (!callback->Use2Columns)
        callback->NewLine();
    }
    else
    {
      RINOK(res);
    }
    
    callback->NewLine();
  }
  return S_OK;
}


static HRESULT FreqBench(
    UInt64 complexInCommands,
    UInt32 numThreads,
    IBenchPrintCallback *_file,
    bool showFreq,
    UInt64 specifiedFreq,
    UInt64 &cpuFreq,
    UInt32 &res)
{
  res = 0;
  cpuFreq = 0;

  UInt32 bufferSize = 1 << 20;
  UInt32 complexity = kNumFreqCommands;
  if (numThreads == 0)
    numThreads = 1;

  #ifdef _7ZIP_ST
  numThreads = 1;
  #endif

  UInt32 bsize = (bufferSize == 0 ? 1 : bufferSize);
  UInt64 numIterations = complexInCommands / complexity / bsize;
  if (numIterations == 0)
    numIterations = 1;

  CBenchInfoCalc progressInfoSpec;

  #ifndef _7ZIP_ST
  CFreqThreads threads;
  if (numThreads > 1)
  {
    threads.Items = new CFreqInfo[numThreads];
    UInt32 i;
    for (i = 0; i < numThreads; i++)
    {
      CFreqInfo &info = threads.Items[i];
      info.Callback = _file;
      info.CallbackRes = S_OK;
      info.NumIterations = numIterations;
      info.Size = bufferSize;
    }
    progressInfoSpec.SetStartTime();
    for (i = 0; i < numThreads; i++)
    {
      CFreqInfo &info = threads.Items[i];
      RINOK(info.Thread.Create(FreqThreadFunction, &info));
      threads.NumThreads++;
    }
    threads.WaitAll();
    for (i = 0; i < numThreads; i++)
    {
      RINOK(threads.Items[i].CallbackRes);
    }
  }
  else
  #endif
  {
    progressInfoSpec.SetStartTime();
    UInt32 sum = g_BenchCpuFreqTemp;
    for (UInt64 k = numIterations; k > 0; k--)
    {
      RINOK(_file->CheckBreak());
      sum = CountCpuFreq(sum, bufferSize, g_BenchCpuFreqTemp);
    }
    res += sum;
  }
  
  CBenchInfo info;
  progressInfoSpec.SetFinishTime(info);

  info.UnpackSize = 0;
  info.PackSize = 0;
  info.NumIterations = 1;

  if (_file)
  {
    {
      UInt64 numCommands = (UInt64)numIterations * bufferSize * numThreads * complexity;
      UInt64 rating = info.GetSpeed(numCommands);
      cpuFreq = rating / numThreads;
      PrintResults(_file, info,
          0, // weight
          rating,
          showFreq, showFreq ? (specifiedFreq != 0 ? specifiedFreq : cpuFreq) : 0, NULL);
    }
    RINOK(_file->CheckBreak());
  }

  return S_OK;
}



static HRESULT CrcBench(
    DECL_EXTERNAL_CODECS_LOC_VARS
    UInt64 complexInCommands,
    UInt32 numThreads, UInt32 bufferSize,
    UInt64 &speed,
    UInt32 complexity, unsigned benchWeight,
    const UInt32 *checkSum,
    const COneMethodInfo &method,
    IBenchPrintCallback *_file,
    CTotalBenchRes *encodeRes,
    bool showFreq, UInt64 cpuFreq)
{
  if (numThreads == 0)
    numThreads = 1;

  #ifdef _7ZIP_ST
  numThreads = 1;
  #endif

  const AString &methodName = method.MethodName;
  // methodName.RemoveChar(L'-');
  CMethodId hashID;
  if (!FindHashMethod(
      EXTERNAL_CODECS_LOC_VARS
      methodName, hashID))
    return E_NOTIMPL;

  CBenchBuffer buffer;
  size_t totalSize = (size_t)bufferSize * numThreads;
  if (totalSize / numThreads != bufferSize)
    return E_OUTOFMEMORY;
  if (!buffer.Alloc(totalSize))
    return E_OUTOFMEMORY;

  Byte *buf = buffer.Buffer;
  CBaseRandomGenerator RG;
  UInt32 bsize = (bufferSize == 0 ? 1 : bufferSize);
  UInt64 numIterations = complexInCommands * 256 / complexity / bsize;
  if (numIterations == 0)
    numIterations = 1;

  CBenchInfoCalc progressInfoSpec;

  #ifndef _7ZIP_ST
  CCrcThreads threads;
  if (numThreads > 1)
  {
    threads.Items = new CCrcInfo[numThreads];
    
    UInt32 i;
    for (i = 0; i < numThreads; i++)
    {
      CCrcInfo &info = threads.Items[i];
      AString name;
      RINOK(CreateHasher(EXTERNAL_CODECS_LOC_VARS hashID, name, info.Hasher));
      if (!info.Hasher)
        return E_NOTIMPL;
      CMyComPtr<ICompressSetCoderProperties> scp;
      info.Hasher.QueryInterface(IID_ICompressSetCoderProperties, &scp);
      if (scp)
      {
        UInt64 reduceSize = 1;
        RINOK(method.SetCoderProps(scp, &reduceSize));
      }

      Byte *data = buf + (size_t)bufferSize * i;
      info.Callback = _file;
      info.Data = data;
      info.NumIterations = numIterations;
      info.Size = bufferSize;
      /* info.Crc = */ RandGenCrc(data, bufferSize, RG);
      info.CheckSumDefined = false;
      if (checkSum)
      {
        info.CheckSum = *checkSum;
        info.CheckSumDefined = (checkSum && (i == 0));
      }

      #ifdef USE_ALLOCA
      info.AllocaSize = (i * 16 * 21) & 0x7FF;
      #endif
    }

    progressInfoSpec.SetStartTime();
    
    for (i = 0; i < numThreads; i++)
    {
      CCrcInfo &info = threads.Items[i];
      RINOK(info.Thread.Create(CrcThreadFunction, &info));
      threads.NumThreads++;
    }
    threads.WaitAll();
    for (i = 0; i < numThreads; i++)
    {
      RINOK(threads.Items[i].Res);
    }
  }
  else
  #endif
  {
    /* UInt32 crc = */ RandGenCrc(buf, bufferSize, RG);
    progressInfoSpec.SetStartTime();
    CMyComPtr<IHasher> hasher;
    AString name;
    RINOK(CreateHasher(EXTERNAL_CODECS_LOC_VARS hashID, name, hasher));
    if (!hasher)
      return E_NOTIMPL;
    CMyComPtr<ICompressSetCoderProperties> scp;
    hasher.QueryInterface(IID_ICompressSetCoderProperties, &scp);
    if (scp)
    {
      UInt64 reduceSize = 1;
      RINOK(method.SetCoderProps(scp, &reduceSize));
    }
    RINOK(CrcBig(buf, bufferSize, numIterations, checkSum, hasher, _file));
  }

  CBenchInfo info;
  progressInfoSpec.SetFinishTime(info);

  UInt64 unpSize = numIterations * bufferSize;
  UInt64 unpSizeThreads = unpSize * numThreads;
  info.UnpackSize = unpSizeThreads;
  info.PackSize = unpSizeThreads;
  info.NumIterations = 1;

  if (_file)
  {
    {
      UInt64 numCommands = unpSizeThreads * complexity / 256;
      UInt64 rating = info.GetSpeed(numCommands);
      PrintResults(_file, info,
          benchWeight, rating,
          showFreq, cpuFreq, encodeRes);
    }
    RINOK(_file->CheckBreak());
  }

  speed = info.GetSpeed(unpSizeThreads);

  return S_OK;
}

static HRESULT TotalBench_Hash(
    DECL_EXTERNAL_CODECS_LOC_VARS
    UInt64 complexInCommands,
    UInt32 numThreads, UInt32 bufSize,
    IBenchPrintCallback *printCallback, CBenchCallbackToPrint *callback,
    CTotalBenchRes *encodeRes,
    bool showFreq, UInt64 cpuFreq)
{
  for (unsigned i = 0; i < ARRAY_SIZE(g_Hash); i++)
  {
    const CBenchHash &bench = g_Hash[i];
    PrintLeft(*callback->_file, bench.Name, kFieldSize_Name);
    // callback->BenchProps.DecComplexUnc = bench.DecComplexUnc;
    // callback->BenchProps.DecComplexCompr = bench.DecComplexCompr;
    // callback->BenchProps.EncComplex = bench.EncComplex;

    COneMethodInfo method;
    NCOM::CPropVariant propVariant;
    propVariant = bench.Name;
    RINOK(method.ParseMethodFromPROPVARIANT(UString(), propVariant));

    UInt64 speed;
    HRESULT res = CrcBench(
        EXTERNAL_CODECS_LOC_VARS
        complexInCommands,
        numThreads, bufSize,
        speed,
        bench.Complex, bench.Weight,
        &bench.CheckSum, method,
        printCallback, encodeRes, showFreq, cpuFreq);
    if (res == E_NOTIMPL)
    {
      // callback->Print(" ---");
    }
    else
    {
      RINOK(res);
    }
    callback->NewLine();
  }
  return S_OK;
}

struct CTempValues
{
  UInt64 *Values;
  CTempValues(UInt32 num) { Values = new UInt64[num]; }
  ~CTempValues() { delete []Values; }
};

static void ParseNumberString(const UString &s, NCOM::CPropVariant &prop)
{
  const wchar_t *end;
  UInt64 result = ConvertStringToUInt64(s, &end);
  if (*end != 0 || s.IsEmpty())
    prop = s;
  else if (result <= (UInt32)0xFFFFFFFF)
    prop = (UInt32)result;
  else
    prop = result;
}

static UInt32 GetNumThreadsNext(unsigned i, UInt32 numThreads)
{
  if (i < 2)
    return i + 1;
  i -= 1;
  UInt32 num = (UInt32)(2 + (i & 1)) << (i >> 1);
  return (num <= numThreads) ? num : numThreads;
}

static bool AreSameMethodNames(const char *fullName, const char *shortName)
{
  return StringsAreEqualNoCase_Ascii(fullName, shortName);
}


#ifdef MY_CPU_X86_OR_AMD64

static void PrintCpuChars(AString &s, UInt32 v)
{
  for (int j = 0; j < 4; j++)
  {
    Byte b = (Byte)(v & 0xFF);
    v >>= 8;
    if (b == 0)
      break;
    s += (char)b;
  }
}

static void x86cpuid_to_String(const Cx86cpuid &c, AString &s)
{
  s.Empty();

  UInt32 maxFunc2 = 0;
  UInt32 t[3];

  MyCPUID(0x80000000, &maxFunc2, &t[0], &t[1], &t[2]);

  bool fullNameIsAvail = (maxFunc2 >= 0x80000004);
  
  if (!fullNameIsAvail)
  {
    for (int i = 0; i < 3; i++)
      PrintCpuChars(s, c.vendor[i]);
  }
  else
  {
    for (int i = 0; i < 3; i++)
    {
      UInt32 d[4] = { 0 };
      MyCPUID(0x80000002 + i, &d[0], &d[1], &d[2], &d[3]);
      for (int j = 0; j < 4; j++)
        PrintCpuChars(s, d[j]);
    }
  }

  s.Add_Space_if_NotEmpty();
  {
    char temp[32];
    ConvertUInt32ToHex(c.ver, temp);
    s += '(';
    s += temp;
    s += ')';
  }
}

#endif



static const char * const k_PROCESSOR_ARCHITECTURE[] =
{
    "x86" // "INTEL"
  , "MIPS"
  , "ALPHA"
  , "PPC"
  , "SHX"
  , "ARM"
  , "IA64"
  , "ALPHA64"
  , "MSIL"
  , "x64" // "AMD64"
  , "IA32_ON_WIN64"
  , "NEUTRAL"
  , "ARM64"
  , "ARM32_ON_WIN64"
};

#define MY__PROCESSOR_ARCHITECTURE_INTEL 0
#define MY__PROCESSOR_ARCHITECTURE_AMD64 9


#define MY__PROCESSOR_INTEL_PENTIUM  586
#define MY__PROCESSOR_AMD_X8664      8664

/*
static const CUInt32PCharPair k_PROCESSOR[] =
{
  { 2200, "IA64" },
  { 8664, "x64" }
};

#define PROCESSOR_INTEL_386      386
#define PROCESSOR_INTEL_486      486
#define PROCESSOR_INTEL_PENTIUM  586
#define PROCESSOR_INTEL_860      860
#define PROCESSOR_INTEL_IA64     2200
#define PROCESSOR_AMD_X8664      8664
#define PROCESSOR_MIPS_R2000     2000
#define PROCESSOR_MIPS_R3000     3000
#define PROCESSOR_MIPS_R4000     4000
#define PROCESSOR_ALPHA_21064    21064
#define PROCESSOR_PPC_601        601
#define PROCESSOR_PPC_603        603
#define PROCESSOR_PPC_604        604
#define PROCESSOR_PPC_620        620
#define PROCESSOR_HITACHI_SH3    10003
#define PROCESSOR_HITACHI_SH3E   10004
#define PROCESSOR_HITACHI_SH4    10005
#define PROCESSOR_MOTOROLA_821   821
#define PROCESSOR_SHx_SH3        103
#define PROCESSOR_SHx_SH4        104
#define PROCESSOR_STRONGARM      2577    // 0xA11
#define PROCESSOR_ARM720         1824    // 0x720
#define PROCESSOR_ARM820         2080    // 0x820
#define PROCESSOR_ARM920         2336    // 0x920
#define PROCESSOR_ARM_7TDMI      70001
#define PROCESSOR_OPTIL          18767   // 0x494f
*/

#ifdef _WIN32

static const char * const k_PF[] =
{
    "FP_ERRATA"
  , "FP_EMU"
  , "CMPXCHG"
  , "MMX"
  , "PPC_MOVEMEM_64BIT"
  , "ALPHA_BYTE"
  , "SSE"
  , "3DNOW"
  , "RDTSC"
  , "PAE"
  , "SSE2"
  , "SSE_DAZ"
  , "NX"
  , "SSE3"
  , "CMPXCHG16B"
  , "CMP8XCHG16"
  , "CHANNELS"
  , "XSAVE"
  , "ARM_VFP_32"
  , "ARM_NEON"
  , "L2AT"
  , "VIRT_FIRMWARE"
  , "RDWRFSGSBASE"
  , "FASTFAIL"
  , "ARM_DIVIDE"
  , "ARM_64BIT_LOADSTORE_ATOMIC"
  , "ARM_EXTERNAL_CACHE"
  , "ARM_FMAC"
  , "RDRAND"
  , "ARM_V8"
  , "ARM_V8_CRYPTO"
  , "ARM_V8_CRC32"
  , "RDTSCP"
};

#endif




static void PrintPage(AString &s, UInt32 v)
{
  if ((v & 0x3FF) == 0)
  {
    s.Add_UInt32(v >> 10);
    s += "K";
  }
  else
    s.Add_UInt32(v >> 10);
}

static AString TypeToString2(const char * const table[], unsigned num, UInt32 value)
{
  char sz[16];
  const char *p = NULL;
  if (value < num)
    p = table[value];
  if (!p)
  {
    ConvertUInt32ToString(value, sz);
    p = sz;
  }
  return (AString)p;
}

#ifdef _WIN32

static void SysInfo_To_String(AString &s, const SYSTEM_INFO &si)
{
  s += TypeToString2(k_PROCESSOR_ARCHITECTURE, ARRAY_SIZE(k_PROCESSOR_ARCHITECTURE), si.wProcessorArchitecture);

  if (!(   si.wProcessorArchitecture == MY__PROCESSOR_ARCHITECTURE_INTEL && si.dwProcessorType == MY__PROCESSOR_INTEL_PENTIUM
      || si.wProcessorArchitecture == MY__PROCESSOR_ARCHITECTURE_AMD64 && si.dwProcessorType == MY__PROCESSOR_AMD_X8664))
  {
    s += " ";
    // s += TypePairToString(k_PROCESSOR, ARRAY_SIZE(k_PROCESSOR), si.dwProcessorType);
    s.Add_UInt32(si.dwProcessorType);
  }
  s += " ";
  PrintHex(s, si.wProcessorLevel);
  s += ".";
  PrintHex(s, si.wProcessorRevision);
  if ((UInt64)si.dwActiveProcessorMask + 1 != ((UInt64)1 << si.dwNumberOfProcessors))
  if ((UInt64)si.dwActiveProcessorMask + 1 != 0 || si.dwNumberOfProcessors != sizeof(UInt64) * 8)
  {
    s += " act:";
    PrintHex(s, si.dwActiveProcessorMask);
  }
  s += " cpus:";
  s.Add_UInt32(si.dwNumberOfProcessors);
  if (si.dwPageSize != 1 << 12)
  {
    s += " page:";
    PrintPage(s, si.dwPageSize);
  }
  if (si.dwAllocationGranularity != 1 << 16)
  {
    s += " gran:";
    PrintPage(s, si.dwAllocationGranularity);
  }
  s += " ";

  DWORD_PTR minAdd = (DWORD_PTR)si.lpMinimumApplicationAddress;
  UInt64 maxSize = (UInt64)(DWORD_PTR)si.lpMaximumApplicationAddress + 1;
  const UInt32 kReserveSize = ((UInt32)1 << 16);
  if (minAdd != kReserveSize)
  {
    PrintSize(s, minAdd);
    s += "-";
  }
  else
  {
    if ((maxSize & (kReserveSize - 1)) == 0)
      maxSize += kReserveSize;
  }
  PrintSize(s, maxSize);
}

#ifndef _WIN64
typedef VOID (WINAPI *Func_GetNativeSystemInfo)(LPSYSTEM_INFO lpSystemInfo);
#endif

#endif

void GetSysInfo(AString &s1, AString &s2)
{
  s1.Empty();
  s2.Empty();

  #ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    {
      SysInfo_To_String(s1, si);
      // s += " : ";
    }
    
    #if !defined(_WIN64) && !defined(UNDER_CE)
    Func_GetNativeSystemInfo fn_GetNativeSystemInfo = (Func_GetNativeSystemInfo)GetProcAddress(
        GetModuleHandleA("kernel32.dll"), "GetNativeSystemInfo");
    if (fn_GetNativeSystemInfo)
    {
      SYSTEM_INFO si2;
      fn_GetNativeSystemInfo(&si2);
      // if (memcmp(&si, &si2, sizeof(si)) != 0)
      {
        // s += " - ";
        SysInfo_To_String(s2, si2);
      }
    }
    #endif
  #endif
}


void GetCpuName(AString &s)
{
  s.Empty();

  #ifdef MY_CPU_X86_OR_AMD64
  {
    Cx86cpuid cpuid;
    if (x86cpuid_CheckAndRead(&cpuid))
    {
      AString s2;
      x86cpuid_to_String(cpuid, s2);
      s += s2;
    }
    else
    {
    #ifdef MY_CPU_AMD64
    s += "x64";
    #else
    s += "x86";
    #endif
    }
  }
  #else
  
    #ifdef MY_CPU_LE
      s += "LE";
    #elif defined(MY_CPU_BE)
      s += "BE";
    #endif

  #endif

  #ifdef _7ZIP_LARGE_PAGES
  Add_LargePages_String(s);
  #endif
}


void GetCpuFeatures(AString &s)
{
  s.Empty();
  
  #ifdef _WIN32
  const unsigned kNumFeatures_Extra = 32; // we check also for unknown features
  const unsigned kNumFeatures = ARRAY_SIZE(k_PF) + kNumFeatures_Extra;
  for (unsigned i = 0; i < kNumFeatures; i++)
  {
    if (IsProcessorFeaturePresent(i))
    {
      s.Add_Space_if_NotEmpty();
      s += TypeToString2(k_PF, ARRAY_SIZE(k_PF), i);
    }
  }
  #endif
}


#ifdef _WIN32
#ifndef UNDER_CE

typedef void (WINAPI * Func_RtlGetVersion) (OSVERSIONINFOEXW *);

static BOOL My_RtlGetVersion(OSVERSIONINFOEXW *vi)
{
  HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
  if (!ntdll)
    return FALSE;
  Func_RtlGetVersion func = (Func_RtlGetVersion)GetProcAddress(ntdll, "RtlGetVersion");
  if (!func)
    return FALSE;
  func(vi);
  return TRUE;
}

#endif
#endif


HRESULT Bench(
    DECL_EXTERNAL_CODECS_LOC_VARS
    IBenchPrintCallback *printCallback,
    IBenchCallback *benchCallback,
    // IBenchFreqCallback *freqCallback,
    const CObjectVector<CProperty> &props,
    UInt32 numIterations,
    bool multiDict)
{
  if (!CrcInternalTest())
    return S_FALSE;

  UInt32 numCPUs = 1;
  UInt64 ramSize = (UInt64)(sizeof(size_t)) << 29;

  NSystem::CProcessAffinity threadsInfo;
  threadsInfo.InitST();

  #ifndef _7ZIP_ST

  if (threadsInfo.Get() && threadsInfo.processAffinityMask != 0)
    numCPUs = threadsInfo.GetNumProcessThreads();
  else
    numCPUs = NSystem::GetNumberOfProcessors();

  #endif
  
  bool ramSize_Defined = NSystem::GetRamSize(ramSize);

  UInt32 numThreadsSpecified = numCPUs;

  UInt32 testTime = kComplexInSeconds;

  UInt64 specifiedFreq = 0;

  bool multiThreadTests = false;

  COneMethodInfo method;

  CBenchBuffer fileDataBuffer;

  {
  unsigned i;
  for (i = 0; i < props.Size(); i++)
  {
    const CProperty &property = props[i];
    UString name (property.Name);
    name.MakeLower_Ascii();

    if (name.IsEqualTo("file"))
    {
      if (property.Value.IsEmpty())
        return E_INVALIDARG;

      #ifdef USE_WIN_FILE
      
      NFile::NIO::CInFile file;
      if (!file.Open(us2fs(property.Value)))
        return E_INVALIDARG;
      UInt64 len;
      if (!file.GetLength(len))
        return E_FAIL;
      if (len >= ((UInt32)1 << 31) || len == 0)
        return E_INVALIDARG;
      if (!fileDataBuffer.Alloc((size_t)len))
        return E_OUTOFMEMORY;
      UInt32 processedSize;
      file.Read(fileDataBuffer.Buffer, (UInt32)len, processedSize);
      if (processedSize != len)
        return E_FAIL;
      if (printCallback)
      {
        printCallback->Print("file size =");
        PrintNumber(*printCallback, len, 0);
        printCallback->NewLine();
      }
      continue;

      #else

      return E_NOTIMPL;
      
      #endif
    }

    NCOM::CPropVariant propVariant;
    if (!property.Value.IsEmpty())
      ParseNumberString(property.Value, propVariant);
    
    if (name.IsEqualTo("time"))
    {
      RINOK(ParsePropToUInt32(UString(), propVariant, testTime));
      continue;
    }
    
    if (name.IsEqualTo("freq"))
    {
      UInt32 freq32 = 0;
      RINOK(ParsePropToUInt32(UString(), propVariant, freq32));
      if (freq32 == 0)
        return E_INVALIDARG;
      specifiedFreq = (UInt64)freq32 * 1000000;

      if (printCallback)
      {
        printCallback->Print("freq=");
        PrintNumber(*printCallback, freq32, 0);
        printCallback->NewLine();
      }

      continue;
    }

    if (name.IsPrefixedBy_Ascii_NoCase("mt"))
    {
      UString s = name.Ptr(2);
      if (s.IsEqualTo("*")
          || s.IsEmpty() && propVariant.vt == VT_BSTR && StringsAreEqual_Ascii(propVariant.bstrVal, "*"))
      {
        multiThreadTests = true;
        continue;
      }
      #ifndef _7ZIP_ST
      RINOK(ParseMtProp(s, propVariant, numCPUs, numThreadsSpecified));
      #endif
      continue;
    }
    
    RINOK(method.ParseMethodFromPROPVARIANT(name, propVariant));
  }
  }

  if (printCallback)
  {
    #ifdef _WIN32
    #ifndef UNDER_CE
    {
      AString s;
      // OSVERSIONINFO vi;
      OSVERSIONINFOEXW vi;
      vi.dwOSVersionInfoSize = sizeof(vi);
      // if (::GetVersionEx(&vi))
      if (My_RtlGetVersion(&vi))
      {
        s += "Windows";
        if (vi.dwPlatformId != VER_PLATFORM_WIN32_NT)
          s.Add_UInt32(vi.dwPlatformId);
        s += " "; s.Add_UInt32(vi.dwMajorVersion);
        s += "."; s.Add_UInt32(vi.dwMinorVersion);
        s += " "; s.Add_UInt32(vi.dwBuildNumber);
        // s += " "; s += GetAnsiString(vi.szCSDVersion);
      }
      printCallback->Print(s);
      printCallback->NewLine();
    }
    #endif
    #endif

    {
      AString s1, s2;
      GetSysInfo(s1, s2);
      if (!s1.IsEmpty() || !s2.IsEmpty())
      {
        printCallback->Print(s1);
        if (s1 != s2 && !s2.IsEmpty())
        {
          printCallback->Print(" - ");
          printCallback->Print(s2);
        }
        printCallback->NewLine();
      }
    }
    {
      AString s;
      GetCpuFeatures(s);
      if (!s.IsEmpty())
      {
        printCallback->Print(s);
        printCallback->NewLine();
      }
    }
    {
      AString s;
      GetCpuName(s);
      if (!s.IsEmpty())
      {
        printCallback->Print(s);
        printCallback->NewLine();
      }
    }
  }

  if (printCallback)
  {
    printCallback->Print("CPU Freq:");
  }

  UInt64 complexInCommands = kComplexInCommands;

  if (printCallback /* || freqCallback */)
  {
    UInt64 numMilCommands = 1 << 6;
    if (specifiedFreq != 0)
    {
      while (numMilCommands > 1 && specifiedFreq < (numMilCommands * 1000000))
        numMilCommands >>= 1;
    }

    for (int jj = 0;; jj++)
    {
      if (printCallback)
        RINOK(printCallback->CheckBreak());

      UInt64 start = ::GetTimeCount();
      UInt32 sum = (UInt32)start;
      sum = CountCpuFreq(sum, (UInt32)(numMilCommands * 1000000 / kNumFreqCommands), g_BenchCpuFreqTemp);
      if (sum == 0xF1541213)
        if (printCallback)
          printCallback->Print("");
      const UInt64 realDelta = ::GetTimeCount() - start;
      start = realDelta;
      if (start == 0)
        start = 1;
      UInt64 freq = GetFreq();
      // mips is constant in some compilers
      const UInt64 mipsVal = numMilCommands * freq / start;
      if (printCallback)
      {
        if (realDelta == 0)
        {
          printCallback->Print(" -");
        }
        else
        {
          // PrintNumber(*printCallback, start, 0);
          PrintNumber(*printCallback, mipsVal, 5);
        }
      }
      /*
      if (freqCallback)
        freqCallback->AddCpuFreq(mipsVal);
      */

      if (jj >= 3)
      {
        SetComplexCommands(testTime, false, mipsVal * 1000000, complexInCommands);
        if (jj >= 8 || start >= freq)
          break;
        // break; // change it
        numMilCommands <<= 1;
      }
    }
  }

  if (printCallback)
  {
    printCallback->NewLine();
    printCallback->NewLine();
    PrintRequirements(*printCallback, "size: ", ramSize_Defined, ramSize, "CPU hardware threads:", numCPUs);
    printCallback->Print(GetProcessThreadsInfo(threadsInfo));
    printCallback->NewLine();
  }

  if (numThreadsSpecified < 1 || numThreadsSpecified > kNumThreadsMax)
    return E_INVALIDARG;

  UInt32 dict;
  bool dictIsDefined = method.Get_DicSize(dict);

  if (method.MethodName.IsEmpty())
    method.MethodName = "LZMA";

  if (benchCallback)
  {
    CBenchProps benchProps;
    benchProps.SetLzmaCompexity();
    UInt32 dictSize = method.Get_Lzma_DicSize();
    UInt32 uncompressedDataSize = kAdditionalSize + dictSize;
    return MethodBench(
        EXTERNAL_CODECS_LOC_VARS
        complexInCommands,
        true, numThreadsSpecified,
        method,
        uncompressedDataSize, fileDataBuffer.Buffer,
        kOldLzmaDictBits, printCallback, benchCallback, &benchProps);
  }

  AString methodName (method.MethodName);
  if (methodName.IsEqualTo_Ascii_NoCase("CRC"))
    methodName = "crc32";
  method.MethodName = methodName;
  CMethodId hashID;
  
  if (FindHashMethod(EXTERNAL_CODECS_LOC_VARS methodName, hashID))
  {
    if (!printCallback)
      return S_FALSE;
    IBenchPrintCallback &f = *printCallback;
    if (!dictIsDefined)
      dict = (1 << 24);


    // methhodName.RemoveChar(L'-');
    UInt32 complexity = 10000;
    const UInt32 *checkSum = NULL;
    {
      unsigned i;
      for (i = 0; i < ARRAY_SIZE(g_Hash); i++)
      {
        const CBenchHash &h = g_Hash[i];
        AString benchMethod (h.Name);
        AString benchProps;
        int propPos = benchMethod.Find(':');
        if (propPos >= 0)
        {
          benchProps = benchMethod.Ptr(propPos + 1);
          benchMethod.DeleteFrom(propPos);
        }

        if (AreSameMethodNames(benchMethod, methodName))
        {
          if (benchProps.IsEmpty()
              || benchMethod.IsEqualTo_Ascii_NoCase("crc32") && benchProps == "8" && method.PropsString.IsEmpty()
              || method.PropsString.IsPrefixedBy_Ascii_NoCase(benchProps))
          {
            complexity = h.Complex;
            checkSum = &h.CheckSum;
            if (method.PropsString.IsEqualTo_Ascii_NoCase(benchProps))
              break;
          }
        }
      }
      if (i == ARRAY_SIZE(g_Hash))
        return E_NOTIMPL;
    }

    f.NewLine();
    f.Print("Size");
    const unsigned kFieldSize_CrcSpeed = 6;
    unsigned numThreadsTests = 0;
    for (;;)
    {
      UInt32 t = GetNumThreadsNext(numThreadsTests, numThreadsSpecified);
      PrintNumber(f, t, kFieldSize_CrcSpeed);
      numThreadsTests++;
      if (t >= numThreadsSpecified)
        break;
    }
    f.NewLine();
    f.NewLine();
    CTempValues speedTotals(numThreadsTests);
    {
      for (unsigned ti = 0; ti < numThreadsTests; ti++)
        speedTotals.Values[ti] = 0;
    }
    
    UInt64 numSteps = 0;
    for (UInt32 i = 0; i < numIterations; i++)
    {
      for (unsigned pow = 10; pow < 32; pow++)
      {
        UInt32 bufSize = (UInt32)1 << pow;
        if (bufSize > dict)
          break;
        char s[16];
        ConvertUInt32ToString(pow, s);
        unsigned pos = MyStringLen(s);
        s[pos++] = ':';
        s[pos++] = ' ';
        s[pos] = 0;
        f.Print(s);

        for (unsigned ti = 0; ti < numThreadsTests; ti++)
        {
          RINOK(f.CheckBreak());
          UInt32 t = GetNumThreadsNext(ti, numThreadsSpecified);
          UInt64 speed = 0;
          RINOK(CrcBench(EXTERNAL_CODECS_LOC_VARS complexInCommands,
              t, bufSize, speed,
              complexity,
              1, // benchWeight,
              (pow == kNumHashDictBits) ? checkSum : NULL, method, NULL, NULL, false, 0));
          PrintNumber(f, (speed >> 20), kFieldSize_CrcSpeed);
          speedTotals.Values[ti] += speed;
        }
        f.NewLine();
        numSteps++;
      }
    }
    if (numSteps != 0)
    {
      f.NewLine();
      f.Print("Avg:");
      for (unsigned ti = 0; ti < numThreadsTests; ti++)
      {
        PrintNumber(f, ((speedTotals.Values[ti] / numSteps) >> 20), kFieldSize_CrcSpeed);
      }
      f.NewLine();
    }
    return S_OK;
  }

  bool use2Columns = false;

  bool totalBenchMode = (method.MethodName.IsEqualTo_Ascii_NoCase("*"));
  bool onlyHashBench = false;
  if (method.MethodName.IsEqualTo_Ascii_NoCase("hash"))
  {
    onlyHashBench = true;
    totalBenchMode = true;
  }

  // ---------- Threads loop ----------
  for (unsigned threadsPassIndex = 0; threadsPassIndex < 3; threadsPassIndex++)
  {

  UInt32 numThreads = numThreadsSpecified;
    
  if (!multiThreadTests)
  {
    if (threadsPassIndex != 0)
      break;
  }
  else
  {
    numThreads = 1;
    if (threadsPassIndex != 0)
    {
      if (numCPUs < 2)
        break;
      numThreads = numCPUs;
      if (threadsPassIndex == 1)
      {
        if (numCPUs >= 4)
          numThreads = numCPUs / 2;
      }
      else if (numCPUs < 4)
        break;
    }
  }
 
  CBenchCallbackToPrint callback;
  callback.Init();
  callback._file = printCallback;
  
  IBenchPrintCallback &f = *printCallback;

  if (threadsPassIndex > 0)
  {
    f.NewLine();
    f.NewLine();
  }

  if (!dictIsDefined)
  {
    const unsigned dicSizeLog_Main = (totalBenchMode ? 24 : 25);
    unsigned dicSizeLog = dicSizeLog_Main;
    
    #ifdef UNDER_CE
    dicSizeLog = (UInt64)1 << 20;
    #endif

    if (ramSize_Defined)
    for (; dicSizeLog > kBenchMinDicLogSize; dicSizeLog--)
      if (GetBenchMemoryUsage(numThreads, ((UInt32)1 << dicSizeLog), totalBenchMode) + (8 << 20) <= ramSize)
        break;

    dict = (UInt32)1 << dicSizeLog;

    if (totalBenchMode && dicSizeLog != dicSizeLog_Main)
    {
      f.Print("Dictionary reduced to: ");
      PrintNumber(f, dicSizeLog, 1);
      f.NewLine();
    }
  }

  PrintRequirements(f, "usage:", true, GetBenchMemoryUsage(numThreads, dict, totalBenchMode), "Benchmark threads:   ", numThreads);
  f.NewLine();

  f.NewLine();

  if (totalBenchMode)
  {
    callback.NameFieldSize = kFieldSize_Name;
    use2Columns = false;
  }
  else
  {
    callback.NameFieldSize = kFieldSize_SmallName;
    use2Columns = true;
  }
  callback.Use2Columns = use2Columns;

  bool showFreq = false;
  UInt64 cpuFreq = 0;

  if (totalBenchMode)
  {
    showFreq = true;
  }

  unsigned fileldSize = kFieldSize_TotalSize;
  if (showFreq)
    fileldSize += kFieldSize_EUAndEffec;

  if (use2Columns)
  {
    PrintSpaces(f, callback.NameFieldSize);
    PrintRight(f, "Compressing", fileldSize);
    f.Print(kSep);
    PrintRight(f, "Decompressing", fileldSize);
  }
  f.NewLine();
  PrintLeft(f, totalBenchMode ? "Method" : "Dict", callback.NameFieldSize);

  int j;

  for (j = 0; j < 2; j++)
  {
    PrintRight(f, "Speed", kFieldSize_Speed + 1);
    PrintRight(f, "Usage", kFieldSize_Usage + 1);
    PrintRight(f, "R/U", kFieldSize_RU + 1);
    PrintRight(f, "Rating", kFieldSize_Rating + 1);
    if (showFreq)
    {
      PrintRight(f, "E/U", kFieldSize_EU + 1);
      PrintRight(f, "Effec", kFieldSize_Effec + 1);
    }
    if (!use2Columns)
      break;
    if (j == 0)
      f.Print(kSep);
  }
  
  f.NewLine();
  PrintSpaces(f, callback.NameFieldSize);
  
  for (j = 0; j < 2; j++)
  {
    PrintRight(f, "KiB/s", kFieldSize_Speed + 1);
    PrintRight(f, "%", kFieldSize_Usage + 1);
    PrintRight(f, "MIPS", kFieldSize_RU + 1);
    PrintRight(f, "MIPS", kFieldSize_Rating + 1);
    if (showFreq)
    {
      PrintRight(f, "%", kFieldSize_EU + 1);
      PrintRight(f, "%", kFieldSize_Effec + 1);
    }
    if (!use2Columns)
      break;
    if (j == 0)
      f.Print(kSep);
  }
  
  f.NewLine();
  f.NewLine();

  if (specifiedFreq != 0)
    cpuFreq = specifiedFreq;


  if (totalBenchMode)
  {
    for (UInt32 i = 0; i < numIterations; i++)
    {
      if (i != 0)
        printCallback->NewLine();
      HRESULT res;

      const unsigned kNumCpuTests = 3;
      for (unsigned freqTest = 0; freqTest < kNumCpuTests; freqTest++)
      {
        PrintLeft(f, "CPU", kFieldSize_Name);
        UInt32 resVal;
        RINOK(FreqBench(complexInCommands, numThreads, printCallback,
            (freqTest == kNumCpuTests - 1 || specifiedFreq != 0), // showFreq
            specifiedFreq,
            cpuFreq, resVal));
        callback.NewLine();

        if (specifiedFreq != 0)
          cpuFreq = specifiedFreq;

        if (freqTest == kNumCpuTests - 1)
          SetComplexCommands(testTime, specifiedFreq != 0, cpuFreq, complexInCommands);
      }
      callback.NewLine();

      callback.SetFreq(true, cpuFreq);

      if (!onlyHashBench)
      {
        res = TotalBench(EXTERNAL_CODECS_LOC_VARS
            complexInCommands, numThreads,
            dictIsDefined || fileDataBuffer.Buffer, // forceUnpackSize
            fileDataBuffer.Buffer ? fileDataBuffer.BufferSize : dict,
            fileDataBuffer.Buffer,
            printCallback, &callback);
        RINOK(res);
      }

      res = TotalBench_Hash(EXTERNAL_CODECS_LOC_VARS complexInCommands, numThreads,
          1 << kNumHashDictBits, printCallback, &callback, &callback.EncodeRes, true, cpuFreq);
      RINOK(res);

      callback.NewLine();
      {
        PrintLeft(f, "CPU", kFieldSize_Name);
        UInt32 resVal;
        UInt64 cpuFreqLastTemp = cpuFreq;
        RINOK(FreqBench(complexInCommands, numThreads, printCallback,
            specifiedFreq != 0, // showFreq
            specifiedFreq,
            cpuFreqLastTemp, resVal));
        callback.NewLine();
      }
    }
  }
  else
  {
    bool needSetComplexity = true;
    if (!methodName.IsEqualTo_Ascii_NoCase("LZMA"))
    {
      unsigned i;
      for (i = 0; i < ARRAY_SIZE(g_Bench); i++)
      {
        const CBenchMethod &h = g_Bench[i];
        AString benchMethod (h.Name);
        AString benchProps;
        int propPos = benchMethod.Find(':');
        if (propPos >= 0)
        {
          benchProps = benchMethod.Ptr(propPos + 1);
          benchMethod.DeleteFrom(propPos);
        }

        if (AreSameMethodNames(benchMethod, methodName))
        {
          if (benchProps.IsEmpty()
              || benchProps == "x5" && method.PropsString.IsEmpty()
              || method.PropsString.IsPrefixedBy_Ascii_NoCase(benchProps))
          {
            callback.BenchProps.EncComplex = h.EncComplex;
            callback.BenchProps.DecComplexCompr = h.DecComplexCompr;
            callback.BenchProps.DecComplexUnc = h.DecComplexUnc;;
            needSetComplexity = false;
            break;
          }
        }
      }
      if (i == ARRAY_SIZE(g_Bench))
        return E_NOTIMPL;
    }
    if (needSetComplexity)
      callback.BenchProps.SetLzmaCompexity();

  for (unsigned i = 0; i < numIterations; i++)
  {
    const unsigned kStartDicLog = 22;
    unsigned pow = (dict < ((UInt32)1 << kStartDicLog)) ? kBenchMinDicLogSize : kStartDicLog;
    if (!multiDict)
      pow = 31;
    while (((UInt32)1 << pow) > dict && pow > 0)
      pow--;
    for (; ((UInt32)1 << pow) <= dict; pow++)
    {
      char s[16];
      ConvertUInt32ToString(pow, s);
      unsigned pos = MyStringLen(s);
      s[pos++] = ':';
      s[pos] = 0;
      PrintLeft(f, s, kFieldSize_SmallName);
      callback.DictSize = (UInt32)1 << pow;

      COneMethodInfo method2 = method;

      if (StringsAreEqualNoCase_Ascii(method2.MethodName, "LZMA"))
      {
        // We add dictionary size property.
        // method2 can have two different dictionary size properties.
        // And last property is main.
        NCOM::CPropVariant propVariant = (UInt32)pow;
        RINOK(method2.ParseMethodFromPROPVARIANT((UString)"d", propVariant));
      }

      size_t uncompressedDataSize;
      if (fileDataBuffer.Buffer)
      {
        uncompressedDataSize = fileDataBuffer.BufferSize;
      }
      else
      {
        uncompressedDataSize = callback.DictSize;
        if (uncompressedDataSize >= (1 << 18))
          uncompressedDataSize += kAdditionalSize;
      }

      HRESULT res = MethodBench(
          EXTERNAL_CODECS_LOC_VARS
          complexInCommands,
          true, numThreads,
          method2,
          uncompressedDataSize, fileDataBuffer.Buffer,
          kOldLzmaDictBits, printCallback, &callback, &callback.BenchProps);
      f.NewLine();
      RINOK(res);
      if (!multiDict)
        break;
    }
  }
  }

  PrintChars(f, '-', callback.NameFieldSize + fileldSize);
  
  if (use2Columns)
  {
    f.Print(kSep);
    PrintChars(f, '-', fileldSize);
  }

  f.NewLine();
  
  if (use2Columns)
  {
    PrintLeft(f, "Avr:", callback.NameFieldSize);
    PrintTotals(f, showFreq, cpuFreq, callback.EncodeRes);
    f.Print(kSep);
    PrintTotals(f, showFreq, cpuFreq, callback.DecodeRes);
    f.NewLine();
  }
  
  PrintLeft(f, "Tot:", callback.NameFieldSize);
  CTotalBenchRes midRes;
  midRes.SetSum(callback.EncodeRes, callback.DecodeRes);
  PrintTotals(f, showFreq, cpuFreq, midRes);
  f.NewLine();

  }
  return S_OK;
}
