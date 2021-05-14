// RandGen.h

#ifndef __CRYPTO_RAND_GEN_H
#define __CRYPTO_RAND_GEN_H

#include "../../../C/Sha256.h"

class CRandomGenerator
{
  Byte _buff[SHA256_DIGEST_SIZE];
  bool _needInit;

  void Init();
public:
  CRandomGenerator(): _needInit(true) {};
  void Generate(Byte *data, unsigned size);
};

extern CRandomGenerator g_RandomGenerator;

#endif
