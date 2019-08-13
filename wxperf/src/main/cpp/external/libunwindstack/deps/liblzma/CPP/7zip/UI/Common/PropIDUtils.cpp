// PropIDUtils.cpp

#include "StdAfx.h"

#include "../../../../C/CpuArch.h"

#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/FileIO.h"
#include "../../../Windows/PropVariantConv.h"

#include "../../PropID.h"

#include "PropIDUtils.h"

#define Get16(x) GetUi16(x)
#define Get32(x) GetUi32(x)

using namespace NWindows;

static const char g_WinAttribChars[16 + 1] = "RHS8DAdNTsLCOnE_";
/*
0 READONLY
1 HIDDEN
2 SYSTEM

4 DIRECTORY
5 ARCHIVE
6 DEVICE
7 NORMAL
8 TEMPORARY
9 SPARSE_FILE
10 REPARSE_POINT
11 COMPRESSED
12 OFFLINE
13 NOT_CONTENT_INDEXED
14 ENCRYPTED

16 VIRTUAL
*/

static const char kPosixTypes[16] = { '0', 'p', 'c', '3', 'd', '5', 'b', '7', '-', '9', 'l', 'B', 's', 'D', 'E', 'F' };
#define MY_ATTR_CHAR(a, n, c) ((a) & (1 << (n))) ? c : '-';

static void ConvertPosixAttribToString(char *s, UInt32 a) throw()
{
  s[0] = kPosixTypes[(a >> 12) & 0xF];
  for (int i = 6; i >= 0; i -= 3)
  {
    s[7 - i] = MY_ATTR_CHAR(a, i + 2, 'r');
    s[8 - i] = MY_ATTR_CHAR(a, i + 1, 'w');
    s[9 - i] = MY_ATTR_CHAR(a, i + 0, 'x');
  }
  if ((a & 0x800) != 0) s[3] = ((a & (1 << 6)) ? 's' : 'S');
  if ((a & 0x400) != 0) s[6] = ((a & (1 << 3)) ? 's' : 'S');
  if ((a & 0x200) != 0) s[9] = ((a & (1 << 0)) ? 't' : 'T');
  s[10] = 0;
  
  a &= ~(UInt32)0xFFFF;
  if (a != 0)
  {
    s[10] = ' ';
    ConvertUInt32ToHex8Digits(a, s + 11);
  }
}

void ConvertWinAttribToString(char *s, UInt32 wa) throw()
{
  for (int i = 0; i < 16; i++)
    if ((wa & (1 << i)) && i != 7)
      *s++ = g_WinAttribChars[i];
  *s = 0;

  // we support p7zip trick that stores posix attributes in high 16 bits, and 0x8000 flag
  // we also support ZIP archives created in Unix, that store posix attributes in high 16 bits without 0x8000 flag
  
  // if (wa & 0x8000)
  if ((wa >> 16) != 0)
  {
    *s++ = ' ';
    ConvertPosixAttribToString(s, wa >> 16);
  }
}

void ConvertPropertyToShortString(char *dest, const PROPVARIANT &prop, PROPID propID, bool full) throw()
{
  *dest = 0;
  
  if (prop.vt == VT_FILETIME)
  {
    FILETIME localFileTime;
    if ((prop.filetime.dwHighDateTime == 0 &&
        prop.filetime.dwLowDateTime == 0) ||
        !::FileTimeToLocalFileTime(&prop.filetime, &localFileTime))
      return;
    ConvertFileTimeToString(localFileTime, dest, true, full);
    return;
  }

  switch (propID)
  {
    case kpidCRC:
    {
      if (prop.vt != VT_UI4)
        break;
      ConvertUInt32ToHex8Digits(prop.ulVal, dest);
      return;
    }
    case kpidAttrib:
    {
      if (prop.vt != VT_UI4)
        break;
      UInt32 a = prop.ulVal;

      /*
      if ((a & 0x8000) && (a & 0x7FFF) == 0)
        ConvertPosixAttribToString(dest, a >> 16);
      else
      */
      ConvertWinAttribToString(dest, a);
      return;
    }
    case kpidPosixAttrib:
    {
      if (prop.vt != VT_UI4)
        break;
      ConvertPosixAttribToString(dest, prop.ulVal);
      return;
    }
    case kpidINode:
    {
      if (prop.vt != VT_UI8)
        break;
      ConvertUInt32ToString((UInt32)(prop.uhVal.QuadPart >> 48), dest);
      dest += strlen(dest);
      *dest++ = '-';
      UInt64 low = prop.uhVal.QuadPart & (((UInt64)1 << 48) - 1);
      ConvertUInt64ToString(low, dest);
      return;
    }
    case kpidVa:
    {
      UInt64 v = 0;
      if (prop.vt == VT_UI4)
        v = prop.ulVal;
      else if (prop.vt == VT_UI8)
        v = (UInt64)prop.uhVal.QuadPart;
      else
        break;
      dest[0] = '0';
      dest[1] = 'x';
      ConvertUInt64ToHex(v, dest + 2);
      return;
    }
  }
  
  ConvertPropVariantToShortString(prop, dest);
}

