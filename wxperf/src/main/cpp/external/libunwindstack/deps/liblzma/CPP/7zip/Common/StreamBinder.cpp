// StreamBinder.cpp

#include "StdAfx.h"

#include "../../Common/MyCom.h"

#include "StreamBinder.h"

class CBinderInStream:
  public ISequentialInStream,
  public CMyUnknownImp
{
  CStreamBinder *_binder;
public:
  MY_UNKNOWN_IMP1(ISequentialInStream)
  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);
  ~CBinderInStream() { _binder->CloseRead(); }
  CBinderInStream(CStreamBinder *binder): _binder(binder) {}
};

STDMETHODIMP CBinderInStream::Read(void *data, UInt32 size, UInt32 *processedSize)
  { return _binder->Read(data, size, processedSize); }

class CBinderOutStream:
  public ISequentialOutStream,
  public CMyUnknownImp
{
  CStreamBinder *_binder;
public:
  MY_UNKNOWN_IMP1(ISequentialOutStream)
  STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize);
  ~CBinderOutStream() { _binder->CloseWrite(); }
  CBinderOutStream(CStreamBinder *binder): _binder(binder) {}
};

STDMETHODIMP CBinderOutStream::Write(const void *data, UInt32 size, UInt32 *processedSize)
  { return _binder->Write(data, size, processedSize); }



WRes CStreamBinder::CreateEvents()
{
  RINOK(_canWrite_Event.Create());
  RINOK(_canRead_Event.Create());
  return _readingWasClosed_Event.Create();
}

void CStreamBinder::ReInit()
{
  _canWrite_Event.Reset();
  _canRead_Event.Reset();
  _readingWasClosed_Event.Reset();

  // _readingWasClosed = false;
  _readingWasClosed2 = false;

  _waitWrite = true;
  _bufSize = 0;
  _buf = NULL;
  ProcessedSize = 0;
  // WritingWasCut = false;
}


void CStreamBinder::CreateStreams(ISequentialInStream **inStream, ISequentialOutStream **outStream)
{
  // _readingWasClosed = false;
  _readingWasClosed2 = false;

  _waitWrite = true;
  _bufSize = 0;
  _buf = NULL;
  ProcessedSize = 0;
  // WritingWasCut = false;

  CBinderInStream *inStreamSpec = new CBinderInStream(this);
  CMyComPtr<ISequentialInStream> inStreamLoc(inStreamSpec);
  *inStream = inStreamLoc.Detach();

  CBinderOutStream *outStreamSpec = new CBinderOutStream(this);
  CMyComPtr<ISequentialOutStream> outStreamLoc(outStreamSpec);
  *outStream = outStreamLoc.Detach();
}

// (_canRead_Event && _bufSize == 0) means that stream is finished.

HRESULT CStreamBinder::Read(void *data, UInt32 size, UInt32 *processedSize)
{
  if (processedSize)
    *processedSize = 0;
  if (size != 0)
  {
    if (_waitWrite)
    {
      RINOK(_canRead_Event.Lock());
      _waitWrite = false;
    }
    if (size > _bufSize)
      size = _bufSize;
    if (size != 0)
    {
      memcpy(data, _buf, size);
      _buf = ((const Byte *)_buf) + size;
      ProcessedSize += size;
      if (processedSize)
        *processedSize = size;
      _bufSize -= size;
      if (_bufSize == 0)
      {
        _waitWrite = true;
        _canRead_Event.Reset();
        _canWrite_Event.Set();
      }
    }
  }
  return S_OK;
}

HRESULT CStreamBinder::Write(const void *data, UInt32 size, UInt32 *processedSize)
{
  if (processedSize)
    *processedSize = 0;
  if (size == 0)
    return S_OK;

  if (!_readingWasClosed2)
  {
    _buf = data;
    _bufSize = size;
    _canRead_Event.Set();
    
    /*
    _canWrite_Event.Lock();
    if (_readingWasClosed)
      _readingWasClosed2 = true;
    */

    HANDLE events[2] = { _canWrite_Event, _readingWasClosed_Event };
    DWORD waitResult = ::WaitForMultipleObjects(2, events, FALSE, INFINITE);
    if (waitResult >= WAIT_OBJECT_0 + 2)
      return E_FAIL;

    size -= _bufSize;
    if (size != 0)
    {
      if (processedSize)
        *processedSize = size;
      return S_OK;
    }
    // if (waitResult == WAIT_OBJECT_0 + 1)
      _readingWasClosed2 = true;
  }

  // WritingWasCut = true;
  return k_My_HRESULT_WritingWasCut;
}
