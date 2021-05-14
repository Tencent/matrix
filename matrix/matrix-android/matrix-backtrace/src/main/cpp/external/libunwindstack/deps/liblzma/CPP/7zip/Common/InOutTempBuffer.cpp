// InOutTempBuffer.cpp

#include "StdAfx.h"

#include "../../../C/7zCrc.h"

#include "../../Common/Defs.h"

#include "InOutTempBuffer.h"
#include "StreamUtils.h"

using namespace NWindows;
using namespace NFile;
using namespace NDir;

static const size_t kTempBufSize = (1 << 20);

#define kTempFilePrefixString FTEXT("7zt")

CInOutTempBuffer::CInOutTempBuffer(): _buf(NULL) { }

void CInOutTempBuffer::Create()
{
  if (!_buf)
    _buf = new Byte[kTempBufSize];
}

CInOutTempBuffer::~CInOutTempBuffer()
{
  delete []_buf;
}

void CInOutTempBuffer::InitWriting()
{
  _bufPos = 0;
  _tempFileCreated = false;
  _size = 0;
  _crc = CRC_INIT_VAL;
}

bool CInOutTempBuffer::WriteToFile(const void *data, UInt32 size)
{
  if (size == 0)
    return true;
  if (!_tempFileCreated)
  {
    if (!_tempFile.CreateRandomInTempFolder(kTempFilePrefixString, &_outFile))
      return false;
    _tempFileCreated = true;
  }
  UInt32 processed;
  if (!_outFile.Write(data, size, processed))
    return false;
  _crc = CrcUpdate(_crc, data, processed);
  _size += processed;
  return (processed == size);
}

bool CInOutTempBuffer::Write(const void *data, UInt32 size)
{
  if (size == 0)
    return true;
  size_t cur = kTempBufSize - _bufPos;
  if (cur != 0)
  {
    if (cur > size)
      cur = size;
    memcpy(_buf + _bufPos, data, cur);
    _crc = CrcUpdate(_crc, data, cur);
    _bufPos += cur;
    _size += cur;
    size -= (UInt32)cur;
    data = ((const Byte *)data) + cur;
  }
  return WriteToFile(data, size);
}

HRESULT CInOutTempBuffer::WriteToStream(ISequentialOutStream *stream)
{
  if (!_outFile.Close())
    return E_FAIL;

  UInt64 size = 0;
  UInt32 crc = CRC_INIT_VAL;

  if (_bufPos != 0)
  {
    RINOK(WriteStream(stream, _buf, _bufPos));
    crc = CrcUpdate(crc, _buf, _bufPos);
    size += _bufPos;
  }
  
  if (_tempFileCreated)
  {
    NIO::CInFile inFile;
    if (!inFile.Open(_tempFile.GetPath()))
      return E_FAIL;
    while (size < _size)
    {
      UInt32 processed;
      if (!inFile.ReadPart(_buf, kTempBufSize, processed))
        return E_FAIL;
      if (processed == 0)
        break;
      RINOK(WriteStream(stream, _buf, processed));
      crc = CrcUpdate(crc, _buf, processed);
      size += processed;
    }
  }
  
  return (_crc == crc && size == _size) ? S_OK : E_FAIL;
}

/*
STDMETHODIMP CSequentialOutTempBufferImp::Write(const void *data, UInt32 size, UInt32 *processed)
{
  if (!_buf->Write(data, size))
  {
    if (processed)
      *processed = 0;
    return E_FAIL;
  }
  if (processed)
    *processed = size;
  return S_OK;
}
*/
