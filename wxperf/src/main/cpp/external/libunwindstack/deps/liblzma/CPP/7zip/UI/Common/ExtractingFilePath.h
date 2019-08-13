// ExtractingFilePath.h

#ifndef __EXTRACTING_FILE_PATH_H
#define __EXTRACTING_FILE_PATH_H

#include "../../../Common/MyString.h"

#ifdef _WIN32
void Correct_AltStream_Name(UString &s);
#endif

// replaces unsuported characters, and replaces "." , ".." and "" to "[]"
UString Get_Correct_FsFile_Name(const UString &name);

void Correct_FsPath(bool absIsAllowed, UStringVector &parts, bool isDir);

UString MakePathFromParts(const UStringVector &parts);

#endif
