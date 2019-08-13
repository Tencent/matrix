// FilePathAutoRename.cpp

#include "StdAfx.h"

#include "../../Common/Defs.h"
#include "../../Common/IntToString.h"

#include "../../Windows/FileFind.h"

#include "FilePathAutoRename.h"

using namespace NWindows;

static bool MakeAutoName(const FString &name,
    const FString &extension, UInt32 value, FString &path)
{
  char temp[16];
  ConvertUInt32ToString(value, temp);
  path = name;
  path.AddAscii(temp);
  path += extension;
  return NFile::NFind::DoesFileOrDirExist(path);
}

bool AutoRenamePath(FString &path)
{
  int dotPos = path.ReverseFind_Dot();
  int slashPos = path.ReverseFind_PathSepar();

  FString name = path;
  FString extension;
  if (dotPos > slashPos + 1)
  {
    name.DeleteFrom(dotPos);
    extension = path.Ptr(dotPos);
  }
  name += FTEXT('_');
  
  FString temp;

  UInt32 left = 1, right = ((UInt32)1 << 30);
  while (left != right)
  {
    UInt32 mid = (left + right) / 2;
    if (MakeAutoName(name, extension, mid, temp))
      left = mid + 1;
    else
      right = mid;
  }
  return !MakeAutoName(name, extension, right, path);
}
