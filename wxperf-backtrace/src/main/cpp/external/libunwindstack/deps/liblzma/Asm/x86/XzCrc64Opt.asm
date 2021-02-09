; XzCrc64Opt.asm -- CRC64 calculation : optimized version
; 2011-06-28 : Igor Pavlov : Public domain

include 7zAsm.asm

MY_ASM_START

ifdef x64

    rD   equ  r9
    rN   equ  r10

    num_VAR     equ  r8
    table_VAR   equ  r9

    SRCDAT  equ  rN + rD

CRC_XOR macro dest:req, src:req, t:req
    xor     dest, QWORD PTR [r5 + src * 8 + 0800h * t]
endm

CRC1b macro
    movzx   x6, BYTE PTR [rD]
    inc     rD
    movzx   x3, x0_L
    xor     x6, x3
    shr     r0, 8
    CRC_XOR r0, r6, 0
    dec     rN
endm

MY_PROLOG macro crc_end:req
    MY_PUSH_4_REGS
    
    mov     r0, r1
    mov     rN, num_VAR
    mov     r5, table_VAR
    mov     rD, r2
    test    rN, rN
    jz      crc_end
  @@:
    test    rD, 3
    jz      @F
    CRC1b
    jnz     @B
  @@:
    cmp     rN, 8
    jb      crc_end
    add     rN, rD
    mov     num_VAR, rN
    sub     rN, 4
    and     rN, NOT 3
    sub     rD, rN
    mov     x1, [SRCDAT]
    xor     r0, r1
    add     rN, 4
endm

MY_EPILOG macro crc_end:req
    sub     rN, 4
    mov     x1, [SRCDAT]
    xor     r0, r1
    mov     rD, rN
    mov     rN, num_VAR
    sub     rN, rD
  crc_end:
    test    rN, rN
    jz      @F
    CRC1b
    jmp     crc_end
  @@:
    MY_POP_4_REGS
endm

MY_PROC XzCrc64UpdateT4, 4
    MY_PROLOG crc_end_4
    align 16
  main_loop_4:
    mov     x1, [SRCDAT]
    movzx   x2, x0_L
    movzx   x3, x0_H
    shr     r0, 16
    movzx   x6, x0_L
    movzx   x7, x0_H
    shr     r0, 16
    CRC_XOR r1, r2, 3
    CRC_XOR r0, r3, 2
    CRC_XOR r1, r6, 1
    CRC_XOR r0, r7, 0
    xor     r0, r1

    add     rD, 4
    jnz     main_loop_4

    MY_EPILOG crc_end_4
MY_ENDP

else

    rD   equ  r1
    rN   equ  r7

    crc_val     equ (REG_SIZE * 5)
    crc_table   equ (8 + crc_val)
    table_VAR   equ [r4 + crc_table]
    num_VAR     equ table_VAR


    SRCDAT  equ  rN + rD

CRC macro op0:req, op1:req, dest0:req, dest1:req, src:req, t:req
    op0     dest0, DWORD PTR [r5 + src * 8 + 0800h * t]
    op1     dest1, DWORD PTR [r5 + src * 8 + 0800h * t + 4]
endm

CRC_XOR macro dest0:req, dest1:req, src:req, t:req
    CRC xor, xor, dest0, dest1, src, t
endm


CRC1b macro
    movzx   x6, BYTE PTR [rD]
    inc     rD
    movzx   x3, x0_L
    xor     x6, x3
    shrd    r0, r2, 8
    shr     r2, 8
    CRC_XOR r0, r2, r6, 0
    dec     rN
endm

MY_PROLOG macro crc_end:req
    MY_PUSH_4_REGS
    
    mov     rN, r2

    mov     x0, [r4 + crc_val]
    mov     x2, [r4 + crc_val + 4]
    mov     r5, table_VAR
    test    rN, rN
    jz      crc_end
  @@:
    test    rD, 3
    jz      @F
    CRC1b
    jnz     @B
  @@:
    cmp     rN, 8
    jb      crc_end
    add     rN, rD

    mov     num_VAR, rN

    sub     rN, 4
    and     rN, NOT 3
    sub     rD, rN
    xor     r0, [SRCDAT]
    add     rN, 4
endm

MY_EPILOG macro crc_end:req
    sub     rN, 4
    xor     r0, [SRCDAT]

    mov     rD, rN
    mov     rN, num_VAR
    sub     rN, rD
  crc_end:
    test    rN, rN
    jz      @F
    CRC1b
    jmp     crc_end
  @@:
    MY_POP_4_REGS
endm

MY_PROC XzCrc64UpdateT4, 5
    MY_PROLOG crc_end_4
    movzx   x6, x0_L
    align 16
  main_loop_4:
    mov     r3, [SRCDAT]
    xor     r3, r2

    CRC xor, mov, r3, r2, r6, 3
    movzx   x6, x0_H
    shr     r0, 16
    CRC_XOR r3, r2, r6, 2

    movzx   x6, x0_L
    movzx   x0, x0_H
    CRC_XOR r3, r2, r6, 1
    CRC_XOR r3, r2, r0, 0
    movzx   x6, x3_L
    mov     r0, r3

    add     rD, 4
    jnz     main_loop_4

    MY_EPILOG crc_end_4
MY_ENDP

endif

end
