// Common/Wildcard.h

#ifndef __COMMON_WILDCARD_H
#define __COMMON_WILDCARD_H

#include "MyString.h"

int CompareFileNames(const wchar_t *s1, const wchar_t *s2) STRING_UNICODE_THROW;
#ifndef USE_UNICODE_FSTRING
  int CompareFileNames(const char *s1, const char *s2);
#endif

bool IsPath1PrefixedByPath2(const wchar_t *s1, const wchar_t *s2);

void SplitPathToParts(const UString &path, UStringVector &pathParts);
void SplitPathToParts_2(const UString &path, UString &dirPrefix, UString &name);
void SplitPathToParts_Smart(const UString &path, UString &dirPrefix, UString &name); // ignores dir delimiter at the end of (path)

UString ExtractDirPrefixFromPath(const UString &path);
UString ExtractFileNameFromPath(const UString &path);

bool DoesNameContainWildcard(const UString &path);
bool DoesWildcardMatchName(const UString &mask, const UString &name);

namespace NWildcard {

#ifdef _WIN32
// returns true, if name is like "a:", "c:", ...
bool IsDriveColonName(const wchar_t *s);
unsigned GetNumPrefixParts_if_DrivePath(UStringVector &pathParts);
#endif

struct CItem
{
  UStringVector PathParts;
  bool Recursive;
  bool ForFile;
  bool ForDir;
  bool WildcardMatching;
  
  #ifdef _WIN32
  bool IsDriveItem() const
  {
    return PathParts.Size() == 1 && !ForFile && ForDir && IsDriveColonName(PathParts[0]);
  }
  #endif

  // CItem(): WildcardMatching(true) {}

  bool AreAllAllowed() const;
  bool CheckPath(const UStringVector &pathParts, bool isFile) const;
};

class CCensorNode
{
  CCensorNode *Parent;
  
  bool CheckPathCurrent(bool include, const UStringVector &pathParts, bool isFile) const;
  void AddItemSimple(bool include, CItem &item);
public:
  bool CheckPathVect(const UStringVector &pathParts, bool isFile, bool &include) const;

  CCensorNode(): Parent(0) { };
  CCensorNode(const UString &name, CCensorNode *parent): Name(name), Parent(parent) { };

  UString Name; // WIN32 doesn't support wildcards in file names
  CObjectVector<CCensorNode> SubNodes;
  CObjectVector<CItem> IncludeItems;
  CObjectVector<CItem> ExcludeItems;

  bool AreAllAllowed() const;

  int FindSubNode(const UString &path) const;

  void AddItem(bool include, CItem &item, int ignoreWildcardIndex = -1);
  void AddItem(bool include, const UString &path, bool recursive, bool forFile, bool forDir, bool wildcardMatching);
  void AddItem2(bool include, const UString &path, bool recursive, bool wildcardMatching);

  bool NeedCheckSubDirs() const;
  bool AreThereIncludeItems() const;

  // bool CheckPath2(bool isAltStream, const UString &path, bool isFile, bool &include) const;
  // bool CheckPath(bool isAltStream, const UString &path, bool isFile) const;

  bool CheckPathToRoot(bool include, UStringVector &pathParts, bool isFile) const;
  // bool CheckPathToRoot(const UString &path, bool isFile, bool include) const;
  void ExtendExclude(const CCensorNode &fromNodes);
};

struct CPair
{
  UString Prefix;
  CCensorNode Head;
  
  CPair(const UString &prefix): Prefix(prefix) { };
};

enum ECensorPathMode
{
  k_RelatPath,  // absolute prefix as Prefix, remain path in Tree
  k_FullPath,   // drive prefix as Prefix, remain path in Tree
  k_AbsPath     // full path in Tree
};

struct CCensorPath
{
  UString Path;
  bool Include;
  bool Recursive;
  bool WildcardMatching;

  CCensorPath():
    Include(true),
    Recursive(false),
    WildcardMatching(true)
    {}
};

class CCensor
{
  int FindPrefix(const UString &prefix) const;
public:
  CObjectVector<CPair> Pairs;

  CObjectVector<NWildcard::CCensorPath> CensorPaths;
  
  bool AllAreRelative() const
    { return (Pairs.Size() == 1 && Pairs.Front().Prefix.IsEmpty()); }
  
  void AddItem(ECensorPathMode pathMode, bool include, const UString &path, bool recursive, bool wildcardMatching);
  // bool CheckPath(bool isAltStream, const UString &path, bool isFile) const;
  void ExtendExclude();

  void AddPathsToCensor(NWildcard::ECensorPathMode censorPathMode);
  void AddPreItem(bool include, const UString &path, bool recursive, bool wildcardMatching);
  void AddPreItem(const UString &path)
  {
    AddPreItem(true, path, false, false);
  }
  void AddPreItem_Wildcard()
  {
    AddPreItem(true, UString("*"), false, true);
  }
};


}

#endif
