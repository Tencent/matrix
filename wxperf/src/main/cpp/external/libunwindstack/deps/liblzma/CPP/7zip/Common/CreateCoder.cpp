// CreateCoder.cpp

#include "StdAfx.h"

#include "../../Windows/Defs.h"
#include "../../Windows/PropVariant.h"

#include "CreateCoder.h"

#include "FilterCoder.h"
#include "RegisterCodec.h"

static const unsigned kNumCodecsMax = 64;
unsigned g_NumCodecs = 0;
const CCodecInfo *g_Codecs[kNumCodecsMax];

// We use g_ExternalCodecs in other stages.
/*
#ifdef EXTERNAL_CODECS
extern CExternalCodecs g_ExternalCodecs;
#define CHECK_GLOBAL_CODECS \
    if (!__externalCodecs || !__externalCodecs->IsSet()) __externalCodecs = &g_ExternalCodecs;
#endif
*/

#define CHECK_GLOBAL_CODECS

void RegisterCodec(const CCodecInfo *codecInfo) throw()
{
  if (g_NumCodecs < kNumCodecsMax)
    g_Codecs[g_NumCodecs++] = codecInfo;
}

static const unsigned kNumHashersMax = 16;
unsigned g_NumHashers = 0;
const CHasherInfo *g_Hashers[kNumHashersMax];

void RegisterHasher(const CHasherInfo *hashInfo) throw()
{
  if (g_NumHashers < kNumHashersMax)
    g_Hashers[g_NumHashers++] = hashInfo;
}


#ifdef EXTERNAL_CODECS

static HRESULT ReadNumberOfStreams(ICompressCodecsInfo *codecsInfo, UInt32 index, PROPID propID, UInt32 &res)
{
  NWindows::NCOM::CPropVariant prop;
  RINOK(codecsInfo->GetProperty(index, propID, &prop));
  if (prop.vt == VT_EMPTY)
    res = 1;
  else if (prop.vt == VT_UI4)
    res = prop.ulVal;
  else
    return E_INVALIDARG;
  return S_OK;
}

static HRESULT ReadIsAssignedProp(ICompressCodecsInfo *codecsInfo, UInt32 index, PROPID propID, bool &res)
{
  NWindows::NCOM::CPropVariant prop;
  RINOK(codecsInfo->GetProperty(index, propID, &prop));
  if (prop.vt == VT_EMPTY)
    res = true;
  else if (prop.vt == VT_BOOL)
    res = VARIANT_BOOLToBool(prop.boolVal);
  else
    return E_INVALIDARG;
  return S_OK;
}

HRESULT CExternalCodecs::Load()
{
  Codecs.Clear();
  Hashers.Clear();

  if (GetCodecs)
  {
    CCodecInfoEx info;
    
    UString s;
    UInt32 num;
    RINOK(GetCodecs->GetNumMethods(&num));
    
    for (UInt32 i = 0; i < num; i++)
    {
      NWindows::NCOM::CPropVariant prop;
      
      RINOK(GetCodecs->GetProperty(i, NMethodPropID::kID, &prop));
      if (prop.vt != VT_UI8)
        continue; // old Interface
      info.Id = prop.uhVal.QuadPart;
      
      prop.Clear();
      
      info.Name.Empty();
      RINOK(GetCodecs->GetProperty(i, NMethodPropID::kName, &prop));
      if (prop.vt == VT_BSTR)
        info.Name.SetFromWStr_if_Ascii(prop.bstrVal);
      else if (prop.vt != VT_EMPTY)
        continue;
      
      RINOK(ReadNumberOfStreams(GetCodecs, i, NMethodPropID::kPackStreams, info.NumStreams));
      {
        UInt32 numUnpackStreams = 1;
        RINOK(ReadNumberOfStreams(GetCodecs, i, NMethodPropID::kUnpackStreams, numUnpackStreams));
        if (numUnpackStreams != 1)
          continue;
      }
      RINOK(ReadIsAssignedProp(GetCodecs, i, NMethodPropID::kEncoderIsAssigned, info.EncoderIsAssigned));
      RINOK(ReadIsAssignedProp(GetCodecs, i, NMethodPropID::kDecoderIsAssigned, info.DecoderIsAssigned));
      
      Codecs.Add(info);
    }
  }
  
  if (GetHashers)
  {
    UInt32 num = GetHashers->GetNumHashers();
    CHasherInfoEx info;
    
    for (UInt32 i = 0; i < num; i++)
    {
      NWindows::NCOM::CPropVariant prop;

      RINOK(GetHashers->GetHasherProp(i, NMethodPropID::kID, &prop));
      if (prop.vt != VT_UI8)
        continue;
      info.Id = prop.uhVal.QuadPart;
      
      prop.Clear();
      
      info.Name.Empty();
      RINOK(GetHashers->GetHasherProp(i, NMethodPropID::kName, &prop));
      if (prop.vt == VT_BSTR)
        info.Name.SetFromWStr_if_Ascii(prop.bstrVal);
      else if (prop.vt != VT_EMPTY)
        continue;
      
      Hashers.Add(info);
    }
  }
  
  return S_OK;
}

