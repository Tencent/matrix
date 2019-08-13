// CWrappers.h

#include "StdAfx.h"

#include "../../../C/Alloc.h"

#include "CWrappers.h"

#include "StreamUtils.h"

#define PROGRESS_UNKNOWN_VALUE ((UInt64)(Int64)-1)

#define CONVERT_PR_VAL(x) (x == PROGRESS_UNKNOWN_VALUE ? NULL : &x)

static SRes CompressProgress(void *pp, UInt64 inSize, UInt64 outSize) throw()
{
  CCompressProgressWrap *p = (CCompressProgressWrap *)pp;
  p->Res = p->Progress->SetRatioInfo(CONVERT_PR_VAL(inSize), CONVERT_PR_VAL(outSize));
  return (SRes)p->Res;
}

CCompressProgressWrap::CCompressProgressWrap(ICompressProgressInfo *progress) throw()
{
  p.Progress = CompressProgress;
  Progress = progress;
  Res = SZ_OK;
}

static const UInt32 kStreamStepSize = (UInt32)1 << 31;

SRes HRESULT_To_SRes(HRESULT res, SRes defaultRes)
{
  switch (res)
  {
    case S_OK: return SZ_OK;
    case E_OUTOFMEMORY: return SZ_ERROR_MEM;
    case E_INVALIDARG: return SZ_ERROR_PARAM;
    case E_ABORT: return SZ_ERROR_PROGRESS;
    case S_FALSE: return SZ_ERROR_DATA;
    case E_NOTIMPL: return SZ_ERROR_UNSUPPORTED;
  }
  return defaultRes;
}

static SRes MyRead(void *object, void *data, size_t *size) throw()
{
  CSeqInStreamWrap *p = (CSeqInStreamWrap *)object;
  UInt32 curSize = ((*size < kStreamStepSize) ? (UInt32)*size : kStreamStepSize);
  p->Res = (p->Stream->Read(data, curSize, &curSize));
  *size = curSize;
  p->Processed += curSize;
  if (p->Res == S_OK)
    return SZ_OK;
  return HRESULT_To_SRes(p->Res, SZ_ERROR_READ);
}

static size_t MyWrite(void *object, const void *data, size_t size) throw()
{
  CSeqOutStreamWrap *p = (CSeqOutStreamWrap *)object;
  if (p->Stream)
  {
    p->Res = WriteStream(p->Stream, data, size);
    if (p->Res != 0)
      return 0;
  }
  else
    p->Res = S_OK;
  p->Processed += size;
  return size;
}

CSeqInStreamWrap::CSeqInStreamWrap(ISequentialInStream *stream) throw()
{
  p.Read = MyRead;
  Stream = stream;
  Processed = 0;
}

CSeqOutStreamWrap::CSeqOutStreamWrap(ISequentialOutStream *stream) throw()
{
  p.Write = MyWrite;
  Stream = stream;
  Res = SZ_OK;
  Processed = 0;
}

HRESULT SResToHRESULT(SRes res) throw()
{
  switch (res)
  {
    case SZ_OK: return S_OK;
    case SZ_ERROR_MEM: return E_OUTOFMEMORY;
    case SZ_ERROR_PARAM: return E_INVALIDARG;
    case SZ_ERROR_PROGRESS: return E_ABORT;
    case SZ_ERROR_DATA: return S_FALSE;
    case SZ_ERROR_UNSUPPORTED: return E_NOTIMPL;
  }
  return E_FAIL;
}

static SRes InStreamWrap_Read(void *pp, void *data, size_t *size) throw()
{
  CSeekInStreamWrap *p = (CSeekInStreamWrap *)pp;
  UInt32 curSize = ((*size < kStreamStepSize) ? (UInt32)*size : kStreamStepSize);
  p->Res = p->Stream->Read(data, curSize, &curSize);
  *size = curSize;
  return (p->Res == S_OK) ? SZ_OK : SZ_ERROR_READ;
}

static SRes InStreamWrap_Seek(void *pp, Int64 *offset, ESzSeek origin) throw()
{
  CSeekInStreamWrap *p = (CSeekInStreamWrap *)pp;
  UInt32 moveMethod;
  switch (origin)
  {
    case SZ_SEEK_SET: moveMethod = STREAM_SEEK_SET; break;
    case SZ_SEEK_CUR: moveMethod = STREAM_SEEK_CUR; break;
    case SZ_SEEK_END: moveMethod = STREAM_SEEK_END; break;
    default: return SZ_ERROR_PARAM;
  }
  UInt64 newPosition;
  p->Res = p->Stream->Seek(*offset, moveMethod, &newPosition);
  *offset = (Int64)newPosition;
  return (p->Res == S_OK) ? SZ_OK : SZ_ERROR_READ;
}

CSeekInStreamWrap::CSeekInStreamWrap(IInStream *stream) throw()
{
  Stream = stream;
  p.Read = InStreamWrap_Read;
  p.Seek = InStreamWrap_Seek;
  Res = S_OK;
}


/* ---------- CByteInBufWrap ---------- */

void CByteInBufWrap::Free() throw()
{
  ::MidFree(Buf);
  Buf = 0;
}

bool CByteInBufWrap::Alloc(UInt32 size) throw()
{
  if (Buf == 0 || size != Size)
  {
    Free();
    Lim = Cur = Buf = (Byte *)::MidAlloc((size_t)size);
    Size = size;
  }
  return (Buf != 0);
}

Byte CByteInBufWrap::ReadByteFromNewBlock() throw()
{
  if (Res == S_OK)
  {
    UInt32 avail;
    Processed += (Cur - Buf);
    Res = Stream->Read(Buf, Size, &avail);
    Cur = Buf;
    Lim = Buf + avail;
    if (avail != 0)
      return *Cur++;
  }
  Extra = true;
  return 0;
}

static Byte Wrap_ReadByte(void *pp) throw()
{
  CByteInBufWrap *p = (CByteInBufWrap *)pp;
  if (p->Cur != p->Lim)
    return *p->Cur++;
  return p->ReadByteFromNewBlock();
}

CByteInBufWrap::CByteInBufWrap(): Buf(0)
{
  p.Read = Wrap_ReadByte;
}


/* ---------- CByteOutBufWrap ---------- */

void CByteOutBufWrap::Free() throw()
{
  ::MidFree(Buf);
  Buf = 0;
}

bool CByteOutBufWrap::Alloc(size_t size) throw()
{
  if (Buf == 0 || size != Size)
  {
    Free();
    Buf = (Byte *)::MidAlloc(size);
    Size = size;
  }
  return (Buf != 0);
}

HRESULT CByteOutBufWrap::Flush() throw()
{
  if (Res == S_OK)
  {
    size_t size = (Cur - Buf);
    Res = WriteStream(Stream, Buf, size);
    if (Res == S_OK)
      Processed += size;
    Cur = Buf;
  }
  return Res;
}

static void Wrap_WriteByte(void *pp, Byte b) throw()
{
  CByteOutBufWrap *p = (CByteOutBufWrap *)pp;
  Byte *dest = p->Cur;
  *dest = b;
  p->Cur = ++dest;
  if (dest == p->Lim)
    p->Flush();
}

CByteOutBufWrap::CByteOutBufWrap() throw(): Buf(0)
{
  p.Write = Wrap_WriteByte;
}
