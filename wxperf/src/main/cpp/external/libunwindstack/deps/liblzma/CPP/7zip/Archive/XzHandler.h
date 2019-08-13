// XzHandler.h

#ifndef __XZ_HANDLER_H
#define __XZ_HANDLER_H

#include "../../../C/Xz.h"

#include "../ICoder.h"

namespace NArchive {
namespace NXz {
 
struct CXzUnpackerCPP
{
  Byte *InBuf;
  Byte *OutBuf;
  CXzUnpacker p;
  
  CXzUnpackerCPP();
  ~CXzUnpackerCPP();
};

struct CStatInfo
{
  UInt64 InSize;
  UInt64 OutSize;
  UInt64 PhySize;

  UInt64 NumStreams;
  UInt64 NumBlocks;

  bool UnpackSize_Defined;

  bool NumStreams_Defined;
  bool NumBlocks_Defined;

  bool IsArc;
  bool UnexpectedEnd;
  bool DataAfterEnd;
  bool Unsupported;
  bool HeadersError;
  bool DataError;
  bool CrcError;

  CStatInfo() { Clear(); }

  void Clear();
};

struct CDecoder: public CStatInfo
{
  CXzUnpackerCPP xzu;
  SRes DecodeRes; // it's not HRESULT

  CDecoder(): DecodeRes(SZ_OK) {}

  /* Decode() can return ERROR code only if there is progress or stream error.
     Decode() returns S_OK in case of xz decoding error, but DecodeRes and CStatInfo contain error information */
  HRESULT Decode(ISequentialInStream *seqInStream, ISequentialOutStream *outStream, ICompressProgressInfo *compressProgress);
  Int32 Get_Extract_OperationResult() const;
};

}}

#endif
