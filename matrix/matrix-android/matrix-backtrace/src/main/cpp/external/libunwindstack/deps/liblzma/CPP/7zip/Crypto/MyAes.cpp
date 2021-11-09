// Crypto/MyAes.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "MyAes.h"

namespace NCrypto {

static struct CAesTabInit { CAesTabInit() { AesGenTables();} } g_AesTabInit;

CAesCbcCoder::CAesCbcCoder(bool encodeMode, unsigned keySize):
  _keySize(keySize),
  _keyIsSet(false),
  _encodeMode(encodeMode)
{
  _offset = ((0 - (unsigned)(ptrdiff_t)_aes) & 0xF) / sizeof(UInt32);
  memset(_iv, 0, AES_BLOCK_SIZE);
  SetFunctions(0);
}

STDMETHODIMP CAesCbcCoder::Init()
{
  AesCbc_Init(_aes + _offset, _iv);
  return _keyIsSet ? S_OK : E_FAIL;
}

STDMETHODIMP_(UInt32) CAesCbcCoder::Filter(Byte *data, UInt32 size)
{
  if (!_keyIsSet)
    return 0;
  if (size == 0)
    return 0;
  if (size < AES_BLOCK_SIZE)
    return AES_BLOCK_SIZE;
  size >>= 4;
  _codeFunc(_aes + _offset, data, size);
  return size << 4;
}

STDMETHODIMP CAesCbcCoder::SetKey(const Byte *data, UInt32 size)
{
  if ((size & 0x7) != 0 || size < 16 || size > 32)
    return E_INVALIDARG;
  if (_keySize != 0 && size != _keySize)
    return E_INVALIDARG;
  AES_SET_KEY_FUNC setKeyFunc = _encodeMode ? Aes_SetKey_Enc : Aes_SetKey_Dec;
  setKeyFunc(_aes + _offset + 4, data, size);
  _keyIsSet = true;
  return S_OK;
}

STDMETHODIMP CAesCbcCoder::SetInitVector(const Byte *data, UInt32 size)
{
  if (size != AES_BLOCK_SIZE)
    return E_INVALIDARG;
  memcpy(_iv, data, size);
  CAesCbcCoder::Init(); // don't call virtual function here !!!
  return S_OK;
}

EXTERN_C_BEGIN

void MY_FAST_CALL AesCbc_Encode(UInt32 *ivAes, Byte *data, size_t numBlocks);
void MY_FAST_CALL AesCbc_Decode(UInt32 *ivAes, Byte *data, size_t numBlocks);
void MY_FAST_CALL AesCtr_Code(UInt32 *ivAes, Byte *data, size_t numBlocks);

void MY_FAST_CALL AesCbc_Encode_Intel(UInt32 *ivAes, Byte *data, size_t numBlocks);
void MY_FAST_CALL AesCbc_Decode_Intel(UInt32 *ivAes, Byte *data, size_t numBlocks);
void MY_FAST_CALL AesCtr_Code_Intel(UInt32 *ivAes, Byte *data, size_t numBlocks);

EXTERN_C_END

bool CAesCbcCoder::SetFunctions(UInt32 algo)
{
  _codeFunc = _encodeMode ?
      g_AesCbc_Encode :
      g_AesCbc_Decode;
  if (algo == 1)
  {
    _codeFunc = _encodeMode ?
        AesCbc_Encode:
        AesCbc_Decode;
  }
  if (algo == 2)
  {
    #ifdef MY_CPU_X86_OR_AMD64
    if (g_AesCbc_Encode != AesCbc_Encode_Intel)
    #endif
      return false;
  }
  return true;
}

STDMETHODIMP CAesCbcCoder::SetCoderProperties(const PROPID *propIDs, const PROPVARIANT *coderProps, UInt32 numProps)
{
  for (UInt32 i = 0; i < numProps; i++)
  {
    const PROPVARIANT &prop = coderProps[i];
    if (propIDs[i] == NCoderPropID::kDefaultProp)
    {
      if (prop.vt != VT_UI4)
        return E_INVALIDARG;
      if (!SetFunctions(prop.ulVal))
        return E_NOTIMPL;
    }
  }
  return S_OK;
}

}