#endif


bool FindMethod(
    DECL_EXTERNAL_CODECS_LOC_VARS
    const AString &name,
    CMethodId &methodId, UInt32 &numStreams)
{
  unsigned i;
  for (i = 0; i < g_NumCodecs; i++)
  {
    const CCodecInfo &codec = *g_Codecs[i];
    if (StringsAreEqualNoCase_Ascii(name, codec.Name))
    {
      methodId = codec.Id;
      numStreams = codec.NumStreams;
      return true;
    }
  }
  
  #ifdef EXTERNAL_CODECS
  
  CHECK_GLOBAL_CODECS

  if (__externalCodecs)
    for (i = 0; i < __externalCodecs->Codecs.Size(); i++)
    {
      const CCodecInfoEx &codec = __externalCodecs->Codecs[i];
      if (StringsAreEqualNoCase_Ascii(name, codec.Name))
      {
        methodId = codec.Id;
        numStreams = codec.NumStreams;
        return true;
      }
    }
  
  #endif
  
  return false;
}

bool FindMethod(
    DECL_EXTERNAL_CODECS_LOC_VARS
    CMethodId methodId,
    AString &name)
{
  name.Empty();
 
  unsigned i;
  for (i = 0; i < g_NumCodecs; i++)
  {
    const CCodecInfo &codec = *g_Codecs[i];
    if (methodId == codec.Id)
    {
      name = codec.Name;
      return true;
    }
  }
  
  #ifdef EXTERNAL_CODECS

  CHECK_GLOBAL_CODECS

  if (__externalCodecs)
    for (i = 0; i < __externalCodecs->Codecs.Size(); i++)
    {
      const CCodecInfoEx &codec = __externalCodecs->Codecs[i];
      if (methodId == codec.Id)
      {
        name = codec.Name;
        return true;
      }
    }
  
  #endif
  
  return false;
}

bool FindHashMethod(
    DECL_EXTERNAL_CODECS_LOC_VARS
    const AString &name,
    CMethodId &methodId)
{
  unsigned i;
  for (i = 0; i < g_NumHashers; i++)
  {
    const CHasherInfo &codec = *g_Hashers[i];
    if (StringsAreEqualNoCase_Ascii(name, codec.Name))
    {
      methodId = codec.Id;
      return true;
    }
  }
  
  #ifdef EXTERNAL_CODECS

  CHECK_GLOBAL_CODECS

  if (__externalCodecs)
    for (i = 0; i < __externalCodecs->Hashers.Size(); i++)
    {
      const CHasherInfoEx &codec = __externalCodecs->Hashers[i];
      if (StringsAreEqualNoCase_Ascii(name, codec.Name))
      {
        methodId = codec.Id;
        return true;
      }
    }
  
  #endif
  
  return false;
}

void GetHashMethods(
    DECL_EXTERNAL_CODECS_LOC_VARS
    CRecordVector<CMethodId> &methods)
{
  methods.ClearAndSetSize(g_NumHashers);
  unsigned i;
  for (i = 0; i < g_NumHashers; i++)
    methods[i] = (*g_Hashers[i]).Id;
  
  #ifdef EXTERNAL_CODECS
  
  CHECK_GLOBAL_CODECS

  if (__externalCodecs)
    for (i = 0; i < __externalCodecs->Hashers.Size(); i++)
      methods.Add(__externalCodecs->Hashers[i].Id);
  
  #endif
}

