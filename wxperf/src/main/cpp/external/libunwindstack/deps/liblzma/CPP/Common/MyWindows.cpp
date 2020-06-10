// MyWindows.cpp

#include "StdAfx.h"

#ifndef _WIN32

#include <stdlib.h>

#include "MyWindows.h"

static inline void *AllocateForBSTR(size_t cb) { return ::malloc(cb); }
static inline void FreeForBSTR(void *pv) { ::free(pv);}

/* Win32 uses DWORD (32-bit) type to store size of string before (OLECHAR *) string.
  We must select CBstrSizeType for another systems (not Win32):

    if (CBstrSizeType is UINT32),
          then we support only strings smaller than 4 GB.
          Win32 version always has that limitation.
  
    if (CBstrSizeType is UINT),
          (UINT can be 16/32/64-bit)
          We can support strings larger than 4 GB (if UINT is 64-bit),
          but sizeof(UINT) can be different in parts compiled by
          different compilers/settings,
          and we can't send such BSTR strings between such parts.
*/

typedef UINT32 CBstrSizeType;
// typedef UINT CBstrSizeType;

#define k_BstrSize_Max 0xFFFFFFFF
// #define k_BstrSize_Max UINT_MAX
// #define k_BstrSize_Max ((UINT)(INT)-1)

BSTR SysAllocStringByteLen(LPCSTR s, UINT len)
{
  /* Original SysAllocStringByteLen in Win32 maybe fills only unaligned null OLECHAR at the end.
     We provide also aligned null OLECHAR at the end. */

  if (len >= (k_BstrSize_Max - sizeof(OLECHAR) - sizeof(OLECHAR) - sizeof(CBstrSizeType)))
    return NULL;

  UINT size = (len + sizeof(OLECHAR) + sizeof(OLECHAR) - 1) & ~(sizeof(OLECHAR) - 1);
  void *p = AllocateForBSTR(size + sizeof(CBstrSizeType));
  if (!p)
    return NULL;
  *(CBstrSizeType *)p = (CBstrSizeType)len;
  BSTR bstr = (BSTR)((CBstrSizeType *)p + 1);
  if (s)
    memcpy(bstr, s, len);
  for (; len < size; len++)
    ((Byte *)bstr)[len] = 0;
  return bstr;
}

BSTR SysAllocStringLen(const OLECHAR *s, UINT len)
{
  if (len >= (k_BstrSize_Max - sizeof(OLECHAR) - sizeof(CBstrSizeType)) / sizeof(OLECHAR))
    return NULL;

  UINT size = len * sizeof(OLECHAR);
  void *p = AllocateForBSTR(size + sizeof(CBstrSizeType) + sizeof(OLECHAR));
  if (!p)
    return NULL;
  *(CBstrSizeType *)p = (CBstrSizeType)size;
  BSTR bstr = (BSTR)((CBstrSizeType *)p + 1);
  if (s)
    memcpy(bstr, s, size);
  bstr[len] = 0;
  return bstr;
}

BSTR SysAllocString(const OLECHAR *s)
{
  if (!s)
    return 0;
  const OLECHAR *s2 = s;
  while (*s2 != 0)
    s2++;
  return SysAllocStringLen(s, (UINT)(s2 - s));
}

void SysFreeString(BSTR bstr)
{
  if (bstr)
    FreeForBSTR((CBstrSizeType *)bstr - 1);
}

UINT SysStringByteLen(BSTR bstr)
{
  if (!bstr)
    return 0;
  return *((CBstrSizeType *)bstr - 1);
}

UINT SysStringLen(BSTR bstr)
{
  if (!bstr)
    return 0;
  return *((CBstrSizeType *)bstr - 1) / sizeof(OLECHAR);
}


HRESULT VariantClear(VARIANTARG *prop)
{
  if (prop->vt == VT_BSTR)
    SysFreeString(prop->bstrVal);
  prop->vt = VT_EMPTY;
  return S_OK;
}

HRESULT VariantCopy(VARIANTARG *dest, const VARIANTARG *src)
{
  HRESULT res = ::VariantClear(dest);
  if (res != S_OK)
    return res;
  if (src->vt == VT_BSTR)
  {
    dest->bstrVal = SysAllocStringByteLen((LPCSTR)src->bstrVal,
        SysStringByteLen(src->bstrVal));
    if (!dest->bstrVal)
      return E_OUTOFMEMORY;
    dest->vt = VT_BSTR;
  }
  else
    *dest = *src;
  return S_OK;
}

LONG CompareFileTime(const FILETIME* ft1, const FILETIME* ft2)
{
  if (ft1->dwHighDateTime < ft2->dwHighDateTime) return -1;
  if (ft1->dwHighDateTime > ft2->dwHighDateTime) return 1;
  if (ft1->dwLowDateTime < ft2->dwLowDateTime) return -1;
  if (ft1->dwLowDateTime > ft2->dwLowDateTime) return 1;
  return 0;
}

DWORD GetLastError()
{
  return 0;
}

#endif
