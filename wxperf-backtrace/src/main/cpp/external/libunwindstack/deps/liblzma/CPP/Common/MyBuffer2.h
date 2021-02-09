// Common/MyBuffer2.h

#ifndef __COMMON_MY_BUFFER2_H
#define __COMMON_MY_BUFFER2_H

#include "../../C/Alloc.h"

#include "Defs.h"

class CMidBuffer
{
  Byte *_data;
  size_t _size;

  CLASS_NO_COPY(CMidBuffer)

public:
  CMidBuffer(): _data(NULL), _size(0) {};
  ~CMidBuffer() { ::MidFree(_data); }

  void Free() { ::MidFree(_data); _data = NULL; _size = 0; }

  bool IsAllocated() const { return _data != NULL; }
  operator       Byte *()       { return _data; }
  operator const Byte *() const { return _data; }
  size_t Size() const { return _size; }

  void AllocAtLeast(size_t size)
  {
    if (!_data || size > _size)
    {
      const size_t kMinSize = (size_t)1 << 16;
      if (size < kMinSize)
        size = kMinSize;
      ::MidFree(_data);
      _size = 0;
      _data = 0;
      _data = (Byte *)::MidAlloc(size);
      if (_data)
        _size = size;
    }
  }
};

#endif
