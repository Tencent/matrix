C_OBJS = $(C_OBJS) \
  $O\XzCrc64.obj
!IF "$(CPU)" == "IA64" || "$(CPU)" == "MIPS" || "$(CPU)" == "ARM"
C_OBJS = $(C_OBJS) \
!ELSE
ASM_OBJS = $(ASM_OBJS) \
!ENDIF
  $O\XzCrc64Opt.obj
