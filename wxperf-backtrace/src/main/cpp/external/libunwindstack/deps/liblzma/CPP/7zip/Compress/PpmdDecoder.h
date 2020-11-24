// PpmdDecoder.h
// 2009-03-11 : Igor Pavlov : Public domain

#ifndef __COMPRESS_PPMD_DECODER_H
#define __COMPRESS_PPMD_DECODER_H

#include "../../../C/Ppmd7.h"

#include "../../Common/MyCom.h"

#include "../Common/CWrappers.h"

#include "../ICoder.h"

namespace NCompress {
namespace NPpmd {

class CDecoder :
  public ICompressCoder,
  public ICompressSetDecoderProperties2,
  public ICompressGetInStreamProcessedSize,
  #ifndef NO_READ_FROM_CODER
  public ICompressSetInStream,
  public ICompressSetOutStreamSize,
  public ISequentialInStream,
  #endif
  public CMyUnknownImp
{
  Byte *_outBuf;
  CPpmd7z_RangeDec _rangeDec;
  CByteInBufWrap _inStream;
  CPpmd7 _ppmd;

  Byte _order;
  bool _outSizeDefined;
  int _status;
  UInt64 _outSize;
  UInt64 _processedSize;

  HRESULT CodeSpec(Byte *memStream, UInt32 size);

public:

  #ifndef NO_READ_FROM_CODER
  CMyComPtr<ISequentialInStream> InSeqStream;
  #endif

  MY_QUERYINTERFACE_BEGIN2(ICompressCoder)
  MY_QUERYINTERFACE_ENTRY(ICompressSetDecoderProperties2)
  // MY_QUERYINTERFACE_ENTRY(ICompressSetFinishMode)
  MY_QUERYINTERFACE_ENTRY(ICompressGetInStreamProcessedSize)
  #ifndef NO_READ_FROM_CODER
  MY_QUERYINTERFACE_ENTRY(ICompressSetInStream)
  MY_QUERYINTERFACE_ENTRY(ICompressSetOutStreamSize)
  MY_QUERYINTERFACE_ENTRY(ISequentialInStream)
  #endif
  MY_QUERYINTERFACE_END
  MY_ADDREF_RELEASE


  STDMETHOD(Code)(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);
  STDMETHOD(SetDecoderProperties2)(const Byte *data, UInt32 size);
  STDMETHOD(GetInStreamProcessedSize)(UInt64 *value);
  
  STDMETHOD(SetOutStreamSize)(const UInt64 *outSize);

  #ifndef NO_READ_FROM_CODER
  STDMETHOD(SetInStream)(ISequentialInStream *inStream);
  STDMETHOD(ReleaseInStream)();
  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);
  #endif

  CDecoder(): _outBuf(NULL), _outSizeDefined(false)
  {
    Ppmd7z_RangeDec_CreateVTable(&_rangeDec);
    _rangeDec.Stream = &_inStream.vt;
    Ppmd7_Construct(&_ppmd);
  }

  ~CDecoder();
};

}}

#endif
