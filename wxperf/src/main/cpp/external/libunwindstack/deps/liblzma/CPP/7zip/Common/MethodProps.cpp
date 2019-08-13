// MethodProps.cpp

#include "StdAfx.h"

#include "../../Common/StringToInt.h"

#include "MethodProps.h"

using namespace NWindows;

bool StringToBool(const UString &s, bool &res)
{
  if (s.IsEmpty() || (s[0] == '+' && s[1] == 0) || StringsAreEqualNoCase_Ascii(s, "ON"))
  {
    res = true;
    return true;
  }
  if ((s[0] == '-' && s[1] == 0) || StringsAreEqualNoCase_Ascii(s, "OFF"))
  {
    res = false;
    return true;
  }
  return false;
}

HRESULT PROPVARIANT_to_bool(const PROPVARIANT &prop, bool &dest)
{
  switch (prop.vt)
  {
    case VT_EMPTY: dest = true; return S_OK;
    case VT_BOOL: dest = (prop.boolVal != VARIANT_FALSE); return S_OK;
    case VT_BSTR: return StringToBool(prop.bstrVal, dest) ? S_OK : E_INVALIDARG;
  }
  return E_INVALIDARG;
}

unsigned ParseStringToUInt32(const UString &srcString, UInt32 &number)
{
  const wchar_t *start = srcString;
  const wchar_t *end;
  number = ConvertStringToUInt32(start, &end);
  return (unsigned)(end - start);
}

HRESULT ParsePropToUInt32(const UString &name, const PROPVARIANT &prop, UInt32 &resValue)
{
  // =VT_UI4
  // =VT_EMPTY
  // {stringUInt32}=VT_EMPTY

  if (prop.vt == VT_UI4)
  {
    if (!name.IsEmpty())
      return E_INVALIDARG;
    resValue = prop.ulVal;
    return S_OK;
  }
  if (prop.vt != VT_EMPTY)
    return E_INVALIDARG;
  if (name.IsEmpty())
    return S_OK;
  UInt32 v;
  if (ParseStringToUInt32(name, v) != name.Len())
    return E_INVALIDARG;
  resValue = v;
  return S_OK;
}

HRESULT ParseMtProp(const UString &name, const PROPVARIANT &prop, UInt32 defaultNumThreads, UInt32 &numThreads)
{
  if (name.IsEmpty())
  {
    switch (prop.vt)
    {
      case VT_UI4:
        numThreads = prop.ulVal;
        break;
      default:
      {
        bool val;
        RINOK(PROPVARIANT_to_bool(prop, val));
        numThreads = (val ? defaultNumThreads : 1);
        break;
      }
    }
    return S_OK;
  }
  if (prop.vt != VT_EMPTY)
    return E_INVALIDARG;
  return ParsePropToUInt32(name, prop, numThreads);
}


static HRESULT StringToDictSize(const UString &s, NCOM::CPropVariant &destProp)
{
  const wchar_t *end;
  UInt32 number = ConvertStringToUInt32(s, &end);
  unsigned numDigits = (unsigned)(end - s);
  if (numDigits == 0 || s.Len() > numDigits + 1)
    return E_INVALIDARG;
  
  if (s.Len() == numDigits)
  {
    if (number >= 64)
      return E_INVALIDARG;
    if (number < 32)
      destProp = (UInt32)((UInt32)1 << (unsigned)number);
    else
      destProp = (UInt64)((UInt64)1 << (unsigned)number);
    return S_OK;
  }
  
  unsigned numBits;
  
  switch (MyCharLower_Ascii(s[numDigits]))
  {
    case 'b': destProp = number; return S_OK;
    case 'k': numBits = 10; break;
    case 'm': numBits = 20; break;
    case 'g': numBits = 30; break;
    default: return E_INVALIDARG;
  }
  
  if (number < ((UInt32)1 << (32 - numBits)))
    destProp = (UInt32)(number << numBits);
  else
    destProp = (UInt64)((UInt64)number << numBits);
  
  return S_OK;
}


static HRESULT PROPVARIANT_to_DictSize(const PROPVARIANT &prop, NCOM::CPropVariant &destProp)
{
  if (prop.vt == VT_UI4)
  {
    UInt32 v = prop.ulVal;
    if (v >= 64)
      return E_INVALIDARG;
    if (v < 32)
      destProp = (UInt32)((UInt32)1 << (unsigned)v);
    else
      destProp = (UInt64)((UInt64)1 << (unsigned)v);
    return S_OK;
  }
  if (prop.vt == VT_BSTR)
    return StringToDictSize(prop.bstrVal, destProp);
  return E_INVALIDARG;
}


