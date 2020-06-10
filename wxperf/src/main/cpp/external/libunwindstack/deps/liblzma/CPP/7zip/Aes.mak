C_OBJS = $(C_OBJS) \
  $O\Aes.obj

!IF "$(PLATFORM)" != "ia64" && "$(PLATFORM)" != "mips" && "$(PLATFORM)" != "arm" && "$(PLATFORM)" != "arm64"
ASM_OBJS = $(ASM_OBJS) \
  $O\AesOpt.obj
!ENDIF
