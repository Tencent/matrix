// MyAesReg.cpp

#include "StdAfx.h"

#include "../Common/RegisterCodec.h"

#include "MyAes.h"

namespace NCrypto {

REGISTER_FILTER_E(AES256CBC,
    CAesCbcDecoder(32),
    CAesCbcEncoder(32),
    0x6F00181, "AES256CBC")

}