void CProps::AddProp32(PROPID propid, UInt32 level)
{
  CProp &prop = Props.AddNew();
  prop.IsOptional = true;
  prop.Id = propid;
  prop.Value = (UInt32)level;
}

class CCoderProps
{
  PROPID *_propIDs;
  NCOM::CPropVariant *_props;
  unsigned _numProps;
  unsigned _numPropsMax;
public:
  CCoderProps(unsigned numPropsMax)
  {
    _numPropsMax = numPropsMax;
    _numProps = 0;
    _propIDs = new PROPID[numPropsMax];
    _props = new NCOM::CPropVariant[numPropsMax];
  }
  ~CCoderProps()
  {
    delete []_propIDs;
    delete []_props;
  }
  void AddProp(const CProp &prop);
  HRESULT SetProps(ICompressSetCoderProperties *setCoderProperties)
  {
    return setCoderProperties->SetCoderProperties(_propIDs, _props, _numProps);
  }
};

void CCoderProps::AddProp(const CProp &prop)
{
  if (_numProps >= _numPropsMax)
    throw 1;
  _propIDs[_numProps] = prop.Id;
  _props[_numProps] = prop.Value;
  _numProps++;
}

HRESULT CProps::SetCoderProps(ICompressSetCoderProperties *scp, const UInt64 *dataSizeReduce) const
{
  CCoderProps coderProps(Props.Size() + (dataSizeReduce ? 1 : 0));
  FOR_VECTOR (i, Props)
    coderProps.AddProp(Props[i]);
  if (dataSizeReduce)
  {
    CProp prop;
    prop.Id = NCoderPropID::kReduceSize;
    prop.Value = *dataSizeReduce;
    coderProps.AddProp(prop);
  }
  return coderProps.SetProps(scp);
}


int CMethodProps::FindProp(PROPID id) const
{
  for (int i = Props.Size() - 1; i >= 0; i--)
    if (Props[i].Id == id)
      return i;
  return -1;
}

int CMethodProps::GetLevel() const
{
  int i = FindProp(NCoderPropID::kLevel);
  if (i < 0)
    return 5;
  if (Props[i].Value.vt != VT_UI4)
    return 9;
  UInt32 level = Props[i].Value.ulVal;
  return level > 9 ? 9 : (int)level;
}

struct CNameToPropID
{
  VARTYPE VarType;
  const char *Name;
};

static const CNameToPropID g_NameToPropID[] =
{
  { VT_UI4, "" },
  { VT_UI4, "d" },
  { VT_UI4, "mem" },
  { VT_UI4, "o" },
  { VT_UI4, "c" },
  { VT_UI4, "pb" },
  { VT_UI4, "lc" },
  { VT_UI4, "lp" },
  { VT_UI4, "fb" },
  { VT_BSTR, "mf" },
  { VT_UI4, "mc" },
  { VT_UI4, "pass" },
  { VT_UI4, "a" },
  { VT_UI4, "mt" },
  { VT_BOOL, "eos" },
  { VT_UI4, "x" },
  { VT_UI4, "reduceSize" }
};

static int FindPropIdExact(const UString &name)
{
  for (unsigned i = 0; i < ARRAY_SIZE(g_NameToPropID); i++)
    if (StringsAreEqualNoCase_Ascii(name, g_NameToPropID[i].Name))
      return i;
  return -1;
}

static bool ConvertProperty(const PROPVARIANT &srcProp, VARTYPE varType, NCOM::CPropVariant &destProp)
{
  if (varType == srcProp.vt)
  {
    destProp = srcProp;
    return true;
  }
  if (varType == VT_BOOL)
  {
    bool res;
    if (PROPVARIANT_to_bool(srcProp, res) != S_OK)
      return false;
    destProp = res;
    return true;
  }
  if (srcProp.vt == VT_EMPTY)
  {
    destProp = srcProp;
    return true;
  }
  return false;
}
    
static void SplitParams(const UString &srcString, UStringVector &subStrings)
{
  subStrings.Clear();
  UString s;
  unsigned len = srcString.Len();
  if (len == 0)
    return;
  for (unsigned i = 0; i < len; i++)
  {
    wchar_t c = srcString[i];
    if (c == L':')
    {
      subStrings.Add(s);
      s.Empty();
    }
    else
      s += c;
  }
  subStrings.Add(s);
}

