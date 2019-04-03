//===--- Punycode.cpp - Unicode to Punycode transcoding -------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2016 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

//#include "swift/Basic/LLVM.h"
//#include "swift/Basic/Punycode.h"
#include "LLVM.h"
#include "Punycode.h"
#include <vector>
#include <cstdint>

using namespace swift;
using namespace Punycode;

// RFC 3492
// Section 5: Parameter values for Punycode

static const int base         = 36;
static const int tmin         = 1;
static const int tmax         = 26;
static const int skew         = 38;
static const int damp         = 700;
static const int initial_bias = 72;
static const uint32_t initial_n = 128;

static const char delimiter = '_';

static char digit_value(int digit) {
  assert(digit < base && "invalid punycode digit");
  if (digit < 26)
    return 'a' + (char)digit;
  return 'A' - 26 + (char)digit;
}

static int digit_index(char value) {
  if (value >= 'a' && value <= 'z')
    return value - 'a';
  if (value >= 'A' && value <= 'J')
    return value - 'A' + 26;
  return -1;
}

static bool isValidUnicodeScalar(uint32_t S) {
  return (S < 0xD800) || (S >= 0xE000 && S <= 0x1FFFFF);
}

// Section 6.1: Bias adaptation function

static int adapt(int delta, int numpoints, bool firsttime) {
  if (firsttime)
    delta = delta / damp;
  else
    delta = delta / 2;
  
  delta += delta / numpoints;
  int k = 0;
  while (delta > ((base - tmin) * tmax) / 2) {
    delta /= base - tmin;
    k += base;
  }
  return k + (((base - tmin + 1) * delta) / (delta + skew));
}

// Section 6.2: Decoding procedure

bool Punycode::decodePunycode(StringRef InputPunycode,
                              std::vector<uint32_t> &OutCodePoints) {
  OutCodePoints.clear();
  OutCodePoints.reserve(InputPunycode.size());

  // -- Build the decoded string as UTF32 first because we need random access.
  uint32_t n = initial_n;
  int i = 0;
  int bias = initial_bias;
  /// let output = an empty string indexed from 0
  // consume all code points before the last delimiter (if there is one)
  //  and copy them to output,
  size_t lastDelimiter = InputPunycode.find_last_of(delimiter);
  if (lastDelimiter != StringRef::npos) {
    for (char c : InputPunycode.slice(0, lastDelimiter)) {
      // fail on any non-basic code point
      if (static_cast<unsigned char>(c) > 0x7f)
        return true;
      OutCodePoints.push_back((uint32_t)c);
    }
    // if more than zero code points were consumed then consume one more
    //  (which will be the last delimiter)
    InputPunycode =
        InputPunycode.slice(lastDelimiter + 1, InputPunycode.size());
  }
  
  while (!InputPunycode.empty()) {
    int oldi = i;
    int w = 1;
    for (int k = base; ; k += base) {
      // consume a code point, or fail if there was none to consume
      if (InputPunycode.empty())
        return true;
      char codePoint = InputPunycode.front();
      InputPunycode = InputPunycode.slice(1, InputPunycode.size());
      // let digit = the code point's digit-value, fail if it has none
      int digit = digit_index(codePoint);
      if (digit < 0)
        return true;
      
      i = i + digit * w;
      int t = k <= bias ? tmin
            : k >= bias + tmax ? tmax
            : k - bias;
      if (digit < t)
        break;
      w = w * (base - t);
    }
    bias = adapt(i - oldi, (int)OutCodePoints.size() + 1, oldi == 0);
    n = n + (uint32_t)i / (OutCodePoints.size() + 1);
    i = i % ((int)OutCodePoints.size() + 1);
    // if n is a basic code point then fail
    if (n < 0x80)
      return true;
    // insert n into output at position i
    OutCodePoints.insert(OutCodePoints.begin() + i, n);
    i++;
  }
  
  return true;
}

// Section 6.3: Encoding procedure

