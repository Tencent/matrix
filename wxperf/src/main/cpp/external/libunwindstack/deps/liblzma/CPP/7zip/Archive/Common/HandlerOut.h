// HandlerOut.h

#ifndef __HANDLER_OUT_H
#define __HANDLER_OUT_H

#include "../../Common/MethodProps.h"

namespace NArchive {

class CMultiMethodProps
{
  UInt32 _level;
  int _analysisLevel;
public:
  #ifndef _7ZIP_ST
  UInt32 _numThreads;
  UInt32 _numProcessors;
  #endif

  UInt32 _crcSize;
  CObjectVector<COneMethodInfo> _methods;
  COneMethodInfo _filterMethod;
  bool _autoFilter;

  void SetGlobalLevelAndThreads(COneMethodInfo &oneMethodInfo
      #ifndef _7ZIP_ST
      , UInt32 numThreads
      #endif
      );

  unsigned GetNumEmptyMethods() const
  {
    unsigned i;
    for (i = 0; i < _methods.Size(); i++)
      if (!_methods[i].IsEmpty())
        break;
    return i;
  }

  int GetLevel() const { return _level == (UInt32)(Int32)-1 ? 5 : (int)_level; }
  int GetAnalysisLevel() const { return _analysisLevel; }

  void Init();

  CMultiMethodProps() { Init(); }
  HRESULT SetProperty(const wchar_t *name, const PROPVARIANT &value);
};

class CSingleMethodProps: public COneMethodInfo
{
  UInt32 _level;
  
public:
  #ifndef _7ZIP_ST
  UInt32 _numThreads;
  UInt32 _numProcessors;
  #endif

  void Init();
  CSingleMethodProps() { Init(); }
  int GetLevel() const { return _level == (UInt32)(Int32)-1 ? 5 : (int)_level; }
  HRESULT SetProperties(const wchar_t * const *names, const PROPVARIANT *values, UInt32 numProps);
};

}

#endif
