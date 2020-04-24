// PpmdDecoder.cpp
// 2009-03-11 : Igor Pavlov : Public domain

#include "StdAfx.h"

#include "../../../C/Alloc.h"
#include "../../../C/CpuArch.h"

#include "../Common/StreamUtils.h"

#include "PpmdDecoder.h"

namespace NCompress {
namespace NPpmd {

static const UInt32 kBufSize = (1 << 20);

enum
{
  kStatus_NeedInit,
  kStatus_Normal,
  kStatus_Finished,
  kStatus_Error
};

CDecoder::~CDecoder()
{
  ::MidFree(_outBuf);
  Ppmd7_Free(&_ppmd, &g_BigAlloc);
}

STDMETHODIMP CDecoder::SetDecoderProperties2(const Byte *props, UInt32 size)
{
  if (size < 5)
    return E_INVALIDARG;
  _order = props[0];
  UInt32 memSize = GetUi32(props + 1);
  if (_order < PPMD7_MIN_ORDER ||
      _order > PPMD7_MAX_ORDER ||
      memSize < PPMD7_MIN_MEM_SIZE ||
      memSize > PPMD7_MAX_MEM_SIZE)
    return E_NOTIMPL;
  if (!_inStream.Alloc(1 << 20))
    return E_OUTOFMEMORY;
  if (!Ppmd7_Alloc(&_ppmd, memSize, &g_BigAlloc))
    return E_OUTOFMEMORY;
  return S_OK;
}

HRESULT CDecoder::CodeSpec(Byte *memStream, UInt32 size)
{
  switch (_status)
  {
    case kStatus_Finished: return S_OK;
    case kStatus_Error: return S_FALSE;
    case kStatus_NeedInit:
      _inStream.Init();
      if (!Ppmd7z_RangeDec_Init(&_rangeDec))
      {
        _status = kStatus_Error;
        return S_FALSE;
      }
      _status = kStatus_Normal;
      Ppmd7_Init(&_ppmd, _order);
      break;
  }
  if (_outSizeDefined)
  {
    const UInt64 rem = _outSize - _processedSize;
    if (size > rem)
      size = (UInt32)rem;
  }

  UInt32 i;
  int sym = 0;
  for (i = 0; i != size; i++)
  {
    sym = Ppmd7_DecodeSymbol(&_ppmd, &_rangeDec.vt);
    if (_inStream.Extra || sym < 0)
      break;
    memStream[i] = (Byte)sym;
  }

  _processedSize += i;
  if (_inStream.Extra)
  {
    _status = kStatus_Error;
    return _inStream.Res;
  }
  if (sym < 0)
    _status = (sym < -1) ? kStatus_Error : kStatus_Finished;
  return S_OK;
}

STDMETHODIMP CDecoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 * /* inSize */, const UInt64 *outSize, ICompressProgressInfo *progress)
{
  if (!_outBuf)
  {
    _outBuf = (Byte *)::MidAlloc(kBufSize);
    if (!_outBuf)
      return E_OUTOFMEMORY;
  }
  
  _inStream.Stream = inStream;
  SetOutStreamSize(outSize);

  do
  {
    const UInt64 startPos = _processedSize;
    HRESULT res = CodeSpec(_outBuf, kBufSize);
    size_t processed = (size_t)(_processedSize - startPos);
    RINOK(WriteStream(outStream, _outBuf, processed));
    RINOK(res);
    if (_status == kStatus_Finished)
      break;
    if (progress)
    {
      UInt64 inSize = _inStream.GetProcessed();
      RINOK(progress->SetRatioInfo(&inSize, &_processedSize));
    }
  }
  while (!_outSizeDefined || _processedSize < _outSize);
  return S_OK;
}

STDMETHODIMP CDecoder::SetOutStreamSize(const UInt64 *outSize)
{
  _outSizeDefined = (outSize != NULL);
  if (_outSizeDefined)
    _outSize = *outSize;
  _processedSize = 0;
  _status = kStatus_NeedInit;
  return S_OK;
}


STDMETHODIMP CDecoder::GetInStreamProcessedSize(UInt64 *value)
{
  *value = _inStream.GetProcessed();
  return S_OK;
}

#ifndef NO_READ_FROM_CODER

STDMETHODIMP CDecoder::SetInStream(ISequentialInStream *inStream)
{
  InSeqStream = inStream;
  _inStream.Stream = inStream;
  return S_OK;
}

STDMETHODIMP CDecoder::ReleaseInStream()
{
  InSeqStream.Release();
  return S_OK;
}

STDMETHODIMP CDecoder::Read(void *data, UInt32 size, UInt32 *processedSize)
{
  const UInt64 startPos = _processedSize;
  HRESULT res = CodeSpec((Byte *)data, size);
  if (processedSize)
    *processedSize = (UInt32)(_processedSize - startPos);
  return res;
}

#endif

}}
