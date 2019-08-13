// ArchiveName.cpp

#include "StdAfx.h"

#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileName.h"

#include "ExtractingFilePath.h"
#include "ArchiveName.h"

using namespace NWindows;
using namespace NFile;

UString CreateArchiveName(const NFind::CFileInfo &fi, bool keepName)
{
  FString resultName = fi.Name;
  if (!fi.IsDir() && !keepName)
  {
    int dotPos = resultName.ReverseFind_Dot();
    if (dotPos > 0)
    {
      FString archiveName2 = resultName.Left(dotPos);
      if (archiveName2.ReverseFind_Dot() < 0)
        resultName = archiveName2;
    }
  }
  return Get_Correct_FsFile_Name(fs2us(resultName));
}

static FString CreateArchiveName2(const FString &path, bool fromPrev, bool keepName)
{
  FString resultName = FTEXT("Archive");
  if (fromPrev)
  {
    FString dirPrefix;
    if (NDir::GetOnlyDirPrefix(path, dirPrefix))
    {
      if (!dirPrefix.IsEmpty() && IsPathSepar(dirPrefix.Back()))
      {
        #if defined(_WIN32) && !defined(UNDER_CE)
        if (NName::IsDriveRootPath_SuperAllowed(dirPrefix))
          resultName = dirPrefix[dirPrefix.Len() - 3]; // only letter
        else
        #endif
        {
          dirPrefix.DeleteBack();
          NFind::CFileInfo fi;
          if (fi.Find(dirPrefix))
            resultName = fi.Name;
        }
      }
    }
  }
  else
  {
    NFind::CFileInfo fi;
    if (fi.Find(path))
    {
      resultName = fi.Name;
      if (!fi.IsDir() && !keepName)
      {
        int dotPos = resultName.ReverseFind_Dot();
        if (dotPos > 0)
        {
          FString name2 = resultName.Left(dotPos);
          if (name2.ReverseFind_Dot() < 0)
            resultName = name2;
        }
      }
    }
  }
  return resultName;
}

UString CreateArchiveName(const UString &path, bool fromPrev, bool keepName)
{
  return Get_Correct_FsFile_Name(fs2us(CreateArchiveName2(us2fs(path), fromPrev, keepName)));
}