HRESULT CreateCoder(
    DECL_EXTERNAL_CODECS_LOC_VARS
    CMethodId methodId, bool encode,
    CMyComPtr<ICompressFilter> &filter,
    CCreatedCoder &cod)
{
  cod.IsExternal = false;
  cod.IsFilter = false;
  cod.NumStreams = 1;

  unsigned i;
  for (i = 0; i < g_NumCodecs; i++)
  {
    const CCodecInfo &codec = *g_Codecs[i];
    if (codec.Id == methodId)
    {
      if (encode)
      {
        if (codec.CreateEncoder)
        {
          void *p = codec.CreateEncoder();
          if (codec.IsFilter) filter = (ICompressFilter *)p;
          else if (codec.NumStreams == 1) cod.Coder = (ICompressCoder *)p;
          else { cod.Coder2 = (ICompressCoder2 *)p; cod.NumStreams = codec.NumStreams; }
          return S_OK;
        }
      }
      else
        if (codec.CreateDecoder)
        {
          void *p = codec.CreateDecoder();
          if (codec.IsFilter) filter = (ICompressFilter *)p;
          else if (codec.NumStreams == 1) cod.Coder = (ICompressCoder *)p;
          else { cod.Coder2 = (ICompressCoder2 *)p; cod.NumStreams = codec.NumStreams; }
          return S_OK;
        }
    }
  }

  #ifdef EXTERNAL_CODECS

  CHECK_GLOBAL_CODECS
  
  if (__externalCodecs)
  {
    cod.IsExternal = true;
    for (i = 0; i < __externalCodecs->Codecs.Size(); i++)
    {
      const CCodecInfoEx &codec = __externalCodecs->Codecs[i];
      if (codec.Id == methodId)
      {
        if (encode)
        {
          if (codec.EncoderIsAssigned)
          {
            if (codec.NumStreams == 1)
            {
              HRESULT res = __externalCodecs->GetCodecs->CreateEncoder(i, &IID_ICompressCoder, (void **)&cod.Coder);
              if (res != S_OK && res != E_NOINTERFACE && res != CLASS_E_CLASSNOTAVAILABLE)
                return res;
              if (cod.Coder)
                return res;
              return __externalCodecs->GetCodecs->CreateEncoder(i, &IID_ICompressFilter, (void **)&filter);
            }
            cod.NumStreams = codec.NumStreams;
            return __externalCodecs->GetCodecs->CreateEncoder(i, &IID_ICompressCoder2, (void **)&cod.Coder2);
          }
        }
        else
          if (codec.DecoderIsAssigned)
          {
            if (codec.NumStreams == 1)
            {
              HRESULT res = __externalCodecs->GetCodecs->CreateDecoder(i, &IID_ICompressCoder, (void **)&cod.Coder);
              if (res != S_OK && res != E_NOINTERFACE && res != CLASS_E_CLASSNOTAVAILABLE)
                return res;
              if (cod.Coder)
                return res;
              return __externalCodecs->GetCodecs->CreateDecoder(i, &IID_ICompressFilter, (void **)&filter);
            }
            cod.NumStreams = codec.NumStreams;
            return __externalCodecs->GetCodecs->CreateDecoder(i, &IID_ICompressCoder2, (void **)&cod.Coder2);
          }
      }
    }
  }
  #endif

  return S_OK;
}

HRESULT CreateCoder(
    DECL_EXTERNAL_CODECS_LOC_VARS
    CMethodId methodId, bool encode,
    CCreatedCoder &cod)
{
  CMyComPtr<ICompressFilter> filter;
  HRESULT res = CreateCoder(
      EXTERNAL_CODECS_LOC_VARS
      methodId, encode,
      filter, cod);
  
  if (filter)
  {
    cod.IsFilter = true;
    CFilterCoder *coderSpec = new CFilterCoder(encode);
    cod.Coder = coderSpec;
    coderSpec->Filter = filter;
  }
  
  return res;
}

HRESULT CreateCoder(
    DECL_EXTERNAL_CODECS_LOC_VARS
    CMethodId methodId, bool encode,
    CMyComPtr<ICompressCoder> &coder)
{
  CCreatedCoder cod;
  HRESULT res = CreateCoder(
      EXTERNAL_CODECS_LOC_VARS
      methodId, encode,
      cod);
  coder = cod.Coder;
  return res;
}

HRESULT CreateFilter(
    DECL_EXTERNAL_CODECS_LOC_VARS
    CMethodId methodId, bool encode,
    CMyComPtr<ICompressFilter> &filter)
{
  CCreatedCoder cod;
  return CreateCoder(
      EXTERNAL_CODECS_LOC_VARS
      methodId, encode,
      filter, cod);
}


HRESULT CreateHasher(
    DECL_EXTERNAL_CODECS_LOC_VARS
    CMethodId methodId,
    AString &name,
    CMyComPtr<IHasher> &hasher)
{
  name.Empty();

  unsigned i;
  for (i = 0; i < g_NumHashers; i++)
  {
    const CHasherInfo &codec = *g_Hashers[i];
    if (codec.Id == methodId)
    {
      hasher = codec.CreateHasher();
      name = codec.Name;
      break;
    }
  }

  #ifdef EXTERNAL_CODECS

  CHECK_GLOBAL_CODECS

  if (!hasher && __externalCodecs)
    for (i = 0; i < __externalCodecs->Hashers.Size(); i++)
    {
      const CHasherInfoEx &codec = __externalCodecs->Hashers[i];
      if (codec.Id == methodId)
      {
        name = codec.Name;
        return __externalCodecs->GetHashers->CreateHasher((UInt32)i, &hasher);
      }
    }

  #endif

  return S_OK;
}
