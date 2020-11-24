// StreamBinder.h

#ifndef __STREAM_BINDER_H
#define __STREAM_BINDER_H

#include "../../Windows/Synchronization.h"

#include "../IStream.h"

/*
We don't use probably UNSAFE version:
reader thread:
     _canWrite_Event.Set();
     _readingWasClosed = true
     _canWrite_Event.Set();
writer thread:
     _canWrite_Event.Wait()
      if (_readingWasClosed)
Can second call of _canWrite_Event.Set() be executed without memory barrier, if event is already set?
*/

class CStreamBinder
{
  NWindows::NSynchronization::CAutoResetEvent _canWrite_Event;
  NWindows::NSynchronization::CManualResetEvent _canRead_Event;
  NWindows::NSynchronization::CManualResetEvent _readingWasClosed_Event;

  // bool _readingWasClosed;
  bool _readingWasClosed2;
  // bool WritingWasCut;
  bool _waitWrite;
  UInt32 _bufSize;
  const void *_buf;
public:
  UInt64 ProcessedSize;

  WRes CreateEvents();
  void CreateStreams(ISequentialInStream **inStream, ISequentialOutStream **outStream);
  
  void ReInit();
  
  HRESULT Read(void *data, UInt32 size, UInt32 *processedSize);
  HRESULT Write(const void *data, UInt32 size, UInt32 *processedSize);

  void CloseRead()
  {
    _readingWasClosed_Event.Set();
    // _readingWasClosed = true;
    // _canWrite_Event.Set();
  }
  
  void CloseWrite()
  {
    _buf = NULL;
    _bufSize = 0;
    _canRead_Event.Set();
  }
};

#endif
