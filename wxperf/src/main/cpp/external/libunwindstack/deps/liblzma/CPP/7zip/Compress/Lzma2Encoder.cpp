// Lzma2Encoder.cpp

#include "StdAfx.h"

#include "../../../C/Alloc.h"

#include "../Common/CWrappers.h"
#include "../Common/StreamUtils.h"

#include "Lzma2Encoder.h"

namespace NCompress {

namespace NLzma {

HRESULT SetLzmaProp(PROPID propID, const PROPVARIANT &prop, CLzmaEncProps &ep);

}

namespace NLzma2 {

CEncoder::CEncoder()
{
  _encoder = 0;
  _encoder = Lzma2Enc_Create(&g_Alloc, &g_BigAlloc);
  if (_encoder == 0)
    throw 1;
}

CEncoder::~CEncoder()
{
  if (_encoder != 0)
    Lzma2Enc_Destroy(_encoder);
}

HRESULT SetLzma2Prop(PROPID propID, const PROPVARIANT &prop, CLzma2EncProps &lzma2Props)
{
  switch (propID)
  {
    case NCoderPropID::kBlockSize:
    {
      if (prop.vt == VT_UI4)
        lzma2Props.blockSize = prop.ulVal;
      else if (prop.vt == VT_UI8)
      {
        size_t v = (size_t)prop.uhVal.QuadPart;
        if (v != prop.uhVal.QuadPart)
          return E_INVALIDARG;
        lzma2Props.blockSize = v;
      }
      else
        return E_INVALIDARG;
      break;
    }
    case NCoderPropID::kNumThreads:
      if (prop.vt != VT_UI4) return E_INVALIDARG; lzma2Props.numTotalThreads = (int)(prop.ulVal); break;
    default:
      RINOK(NLzma::SetLzmaProp(propID, prop, lzma2Props.lzmaProps));
  }
  return S_OK;
}

STDMETHODIMP CEncoder::SetCoderProperties(const PROPID *propIDs,
    const PROPVARIANT *coderProps, UInt32 numProps)
{
  CLzma2EncProps lzma2Props;
  Lzma2EncProps_Init(&lzma2Props);

  for (UInt32 i = 0; i < numProps; i++)
  {
    RINOK(SetLzma2Prop(propIDs[i], coderProps[i], lzma2Props));
  }
  return SResToHRESULT(Lzma2Enc_SetProps(_encoder, &lzma2Props));
}

STDMETHODIMP CEncoder::WriteCoderProperties(ISequentialOutStream *outStream)
{
  Byte prop = Lzma2Enc_WriteProperties(_encoder);
  return WriteStream(outStream, &prop, 1);
}

STDMETHODIMP CEncoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 * /* inSize */, const UInt64 * /* outSize */, ICompressProgressInfo *progress)
{
  CSeqInStreamWrap inWrap(inStream);
  CSeqOutStreamWrap outWrap(outStream);
  CCompressProgressWrap progressWrap(progress);

  SRes res = Lzma2Enc_Encode(_encoder, &outWrap.p, &inWrap.p, progress ? &progressWrap.p : NULL);
  if (res == SZ_ERROR_READ && inWrap.Res != S_OK)
    return inWrap.Res;
  if (res == SZ_ERROR_WRITE && outWrap.Res != S_OK)
    return outWrap.Res;
  if (res == SZ_ERROR_PROGRESS && progressWrap.Res != S_OK)
    return progressWrap.Res;
  return SResToHRESULT(res);
}
  
}}
