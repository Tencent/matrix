//
// Created by tomystang on 2020/11/26.
//

#include <util/Auxiliary.h>
#include <ctime>
#include <unistd.h>

using namespace memguard;

static TLSVAR uint32_t sLastRndValue = 0xFCDE97AB;

void memguard::random::InitializeRndSeed() {
    sLastRndValue = ::time(nullptr) + ::gettid();
}

uint32_t memguard::random::GenerateUnsignedInt32() {
    sLastRndValue ^= sLastRndValue << 7;
    sLastRndValue ^= sLastRndValue >> 17;
    sLastRndValue ^= sLastRndValue << 5;
    return sLastRndValue;
}