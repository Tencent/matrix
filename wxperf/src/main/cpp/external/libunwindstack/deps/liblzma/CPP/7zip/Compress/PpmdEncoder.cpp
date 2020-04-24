// PpmdEncoder.cpp

#include "StdAfx.h"

#include "../../../C/Alloc.h"
#include "../../../C/CpuArch.h"

#include "../Common/StreamUtils.h"

#include "PpmdEncoder.h"

namespace NCompress {
namespace NPpmd {

static const UInt32 kBufSize = (1 << 20);

static const Byte kOrders[10] = { 3, 4, 4, 5, 5, 6, 8, 16, 24, 32 };

void CEncProps::Normalize(int level)
{
  if (level < 0) level = 5;
  if (level > 9) level = 9;
  if (MemSize == (UInt32)(Int32)-1)
    MemSize = level >= 9 ? ((UInt32)192 << 20) : ((UInt32)1 << (level + 19));
  const unsigned kMult = 16;
  if (MemSize / kMult > ReduceSize)
  {
    for (unsigned i = 16; i <= 31; i++)
    {
      UInt32 m = (UInt32)1 << i;
      if (ReduceSize <= m / kMult)
      {
        if (MemSize > m)
          MemSize = m;
        break;
      }
    }
  }
  if (Order == -1) Order = kOrders[(unsigned)level];
}

CEncoder::CEncoder():
  _inBuf(NULL)
{
  _props.Normalize(-1);
  _rangeEnc.Stream = &_outStream.vt;
  Ppmd7_Construct(&_ppmd);
}

CEncoder::~CEncoder()
{
  ::MidFree(_inBuf);
  Ppmd7_Free(&_ppmd, &g_BigAlloc);
}

STDMETHODIMP CEncoder::SetCoderProperties(const PROPID *propIDs, const PROPVARIANT *coderProps, UInt32 numProps)
{
  int level = -1;
  CEncProps props;
  for (UInt32 i = 0; i < numProps; i++)
  {
    const PROPVARIANT &prop = coderProps[i];
    PROPID propID = propIDs[i];
    if (propID > NCoderPropID::kReduceSize)
      continue;
    if (propID == NCoderPropID::kReduceSize)
    {
      if (prop.vt == VT_UI8 && prop.uhVal.QuadPart < (UInt32)(Int32)-1)
        props.ReduceSize = (UInt32)prop.uhVal.QuadPart;
      continue;
    }
    if (prop.vt != VT_UI4)
      return E_INVALIDARG;
    UInt32 v = (UInt32)prop.ulVal;
    switch (propID)
    {
      case NCoderPropID::kUsedMemorySize:
        if (v < (1 << 16) || v > PPMD7_MAX_MEM_SIZE || (v & 3) != 0)
          return E_INVALIDARG;
        props.MemSize = v;
        break;
      case NCoderPropID::kOrder:
        if (v < 2 || v > 32)
          return E_INVALIDARG;
        props.Order = (Byte)v;
        break;
      case NCoderPropID::kNumThreads: break;
      case NCoderPropID::kLevel: level = (int)v; break;
      default: return E_INVALIDARG;
    }
  }
  props.Normalize(level);
  _props = props;
  return S_OK;
}

STDMETHODIMP CEncoder::WriteCoderProperties(ISequentialOutStream *outStream)
{
  const UInt32 kPropSize = 5;
  Byte props[kPropSize];
  props[0] = (Byte)_props.Order;
  SetUi32(props + 1, _props.MemSize);
  return WriteStream(outStream, props, kPropSize);
}

HRESULT CEncoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 * /* inSize */, const UInt64 * /* outSize */, ICompressProgressInfo *progress)
{
  if (!_inBuf)
  {
    _inBuf = (Byte *)::MidAlloc(kBufSize);
    if (!_inBuf)
      return E_OUTOFMEMORY;
  }
  if (!_outStream.Alloc(1 << 20))
    return E_OUTOFMEMORY;
  if (!Ppmd7_Alloc(&_ppmd, _props.MemSize, &g_BigAlloc))
    return E_OUTOFMEMORY;

  _outStream.Stream = outStream;
  _outStream.Init();

  Ppmd7z_RangeEnc_Init(&_rangeEnc);
  Ppmd7_Init(&_ppmd, _props.Order);

  UInt64 processed = 0;
  for (;;)
  {
    UInt32 size;
    RINOK(inStream->Read(_inBuf, kBufSize, &size));
    if (size == 0)
    {
      // We don't write EndMark in PPMD-7z.
      // Ppmd7_EncodeSymbol(&_ppmd, &_rangeEnc, -1);
      Ppmd7z_RangeEnc_FlushData(&_rangeEnc);
      return _outStream.Flush();
    }
    for (UInt32 i = 0; i < size; i++)
    {
      Ppmd7_EncodeSymbol(&_ppmd, &_rangeEnc, _inBuf[i]);
      RINOK(_outStream.Res);
    }
    processed += size;
    if (progress)
    {
      UInt64 outSize = _outStream.GetProcessed();
      RINOK(progress->SetRatioInfo(&processed, &outSize));
    }
  }
}

}}
