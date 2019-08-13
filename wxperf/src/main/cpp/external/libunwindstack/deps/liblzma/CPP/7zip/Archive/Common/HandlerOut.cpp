// HandlerOut.cpp

#include "StdAfx.h"

#ifndef _7ZIP_ST
#include "../../../Windows/System.h"
#endif

#include "../Common/ParseProperties.h"

#include "HandlerOut.h"

using namespace NWindows;

namespace NArchive {

static void SetMethodProp32(COneMethodInfo &m, PROPID propID, UInt32 value)
{
  if (m.FindProp(propID) < 0)
    m.AddProp32(propID, value);
}

void CMultiMethodProps::SetGlobalLevelAndThreads(COneMethodInfo &oneMethodInfo
    #ifndef _7ZIP_ST
    , UInt32 numThreads
    #endif
    )
{
  UInt32 level = _level;
  if (level != (UInt32)(Int32)-1)
    SetMethodProp32(oneMethodInfo, NCoderPropID::kLevel, (UInt32)level);
  
  #ifndef _7ZIP_ST
  SetMethodProp32(oneMethodInfo, NCoderPropID::kNumThreads, numThreads);
  #endif
}

void CMultiMethodProps::Init()
{
  #ifndef _7ZIP_ST
  _numProcessors = _numThreads = NSystem::GetNumberOfProcessors();
  #endif
  
  _level = (UInt32)(Int32)-1;
  _analysisLevel = -1;

  _autoFilter = true;
  _crcSize = 4;
  _filterMethod.Clear();
  _methods.Clear();
}

HRESULT CMultiMethodProps::SetProperty(const wchar_t *nameSpec, const PROPVARIANT &value)
{
  UString name = nameSpec;
  name.MakeLower_Ascii();
  if (name.IsEmpty())
    return E_INVALIDARG;
  
  if (name[0] == 'x')
  {
    name.Delete(0);
    _level = 9;
    return ParsePropToUInt32(name, value, _level);
  }

  if (name.IsPrefixedBy_Ascii_NoCase("yx"))
  {
    name.Delete(0, 2);
    UInt32 v = 9;
    RINOK(ParsePropToUInt32(name, value, v));
    _analysisLevel = (int)v;
    return S_OK;
  }
  
  if (name.IsEqualTo("crc"))
  {
    name.Delete(0, 3);
    _crcSize = 4;
    return ParsePropToUInt32(name, value, _crcSize);
  }
  
  UInt32 number;
  unsigned index = ParseStringToUInt32(name, number);
  UString realName = name.Ptr(index);
  if (index == 0)
  {
    if (name.IsPrefixedBy_Ascii_NoCase("mt"))
    {
      #ifndef _7ZIP_ST
      RINOK(ParseMtProp(name.Ptr(2), value, _numProcessors, _numThreads));
      #endif
      
      return S_OK;
    }
    if (name.IsEqualTo("f"))
    {
      HRESULT res = PROPVARIANT_to_bool(value, _autoFilter);
      if (res == S_OK)
        return res;
      if (value.vt != VT_BSTR)
        return E_INVALIDARG;
      return _filterMethod.ParseMethodFromPROPVARIANT(UString(), value);
    }
    number = 0;
  }
  if (number > 64)
    return E_FAIL;
  for (int j = _methods.Size(); j <= (int)number; j++)
    _methods.Add(COneMethodInfo());
  return _methods[number].ParseMethodFromPROPVARIANT(realName, value);
}

void CSingleMethodProps::Init()
{
  Clear();
  _level = (UInt32)(Int32)-1;
  
  #ifndef _7ZIP_ST
  _numProcessors = _numThreads = NWindows::NSystem::GetNumberOfProcessors();
  AddProp_NumThreads(_numThreads);
  #endif
}

HRESULT CSingleMethodProps::SetProperties(const wchar_t * const *names, const PROPVARIANT *values, UInt32 numProps)
{
  Init();
  for (UInt32 i = 0; i < numProps; i++)
  {
    UString name = names[i];
    name.MakeLower_Ascii();
    if (name.IsEmpty())
      return E_INVALIDARG;
    const PROPVARIANT &value = values[i];
    if (name[0] == L'x')
    {
      UInt32 a = 9;
      RINOK(ParsePropToUInt32(name.Ptr(1), value, a));
      _level = a;
      AddProp_Level(a);
    }
    else if (name.IsPrefixedBy_Ascii_NoCase("mt"))
    {
      #ifndef _7ZIP_ST
      RINOK(ParseMtProp(name.Ptr(2), value, _numProcessors, _numThreads));
      AddProp_NumThreads(_numThreads);
      #endif
    }
    else
    {
      RINOK(ParseMethodFromPROPVARIANT(names[i], value));
    }
  }
  return S_OK;
}

}
