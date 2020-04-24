// EnumDirItems.h

#ifndef __ENUM_DIR_ITEMS_H
#define __ENUM_DIR_ITEMS_H

#include "../../../Common/Wildcard.h"

#include "../../../Windows/FileFind.h"

#include "DirItem.h"

void AddDirFileInfo(int phyParent, int logParent, int secureIndex,
    const NWindows::NFile::NFind::CFileInfo &fi, CObjectVector<CDirItem> &dirItems);

HRESULT EnumerateItems(
    const NWildcard::CCensor &censor,
    NWildcard::ECensorPathMode pathMode,
    const UString &addPathPrefix,
    CDirItems &dirItems);


struct CMessagePathException: public UString
{
  CMessagePathException(const char *a, const wchar_t *u = NULL);
  CMessagePathException(const wchar_t *a, const wchar_t *u = NULL);
};


HRESULT EnumerateDirItemsAndSort(
    NWildcard::CCensor &censor,
    NWildcard::ECensorPathMode pathMode,
    const UString &addPathPrefix,
    UStringVector &sortedPaths,
    UStringVector &sortedFullPaths,
    CDirItemsStat &st,
    IDirItemsCallback *callback);

#ifdef _WIN32
void ConvertToLongNames(NWildcard::CCensor &censor);
#endif

#endif
