C_OBJS = $(C_OBJS) \
  $O\7zCrc.obj
!IF "$(CPU)" == "IA64" || "$(CPU)" == "MIPS" || "$(CPU)" == "ARM"
C_OBJS = $(C_OBJS) \
!ELSE
ASM_OBJS = $(ASM_OBJS) \
!ENDIF
  $O\7zCrcOpt.obj
