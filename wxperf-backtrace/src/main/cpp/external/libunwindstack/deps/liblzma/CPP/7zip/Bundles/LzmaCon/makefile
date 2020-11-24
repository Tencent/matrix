PROG = lzma.exe
MY_CONSOLE = 1
CFLAGS = $(CFLAGS)

CURRENT_OBJS = \
  $O\LzmaAlone.obj \

COMPRESS_OBJS = \
  $O\LzmaDecoder.obj \
  $O\LzmaEncoder.obj \
  $O\LzmaRegister.obj \

COMMON_OBJS = \
  $O\CommandLineParser.obj \
  $O\CRC.obj \
  $O\CrcReg.obj \
  $O\IntToString.obj \
  $O\MyString.obj \
  $O\NewHandler.obj \
  $O\StringConvert.obj \
  $O\StringToInt.obj \
  $O\MyVector.obj

WIN_OBJS = \
  $O\FileIO.obj \
  $O\PropVariant.obj \
  $O\System.obj

7ZIP_COMMON_OBJS = \
  $O\CWrappers.obj \
  $O\CreateCoder.obj \
  $O\FileStreams.obj \
  $O\FilterCoder.obj \
  $O\MethodProps.obj \
  $O\OutBuffer.obj \
  $O\StreamUtils.obj \

UI_COMMON_OBJS = \
  $O\Bench.obj \

CONSOLE_OBJS = \
  $O\ConsoleClose.obj \
  $O\BenchCon.obj \

C_OBJS = \
  $O\Alloc.obj \
  $O\Bra86.obj \
  $O\CpuArch.obj \
  $O\LzFind.obj \
  $O\LzFindMt.obj \
  $O\Lzma86Dec.obj \
  $O\Lzma86Enc.obj \
  $O\LzmaDec.obj \
  $O\LzmaEnc.obj \
  $O\Threads.obj \

!include "../../Crc.mak"
!include "../../LzmaDec.mak"

!include "../../7zip.mak"
