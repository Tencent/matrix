// DLLExports2.cpp

#include "StdAfx.h"

#include "../../Common/MyWindows.h"

#include "../../Common/MyInitGuid.h"

#if defined(_7ZIP_LARGE_PAGES)
#include "../../../C/Alloc.h"
#endif

#include "../../Common/ComTry.h"

#include "../../Windows/NtCheck.h"
#include "../../Windows/PropVariant.h"

#include "../ICoder.h"
#include "../IPassword.h"

#include "../Common/CreateCoder.h"

#include "IArchive.h"

HINSTANCE g_hInstance;

#define NT_CHECK_FAIL_ACTION return FALSE;

#ifdef _WIN32
extern "C"
BOOL WINAPI DllMain(
  #ifdef UNDER_CE
  HANDLE
  #else
  HINSTANCE
  #endif
  hInstance, DWORD dwReason, LPVOID /*lpReserved*/)
{
  if (dwReason == DLL_PROCESS_ATTACH)
  {
    // OutputDebugStringA("7z.dll DLL_PROCESS_ATTACH");
    g_hInstance = (HINSTANCE)hInstance;
    NT_CHECK;
  }
  /*
  if (dwReason == DLL_PROCESS_DETACH)
  {
    OutputDebugStringA("7z.dll DLL_PROCESS_DETACH");
  }
  */
  return TRUE;
}
#endif

DEFINE_GUID(CLSID_CArchiveHandler,
    k_7zip_GUID_Data1,
    k_7zip_GUID_Data2,
    k_7zip_GUID_Data3_Common,
    0x10, 0x00, 0x00, 0x01, 0x10, 0x00, 0x00, 0x00);

STDAPI CreateCoder(const GUID *clsid, const GUID *iid, void **outObject);
STDAPI CreateHasher(const GUID *clsid, IHasher **hasher);
STDAPI CreateArchiver(const GUID *clsid, const GUID *iid, void **outObject);

STDAPI CreateObject(const GUID *clsid, const GUID *iid, void **outObject)
{
  // COM_TRY_BEGIN
  *outObject = 0;
  if (*iid == IID_ICompressCoder ||
      *iid == IID_ICompressCoder2 ||
      *iid == IID_ICompressFilter)
    return CreateCoder(clsid, iid, outObject);
  if (*iid == IID_IHasher)
    return CreateHasher(clsid, (IHasher **)outObject);
  return CreateArchiver(clsid, iid, outObject);
  // COM_TRY_END
}

STDAPI SetLargePageMode()
{
  #if defined(_7ZIP_LARGE_PAGES)
  SetLargePageSize();
  #endif
  return S_OK;
}

extern bool g_CaseSensitive;

STDAPI SetCaseSensitive(Int32 caseSensitive)
{
  g_CaseSensitive = (caseSensitive != 0);
  return S_OK;
}

#ifdef EXTERNAL_CODECS

CExternalCodecs g_ExternalCodecs;

STDAPI SetCodecs(ICompressCodecsInfo *compressCodecsInfo)
{
  COM_TRY_BEGIN

  // OutputDebugStringA(compressCodecsInfo ? "SetCodecs" : "SetCodecs NULL");
  if (compressCodecsInfo)
  {
    g_ExternalCodecs.GetCodecs = compressCodecsInfo;
    return g_ExternalCodecs.Load();
  }
  g_ExternalCodecs.ClearAndRelease();
  return S_OK;

  COM_TRY_END
}

#else

STDAPI SetCodecs(ICompressCodecsInfo *)
{
  return S_OK;
}

#endif
