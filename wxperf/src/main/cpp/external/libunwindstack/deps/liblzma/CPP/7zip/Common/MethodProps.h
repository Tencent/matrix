// MethodProps.h

#ifndef __7Z_METHOD_PROPS_H
#define __7Z_METHOD_PROPS_H

#include "../../Common/MyString.h"

#include "../../Windows/PropVariant.h"

#include "../ICoder.h"

bool StringToBool(const UString &s, bool &res);
HRESULT PROPVARIANT_to_bool(const PROPVARIANT &prop, bool &dest);
unsigned ParseStringToUInt32(const UString &srcString, UInt32 &number);
HRESULT ParsePropToUInt32(const UString &name, const PROPVARIANT &prop, UInt32 &resValue);

HRESULT ParseMtProp(const UString &name, const PROPVARIANT &prop, UInt32 defaultNumThreads, UInt32 &numThreads);

struct CProp
{
  PROPID Id;
  bool IsOptional;
  NWindows::NCOM::CPropVariant Value;
  CProp(): IsOptional(false) {}
};

struct CProps
{
  CObjectVector<CProp> Props;

  void Clear() { Props.Clear(); }

  bool AreThereNonOptionalProps() const
  {
    FOR_VECTOR (i, Props)
      if (!Props[i].IsOptional)
        return true;
    return false;
  }

  void AddProp32(PROPID propid, UInt32 level);

  void AddProp_Ascii(PROPID propid, const char *s)
  {
    CProp &prop = Props.AddNew();
    prop.IsOptional = true;
    prop.Id = propid;
    prop.Value = s;
  }

  HRESULT SetCoderProps(ICompressSetCoderProperties *scp, const UInt64 *dataSizeReduce) const;
};

class CMethodProps: public CProps
{
  HRESULT SetParam(const UString &name, const UString &value);
public:
  int GetLevel() const;
  int Get_NumThreads() const
  {
    int i = FindProp(NCoderPropID::kNumThreads);
    if (i >= 0)
      if (Props[i].Value.vt == VT_UI4)
        return (int)Props[i].Value.ulVal;
    return -1;
  }

  bool Get_DicSize(UInt32 &res) const
  {
    res = 0;
    int i = FindProp(NCoderPropID::kDictionarySize);
    if (i >= 0)
      if (Props[i].Value.vt == VT_UI4)
      {
        res = Props[i].Value.ulVal;
        return true;
      }
    return false;
  }

  int FindProp(PROPID id) const;

  UInt32 Get_Lzma_Algo() const
  {
    int i = FindProp(NCoderPropID::kAlgorithm);
    if (i >= 0)
      if (Props[i].Value.vt == VT_UI4)
        return Props[i].Value.ulVal;
    return GetLevel() >= 5 ? 1 : 0;
  }

  UInt32 Get_Lzma_DicSize() const
  {
    int i = FindProp(NCoderPropID::kDictionarySize);
    if (i >= 0)
      if (Props[i].Value.vt == VT_UI4)
        return Props[i].Value.ulVal;
    int level = GetLevel();
    return level <= 5 ? (1 << (level * 2 + 14)) : (level == 6 ? (1 << 25) : (1 << 26));
  }

  bool Are_Lzma_Model_Props_Defined() const
  {
    if (FindProp(NCoderPropID::kPosStateBits) >= 0) return true;
    if (FindProp(NCoderPropID::kLitContextBits) >= 0) return true;
    if (FindProp(NCoderPropID::kLitPosBits) >= 0) return true;
    return false;
  }

  UInt32 Get_Lzma_NumThreads(bool &fixedNumber) const
  {
    fixedNumber = false;
    int numThreads = Get_NumThreads();
    if (numThreads >= 0)
    {
      fixedNumber = true;
      return numThreads < 2 ? 1 : 2;
    }
    return Get_Lzma_Algo() == 0 ? 1 : 2;
  }

  UInt32 Get_BZip2_NumThreads(bool &fixedNumber) const
  {
    fixedNumber = false;
    int numThreads = Get_NumThreads();
    if (numThreads >= 0)
    {
      fixedNumber = true;
      if (numThreads < 1) return 1;
      if (numThreads > 64) return 64;
      return numThreads;
    }
    return 1;
  }

  UInt32 Get_BZip2_BlockSize() const
  {
    int i = FindProp(NCoderPropID::kDictionarySize);
    if (i >= 0)
      if (Props[i].Value.vt == VT_UI4)
      {
        UInt32 blockSize = Props[i].Value.ulVal;
        const UInt32 kDicSizeMin = 100000;
        const UInt32 kDicSizeMax = 900000;
        if (blockSize < kDicSizeMin) blockSize = kDicSizeMin;
        if (blockSize > kDicSizeMax) blockSize = kDicSizeMax;
        return blockSize;
      }
    int level = GetLevel();
    return 100000 * (level >= 5 ? 9 : (level >= 1 ? level * 2 - 1: 1));
  }

  UInt32 Get_Ppmd_MemSize() const
  {
    int i = FindProp(NCoderPropID::kUsedMemorySize);
    if (i >= 0)
      if (Props[i].Value.vt == VT_UI4)
        return Props[i].Value.ulVal;
    int level = GetLevel();
    return level >= 9 ? (192 << 20) : ((UInt32)1 << (level + 19));
  }

  void AddProp_Level(UInt32 level)
  {
    AddProp32(NCoderPropID::kLevel, level);
  }

  void AddProp_NumThreads(UInt32 numThreads)
  {
    AddProp32(NCoderPropID::kNumThreads, numThreads);
  }

  HRESULT ParseParamsFromString(const UString &srcString);
  HRESULT ParseParamsFromPROPVARIANT(const UString &realName, const PROPVARIANT &value);
};

class COneMethodInfo: public CMethodProps
{
public:
  AString MethodName;
  UString PropsString;
  
  void Clear()
  {
    CMethodProps::Clear();
    MethodName.Empty();
    PropsString.Empty();
  }
  bool IsEmpty() const { return MethodName.IsEmpty() && Props.IsEmpty(); }
  HRESULT ParseMethodFromPROPVARIANT(const UString &realName, const PROPVARIANT &value);
  HRESULT ParseMethodFromString(const UString &s);
};

#endif