void ConvertPropertyToString(UString &dest, const PROPVARIANT &prop, PROPID propID, bool full)
{
  if (prop.vt == VT_BSTR)
  {
    dest.SetFromBstr(prop.bstrVal);
    return;
  }
  char temp[64];
  ConvertPropertyToShortString(temp, prop, propID, full);
  dest.SetFromAscii(temp);
}

static inline unsigned GetHex(unsigned v)
{
  return (v < 10) ? ('0' + v) : ('A' + (v - 10));
}

#ifndef _SFX

static inline void AddHexToString(AString &res, unsigned v)
{
  res += (char)GetHex(v >> 4);
  res += (char)GetHex(v & 0xF);
  res += ' ';
}

/*
static AString Data_To_Hex(const Byte *data, size_t size)
{
  AString s;
  for (size_t i = 0; i < size; i++)
    AddHexToString(s, data[i]);
  return s;
}
*/

static const char * const sidNames[] =
{
    "0"
  , "Dialup"
  , "Network"
  , "Batch"
  , "Interactive"
  , "Logon"  // S-1-5-5-X-Y
  , "Service"
  , "Anonymous"
  , "Proxy"
  , "EnterpriseDC"
  , "Self"
  , "AuthenticatedUsers"
  , "RestrictedCode"
  , "TerminalServer"
  , "RemoteInteractiveLogon"
  , "ThisOrganization"
  , "16"
  , "IUserIIS"
  , "LocalSystem"
  , "LocalService"
  , "NetworkService"
  , "Domains"
};

struct CSecID2Name
{
  UInt32 n;
  const char *sz;
};

static const CSecID2Name sid_32_Names[] =
{
  { 544, "Administrators" },
  { 545, "Users" },
  { 546, "Guests" },
  { 547, "PowerUsers" },
  { 548, "AccountOperators" },
  { 549, "ServerOperators" },
  { 550, "PrintOperators" },
  { 551, "BackupOperators" },
  { 552, "Replicators" },
  { 553, "Backup Operators" },
  { 554, "PreWindows2000CompatibleAccess" },
  { 555, "RemoteDesktopUsers" },
  { 556, "NetworkConfigurationOperators" },
  { 557, "IncomingForestTrustBuilders" },
  { 558, "PerformanceMonitorUsers" },
  { 559, "PerformanceLogUsers" },
  { 560, "WindowsAuthorizationAccessGroup" },
  { 561, "TerminalServerLicenseServers" },
  { 562, "DistributedCOMUsers" },
  { 569, "CryptographicOperators" },
  { 573, "EventLogReaders" },
  { 574, "CertificateServiceDCOMAccess" }
};

static const CSecID2Name sid_21_Names[] =
{
  { 500, "Administrator" },
  { 501, "Guest" },
  { 502, "KRBTGT" },
  { 512, "DomainAdmins" },
  { 513, "DomainUsers" },
  { 515, "DomainComputers" },
  { 516, "DomainControllers" },
  { 517, "CertPublishers" },
  { 518, "SchemaAdmins" },
  { 519, "EnterpriseAdmins" },
  { 520, "GroupPolicyCreatorOwners" },
  { 553, "RASandIASServers" },
  { 553, "RASandIASServers" },
  { 571, "AllowedRODCPasswordReplicationGroup" },
  { 572, "DeniedRODCPasswordReplicationGroup" }
};

struct CServicesToName
{
  UInt32 n[5];
  const char *sz;
};

