/*
 * Copyright (C) 2017 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

  .text
  .type     AsmGetRegs, %function
  .globl    AsmGetRegs
  .ent      AsmGetRegs
  .balign   16
AsmGetRegs:
  .cfi_startproc
  .cfi_def_cfa $sp, 0
  .set push
  .set noreorder
  .cpload $t9
  sw   $zero, 0($a0)
  .set noat
  sw   $at, 4($a0)
  .set at
  sw   $v0, 8($a0)
  sw   $v1, 12($a0)
  sw   $a0, 16($a0)
  sw   $a1, 20($a0)
  sw   $a2, 24($a0)
  sw   $a3, 28($a0)
  sw   $t0, 32($a0)
  sw   $t1, 36($a0)
  sw   $t2, 40($a0)
  sw   $t3, 44($a0)
  sw   $t4, 48($a0)
  sw   $t5, 52($a0)
  sw   $t6, 56($a0)
  sw   $t7, 60($a0)
  sw   $s0, 64($a0)
  sw   $s1, 68($a0)
  sw   $s2, 72($a0)
  sw   $s3, 76($a0)
  sw   $s4, 80($a0)
  sw   $s5, 84($a0)
  sw   $s6, 88($a0)
  sw   $s7, 92($a0)
  sw   $t8, 96($a0)
  sw   $t9, 100($a0)
  sw   $k0, 104($a0)
  sw   $k1, 108($a0)
  sw   $gp, 112($a0)
  sw   $sp, 116($a0)
  sw   $s8, 120($a0)
  sw   $ra, 124($a0)
  jalr $zero, $ra
  sw   $ra, 128($a0)   // set PC to the calling function

  .set pop
  .cfi_endproc
  .size     AsmGetRegs, .-AsmGetRegs
  .end      AsmGetRegs