static void SplitParam(const UString &param, UString &name, UString &value)
{
  int eqPos = param.Find(L'=');
  if (eqPos >= 0)
  {
    name.SetFrom(param, eqPos);
    value = param.Ptr(eqPos + 1);
    return;
  }
  unsigned i;
  for (i = 0; i < param.Len(); i++)
  {
    wchar_t c = param[i];
    if (c >= L'0' && c <= L'9')
      break;
  }
  name.SetFrom(param, i);
  value = param.Ptr(i);
}

static bool IsLogSizeProp(PROPID propid)
{
  switch (propid)
  {
    case NCoderPropID::kDictionarySize:
    case NCoderPropID::kUsedMemorySize:
    case NCoderPropID::kBlockSize:
    case NCoderPropID::kReduceSize:
      return true;
  }
  return false;
}

HRESULT CMethodProps::SetParam(const UString &name, const UString &value)
{
  int index = FindPropIdExact(name);
  if (index < 0)
    return E_INVALIDARG;
  const CNameToPropID &nameToPropID = g_NameToPropID[(unsigned)index];
  CProp prop;
  prop.Id = index;

  if (IsLogSizeProp(prop.Id))
  {
    RINOK(StringToDictSize(value, prop.Value));
  }
  else
  {
    NCOM::CPropVariant propValue;
    if (nameToPropID.VarType == VT_BSTR)
      propValue = value;
    else if (nameToPropID.VarType == VT_BOOL)
    {
      bool res;
      if (!StringToBool(value, res))
        return E_INVALIDARG;
      propValue = res;
    }
    else if (!value.IsEmpty())
    {
      UInt32 number;
      if (ParseStringToUInt32(value, number) == value.Len())
        propValue = number;
      else
        propValue = value;
    }
    if (!ConvertProperty(propValue, nameToPropID.VarType, prop.Value))
      return E_INVALIDARG;
  }
  Props.Add(prop);
  return S_OK;
}

HRESULT CMethodProps::ParseParamsFromString(const UString &srcString)
{
  UStringVector params;
  SplitParams(srcString, params);
  FOR_VECTOR (i, params)
  {
    const UString &param = params[i];
    UString name, value;
    SplitParam(param, name, value);
    RINOK(SetParam(name, value));
  }
  return S_OK;
}

HRESULT CMethodProps::ParseParamsFromPROPVARIANT(const UString &realName, const PROPVARIANT &value)
{
  if (realName.Len() == 0)
  {
    // [empty]=method
    return E_INVALIDARG;
  }
  if (value.vt == VT_EMPTY)
  {
    // {realName}=[empty]
    UString name, valueStr;
    SplitParam(realName, name, valueStr);
    return SetParam(name, valueStr);
  }
  
  // {realName}=value
  int index = FindPropIdExact(realName);
  if (index < 0)
    return E_INVALIDARG;
  const CNameToPropID &nameToPropID = g_NameToPropID[(unsigned)index];
  CProp prop;
  prop.Id = index;
  
  if (IsLogSizeProp(prop.Id))
  {
    RINOK(PROPVARIANT_to_DictSize(value, prop.Value));
  }
  else
  {
    if (!ConvertProperty(value, nameToPropID.VarType, prop.Value))
      return E_INVALIDARG;
  }
  Props.Add(prop);
  return S_OK;
}

HRESULT COneMethodInfo::ParseMethodFromString(const UString &s)
{
  MethodName.Empty();
  int splitPos = s.Find(L':');
  {
    UString temp = s;
    if (splitPos >= 0)
      temp.DeleteFrom(splitPos);
    if (!temp.IsAscii())
      return E_INVALIDARG;
    MethodName.SetFromWStr_if_Ascii(temp);
  }
  if (splitPos < 0)
    return S_OK;
  PropsString = s.Ptr(splitPos + 1);
  return ParseParamsFromString(PropsString);
}

HRESULT COneMethodInfo::ParseMethodFromPROPVARIANT(const UString &realName, const PROPVARIANT &value)
{
  if (!realName.IsEmpty() && !StringsAreEqualNoCase_Ascii(realName, "m"))
    return ParseParamsFromPROPVARIANT(realName, value);
  // -m{N}=method
  if (value.vt != VT_BSTR)
    return E_INVALIDARG;
  return ParseMethodFromString(value.bstrVal);
}