static const CServicesToName services_to_name[] =
{
  { { 0x38FB89B5, 0xCBC28419, 0x6D236C5C, 0x6E770057, 0x876402C0 } , "TrustedInstaller" }
};

static void ParseSid(AString &s, const Byte *p, UInt32 lim, UInt32 &sidSize)
{
  sidSize = 0;
  if (lim < 8)
  {
    s += "ERROR";
    return;
  }
  UInt32 rev = p[0];
  if (rev != 1)
  {
    s += "UNSUPPORTED";
    return;
  }
  UInt32 num = p[1];
  if (8 + num * 4 > lim)
  {
    s += "ERROR";
    return;
  }
  sidSize = 8 + num * 4;
  UInt32 authority = GetBe32(p + 4);

  if (p[2] == 0 && p[3] == 0 && authority == 5 && num >= 1)
  {
    UInt32 v0 = Get32(p + 8);
    if (v0 < ARRAY_SIZE(sidNames))
    {
      s += sidNames[v0];
      return;
    }
    if (v0 == 32 && num == 2)
    {
      UInt32 v1 = Get32(p + 12);
      for (unsigned i = 0; i < ARRAY_SIZE(sid_32_Names); i++)
        if (sid_32_Names[i].n == v1)
        {
          s += sid_32_Names[i].sz;
          return;
        }
    }
    if (v0 == 21 && num == 5)
    {
      UInt32 v4 = Get32(p + 8 + 4 * 4);
      for (unsigned i = 0; i < ARRAY_SIZE(sid_21_Names); i++)
        if (sid_21_Names[i].n == v4)
        {
          s += sid_21_Names[i].sz;
          return;
        }
    }
    if (v0 == 80 && num == 6)
    {
      for (unsigned i = 0; i < ARRAY_SIZE(services_to_name); i++)
      {
        const CServicesToName &sn = services_to_name[i];
        int j;
        for (j = 0; j < 5 && sn.n[j] == Get32(p + 8 + 4 + j * 4); j++);
        if (j == 5)
        {
          s += sn.sz;
          return;
        }
      }
    }
  }
  
  char sz[16];
  s += "S-1-";
  if (p[2] == 0 && p[3] == 0)
  {
    ConvertUInt32ToString(authority, sz);
    s += sz;
  }
  else
  {
    s += "0x";
    for (int i = 2; i < 8; i++)
      AddHexToString(s, p[i]);
  }
  for (UInt32 i = 0; i < num; i++)
  {
    s += '-';
    ConvertUInt32ToString(Get32(p + 8 + i * 4), sz);
    s += sz;
  }
}

static void ParseOwner(AString &s, const Byte *p, UInt32 size, UInt32 pos)
{
  if (pos > size)
  {
    s += "ERROR";
    return;
  }
  UInt32 sidSize = 0;
  ParseSid(s, p + pos, size - pos, sidSize);
}

static void AddUInt32ToString(AString &s, UInt32 val)
{
  char sz[16];
  ConvertUInt32ToString(val, sz);
  s += sz;
}

static void ParseAcl(AString &s, const Byte *p, UInt32 size, const char *strName, UInt32 flags, UInt32 offset)
{
  UInt32 control = Get16(p + 2);
  if ((flags & control) == 0)
    return;
  UInt32 pos = Get32(p + offset);
  s += ' ';
  s += strName;
  if (pos >= size)
    return;
  p += pos;
  size -= pos;
  if (size < 8)
    return;
  if (Get16(p) != 2) // revision
    return;
  UInt32 num = Get32(p + 4);
  AddUInt32ToString(s, num);
  
  /*
  UInt32 aclSize = Get16(p + 2);
  if (num >= (1 << 16))
    return;
  if (aclSize > size)
    return;
  size = aclSize;
  size -= 8;
  p += 8;
  for (UInt32 i = 0 ; i < num; i++)
  {
    if (size <= 8)
      return;
    // Byte type = p[0];
    // Byte flags = p[1];
    // UInt32 aceSize = Get16(p + 2);
    // UInt32 mask = Get32(p + 4);
    p += 8;
    size -= 8;

    UInt32 sidSize = 0;
    s += ' ';
    ParseSid(s, p, size, sidSize);
    if (sidSize == 0)
      return;
    p += sidSize;
    size -= sidSize;
  }

  // the tail can contain zeros. So (size != 0) is not ERROR
  // if (size != 0) s += " ERROR";
  */
}