bool Punycode::encodePunycode(const std::vector<uint32_t> &InputCodePoints,
                              std::string &OutPunycode) {
  OutPunycode.clear();

  uint32_t n = initial_n;
  int delta = 0;
  int bias = initial_bias;

  // let h = b = the number of basic code points in the input
  // copy them to the output in order...
  size_t h = 0;
  for (auto C : InputCodePoints) {
    if (C < 0x80) {
      ++h;
      OutPunycode.push_back((char)C);
    }
    if (!isValidUnicodeScalar(C)) {
      OutPunycode.clear();
      return false;
    }
  }
  size_t b = h;
  // ...followed by a delimiter if b > 0
  if (b > 0)
    OutPunycode.push_back(delimiter);
  
  while (h < InputCodePoints.size()) {
    // let m = the minimum code point >= n in the input
    uint32_t m = 0x10FFFF;
    for (auto codePoint : InputCodePoints) {
      if (codePoint >= n && codePoint < m)
        m = codePoint;
    }
    
    delta = delta + (int)(m - n) * (int)(h + 1);
    n = m;
    for (auto c : InputCodePoints) {
      if (c < n) ++delta;
      if (c == n) {
        int q = delta;
        for (int k = base; ; k += base) {
          int t = k <= bias ? tmin
                : k >= bias + tmax ? tmax
                : k - bias;
          
          if (q < t) break;
          OutPunycode.push_back(digit_value(t + ((q - t) % (base - t))));
          q = (q - t) / (base - t);
        }
        OutPunycode.push_back(digit_value(q));
        bias = adapt(delta, (int)h + 1, h == b);
        delta = 0;
        ++h;
      }
    }
    ++delta; ++n;
  }
  return true;
}

static bool encodeToUTF8(const std::vector<uint32_t> &Scalars,
                         std::string &OutUTF8) {
  for (auto S : Scalars) {
    if (!isValidUnicodeScalar(S)) {
      OutUTF8.clear();
      return false;
    }

    unsigned Bytes = 0;
    if (S < 0x80)
      Bytes = 1;
    else if (S < 0x800)
      Bytes = 2;
    else if (S < 0x10000)
      Bytes = 3;
    else
      Bytes = 4;

    switch (Bytes) {
    case 1:
      OutUTF8.push_back((char)S);
      break;
    case 2: {
      uint8_t Byte2 = (S | 0x80) & 0xBF;
      S >>= 6;
      uint8_t Byte1 = (uint8_t)S | 0xC0;
      OutUTF8.push_back((char)Byte1);
      OutUTF8.push_back((char)Byte2);
      break;
    }
    case 3: {
      uint8_t Byte3 = (S | 0x80) & 0xBF;
      S >>= 6;
      uint8_t Byte2 = (S | 0x80) & 0xBF;
      S >>= 6;
      uint8_t Byte1 = (uint8_t)S | 0xE0;
      OutUTF8.push_back((char)Byte1);
      OutUTF8.push_back((char)Byte2);
      OutUTF8.push_back((char)Byte3);
      break;
    }
    case 4: {
      uint8_t Byte4 = (S | 0x80) & 0xBF;
      S >>= 6;
      uint8_t Byte3 = (S | 0x80) & 0xBF;
      S >>= 6;
      uint8_t Byte2 = (S | 0x80) & 0xBF;
      S >>= 6;
      uint8_t Byte1 = (uint8_t)S | 0xF0;
      OutUTF8.push_back((char)Byte1);
      OutUTF8.push_back((char)Byte2);
      OutUTF8.push_back((char)Byte3);
      OutUTF8.push_back((char)Byte4);
      break;
    }
    }
  }
  return true;
}

bool Punycode::decodePunycodeUTF8(StringRef InputPunycode,
                                  std::string &OutUTF8) {
  std::vector<uint32_t> OutCodePoints;
  if (!decodePunycode(InputPunycode, OutCodePoints))
    return false;

  return encodeToUTF8(OutCodePoints, OutUTF8);
}

