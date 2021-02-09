PROG = 7z.exe
MY_CONSOLE = 1
CFLAGS = $(CFLAGS)  \
  -DEXTERNAL_CODECS \

!IFNDEF UNDER_CE
CFLAGS = $(CFLAGS) -DWIN_LONG_PATH -D_7ZIP_LARGE_PAGES -DSUPPORT_DEVICE_FILE
!ENDIF

COMMON_OBJS = \
  $O\CommandLineParser.obj \
  $O\CRC.obj \
  $O\IntToString.obj \
  $O\ListFileUtils.obj \
  $O\NewHandler.obj \
  $O\StdInStream.obj \
  $O\StdOutStream.obj \
  $O\MyString.obj \
  $O\StringConvert.obj \
  $O\StringToInt.obj \
  $O\UTFConvert.obj \
  $O\MyVector.obj \
  $O\Wildcard.obj \

WIN_OBJS = \
  $O\DLL.obj \
  $O\ErrorMsg.obj \
  $O\FileDir.obj \
  $O\FileFind.obj \
  $O\FileIO.obj \
  $O\FileLink.obj \
  $O\FileName.obj \
  $O\FileSystem.obj \
  $O\MemoryLock.obj \
  $O\PropVariant.obj \
  $O\PropVariantConv.obj \
  $O\Registry.obj \
  $O\System.obj \
  $O\TimeUtils.obj \

7ZIP_COMMON_OBJS = \
  $O\CreateCoder.obj \
  $O\FilePathAutoRename.obj \
  $O\FileStreams.obj \
  $O\FilterCoder.obj \
  $O\LimitedStreams.obj \
  $O\MethodProps.obj \
  $O\ProgressUtils.obj \
  $O\PropId.obj \
  $O\StreamObjects.obj \
  $O\StreamUtils.obj \
  $O\UniqBlocks.obj \

AR_COMMON_OBJS = \
  $O\OutStreamWithCRC.obj \

COMPRESS_OBJS = \
  $O\CopyCoder.obj \

C_OBJS = \
  $O\Alloc.obj \
  $O\CpuArch.obj \
  $O\Sort.obj \
  $O\Threads.obj \

!include "../../Crc.mak"
!include "Console.mak"

!include "../../7zip.mak"