#define MY_SE_OWNER_DEFAULTED       (0x0001)
#define MY_SE_GROUP_DEFAULTED       (0x0002)
#define MY_SE_DACL_PRESENT          (0x0004)
#define MY_SE_DACL_DEFAULTED        (0x0008)
#define MY_SE_SACL_PRESENT          (0x0010)
#define MY_SE_SACL_DEFAULTED        (0x0020)
#define MY_SE_DACL_AUTO_INHERIT_REQ (0x0100)
#define MY_SE_SACL_AUTO_INHERIT_REQ (0x0200)
#define MY_SE_DACL_AUTO_INHERITED   (0x0400)
#define MY_SE_SACL_AUTO_INHERITED   (0x0800)
#define MY_SE_DACL_PROTECTED        (0x1000)
#define MY_SE_SACL_PROTECTED        (0x2000)
#define MY_SE_RM_CONTROL_VALID      (0x4000)
#define MY_SE_SELF_RELATIVE         (0x8000)

void ConvertNtSecureToString(const Byte *data, UInt32 size, AString &s)
{
  s.Empty();
  if (size < 20 || size > (1 << 18))
  {
    s += "ERROR";
    return;
  }
  if (Get16(data) != 1) // revision
  {
    s += "UNSUPPORTED";
    return;
  }
  ParseOwner(s, data, size, Get32(data + 4));
  s += ' ';
  ParseOwner(s, data, size, Get32(data + 8));
  ParseAcl(s, data, size, "s:", MY_SE_SACL_PRESENT, 12);
  ParseAcl(s, data, size, "d:", MY_SE_DACL_PRESENT, 16);
  s += ' ';
  AddUInt32ToString(s, size);
  // s += '\n';
  // s += Data_To_Hex(data, size);
}

#ifdef _WIN32

static bool CheckSid(const Byte *data, UInt32 size, UInt32 pos) throw()
{
  if (pos >= size)
    return false;
  size -= pos;
  if (size < 8)
    return false;
  UInt32 rev = data[pos];
  if (rev != 1)
    return false;
  UInt32 num = data[pos + 1];
  return (8 + num * 4 <= size);
}

static bool CheckAcl(const Byte *p, UInt32 size, UInt32 flags, UInt32 offset) throw()
{
  UInt32 control = Get16(p + 2);
  if ((flags & control) == 0)
    return true;
  UInt32 pos = Get32(p + offset);
  if (pos >= size)
    return false;
  p += pos;
  size -= pos;
  if (size < 8)
    return false;
  UInt32 aclSize = Get16(p + 2);
  return (aclSize <= size);
}

bool CheckNtSecure(const Byte *data, UInt32 size) throw()
{
  if (size < 20)
    return false;
  if (Get16(data) != 1) // revision
    return true; // windows function can handle such error, so we allow it
  if (size > (1 << 18))
    return false;
  if (!CheckSid(data, size, Get32(data + 4))) return false;
  if (!CheckSid(data, size, Get32(data + 8))) return false;
  if (!CheckAcl(data, size, MY_SE_SACL_PRESENT, 12)) return false;
  if (!CheckAcl(data, size, MY_SE_DACL_PRESENT, 16)) return false;
  return true;
}

#endif

bool ConvertNtReparseToString(const Byte *data, UInt32 size, UString &s)
{
  s.Empty();
  NFile::CReparseAttr attr;
  if (attr.Parse(data, size))
  {
    if (!attr.IsSymLink())
      s.AddAscii("Junction: ");
    s += attr.GetPath();
    if (!attr.IsOkNamePair())
    {
      s.AddAscii(" : ");
      s += attr.PrintName;
    }
    return true;
  }

  if (size < 8)
    return false;
  UInt32 tag = Get32(data);
  UInt32 len = Get16(data + 4);
  if (len + 8 > size)
    return false;
  if (Get16(data + 6) != 0) // padding
    return false;

  char hex[16];
  ConvertUInt32ToHex8Digits(tag, hex);
  s.AddAscii(hex);
  s.Add_Space();

  data += 8;

  for (UInt32 i = 0; i < len; i++)
  {
    unsigned b = ((const Byte *)data)[i];
    s += (wchar_t)GetHex((b >> 4) & 0xF);
    s += (wchar_t)GetHex(b & 0xF);
  }
  return true;
}

#endif
