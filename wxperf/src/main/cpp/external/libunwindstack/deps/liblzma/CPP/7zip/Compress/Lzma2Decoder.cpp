// Lzma2Decoder.cpp

#include "StdAfx.h"

#include "../../../C/Alloc.h"

#include "../Common/StreamUtils.h"

#include "Lzma2Decoder.h"

static HRESULT SResToHRESULT(SRes res)
{
  switch (res)
  {
    case SZ_OK: return S_OK;
    case SZ_ERROR_MEM: return E_OUTOFMEMORY;
    case SZ_ERROR_PARAM: return E_INVALIDARG;
    // case SZ_ERROR_PROGRESS: return E_ABORT;
    case SZ_ERROR_DATA: return S_FALSE;
  }
  return E_FAIL;
}

namespace NCompress {
namespace NLzma2 {

CDecoder::CDecoder():
    _inBuf(NULL),
    _inBufSize(0),
    _inBufSizeNew(1 << 20),
    _outStepSize(1 << 22),
    _outSizeDefined(false),
    _finishMode(false)
{
  Lzma2Dec_Construct(&_state);
}

STDMETHODIMP CDecoder::SetInBufSize(UInt32 , UInt32 size) { _inBufSizeNew = size; return S_OK; }
STDMETHODIMP CDecoder::SetOutBufSize(UInt32 , UInt32 size) { _outStepSize = size; return S_OK; }

CDecoder::~CDecoder()
{
  Lzma2Dec_Free(&_state, &g_Alloc);
  MidFree(_inBuf);
}

STDMETHODIMP CDecoder::SetDecoderProperties2(const Byte *prop, UInt32 size)
{
  if (size != 1)
    return E_NOTIMPL;
  
  RINOK(SResToHRESULT(Lzma2Dec_Allocate(&_state, prop[0], &g_Alloc)));
  if (!_inBuf || _inBufSize != _inBufSizeNew)
  {
    MidFree(_inBuf);
    _inBufSize = 0;
    _inBuf = (Byte *)MidAlloc(_inBufSizeNew);
    if (!_inBuf)
      return E_OUTOFMEMORY;
    _inBufSize = _inBufSizeNew;
  }

  return S_OK;
}

STDMETHODIMP CDecoder::GetInStreamProcessedSize(UInt64 *value) { *value = _inSizeProcessed; return S_OK; }
STDMETHODIMP CDecoder::SetInStream(ISequentialInStream *inStream) { _inStream = inStream; return S_OK; }
STDMETHODIMP CDecoder::ReleaseInStream() { _inStream.Release(); return S_OK; }

STDMETHODIMP CDecoder::SetOutStreamSize(const UInt64 *outSize)
{
  _outSizeDefined = (outSize != NULL);
  _outSize = 0;
  if (_outSizeDefined)
    _outSize = *outSize;

  Lzma2Dec_Init(&_state);
  
  _inPos = _inSize = 0;
  _inSizeProcessed = _outSizeProcessed = 0;
  return S_OK;
}

STDMETHODIMP CDecoder::SetFinishMode(UInt32 finishMode)
{
  _finishMode = (finishMode != 0);
  return S_OK;
}

STDMETHODIMP CDecoder::Code(ISequentialInStream *inStream,
    ISequentialOutStream *outStream, const UInt64 *inSize,
    const UInt64 *outSize, ICompressProgressInfo *progress)
{
  if (!_inBuf)
    return S_FALSE;
  SetOutStreamSize(outSize);

  UInt32 step = _outStepSize;
  const UInt32 kOutStepSize_Min = 1 << 12;
  if (step < kOutStepSize_Min)
    step = kOutStepSize_Min;
  
  SizeT wrPos = _state.decoder.dicPos;

  SizeT next = (_state.decoder.dicBufSize - _state.decoder.dicPos < step) ?
      _state.decoder.dicBufSize :
      _state.decoder.dicPos + step;

  HRESULT hres = S_OK;

  for (;;)
  {
    if (_inPos == _inSize)
    {
      _inPos = _inSize = 0;
      hres = inStream->Read(_inBuf, _inBufSize, &_inSize);
      if (hres != S_OK)
        break;
    }

    SizeT dicPos = _state.decoder.dicPos;
    SizeT curSize = next - dicPos;
   
    ELzmaFinishMode finishMode = LZMA_FINISH_ANY;
    if (_outSizeDefined)
    {
      const UInt64 rem = _outSize - _outSizeProcessed;
      if (curSize >= rem)
      {
        curSize = (SizeT)rem;
        if (_finishMode)
          finishMode = LZMA_FINISH_END;
      }
    }

    SizeT inSizeProcessed = _inSize - _inPos;
    ELzmaStatus status;
    SRes res = Lzma2Dec_DecodeToDic(&_state, dicPos + curSize, _inBuf + _inPos, &inSizeProcessed, finishMode, &status);

    _inPos += (UInt32)inSizeProcessed;
    _inSizeProcessed += inSizeProcessed;
    SizeT outSizeProcessed = _state.decoder.dicPos - dicPos;
    _outSizeProcessed += outSizeProcessed;

    bool finished = (inSizeProcessed == 0 && outSizeProcessed == 0
        || status == LZMA_STATUS_FINISHED_WITH_MARK);
    bool outFinished = (_outSizeDefined && _outSizeProcessed >= _outSize);

    if (res != 0
        || _state.decoder.dicPos >= next
        || finished
        || outFinished)
    {
      HRESULT res2 = WriteStream(outStream, _state.decoder.dic + wrPos, _state.decoder.dicPos - wrPos);

      if (_state.decoder.dicPos == _state.decoder.dicBufSize)
        _state.decoder.dicPos = 0;
      
      wrPos = _state.decoder.dicPos;

      next = (_state.decoder.dicBufSize - _state.decoder.dicPos < step) ?
          _state.decoder.dicBufSize :
          _state.decoder.dicPos + step;

      if (res != 0)
        return S_FALSE;
      RINOK(res2);

      if (finished)
      {
        if (status == LZMA_STATUS_FINISHED_WITH_MARK)
        {
          if (_finishMode && inSize && *inSize != _inSizeProcessed)
            return S_FALSE;
          if (finishMode == LZMA_FINISH_END && !outFinished)
            return S_FALSE;
          return S_OK;
        }
        return (finishMode == LZMA_FINISH_END) ? S_FALSE : S_OK;
      }

      if (outFinished && finishMode == LZMA_FINISH_ANY)
        return S_OK;
    }
    
    if (progress)
    {
      RINOK(progress->SetRatioInfo(&_inSizeProcessed, &_outSizeProcessed));
    }
  }

  HRESULT res2 = WriteStream(outStream, _state.decoder.dic + wrPos, _state.decoder.dicPos - wrPos);
  if (hres != S_OK)
    return hres;
  return res2;
}

#ifndef NO_READ_FROM_CODER

STDMETHODIMP CDecoder::Read(void *data, UInt32 size, UInt32 *processedSize)
{
  UInt32 totalProcessed = 0;

  if (processedSize)
    *processedSize = 0;

  for (;;)
  {
    if (_inPos == _inSize)
    {
      _inPos = _inSize = 0;
      RINOK(_inStream->Read(_inBuf, _inBufSize, &_inSize));
    }
    {
      ELzmaFinishMode finishMode = LZMA_FINISH_ANY;
      if (_outSizeDefined)
      {
        const UInt64 rem = _outSize - _outSizeProcessed;
        if (rem <= size)
        {
          size = (UInt32)rem;
          if (_finishMode)
            finishMode = LZMA_FINISH_END;
        }
      }

      SizeT outProcessed = size;
      SizeT inProcessed = _inSize - _inPos;
      
      ELzmaStatus status;
      SRes res = Lzma2Dec_DecodeToBuf(&_state, (Byte *)data, &outProcessed,
          _inBuf + _inPos, &inProcessed, finishMode, &status);

      _inPos += (UInt32)inProcessed;
      _inSizeProcessed += inProcessed;
      _outSizeProcessed += outProcessed;
      size -= (UInt32)outProcessed;
      data = (Byte *)data + outProcessed;
      
      totalProcessed += (UInt32)outProcessed;
      if (processedSize)
        *processedSize = totalProcessed;

      if (res != SZ_OK)
      {
        if (totalProcessed != 0)
          return S_OK;
        return SResToHRESULT(res);
      }
      
      if (inProcessed == 0 && outProcessed == 0)
        return S_OK;
      if (status == LZMA_STATUS_FINISHED_WITH_MARK)
        return S_OK;
      if (outProcessed != 0)
      {
        if (finishMode != LZMA_FINISH_END || _outSize != _outSizeProcessed)
          return S_OK;
      }
    }
  }
}

#endif

}}
