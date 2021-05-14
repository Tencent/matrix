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
  .balign    16
AsmGetRegs:
  .cfi_startproc
  .cfi_def_cfa $sp, 0
  .set push
  .set noreorder
  .cpload $t9
  sd   $zero, 0($a0)
  .set noat
  sd   $at, 8($a0)
  .set at
  sd   $v0, 16($a0)
  sd   $v1, 24($a0)
  sd   $a0, 32($a0)
  sd   $a1, 40($a0)
  sd   $a2, 48($a0)
  sd   $a3, 56($a0)
  sd   $a4, 64($a0)
  sd   $a5, 72($a0)
  sd   $a6, 80($a0)
  sd   $a7, 88($a0)
  sd   $t0, 96($a0)
  sd   $t1, 104($a0)
  sd   $t2, 112($a0)
  sd   $t3, 120($a0)
  sd   $s0, 128($a0)
  sd   $s1, 136($a0)
  sd   $s2, 144($a0)
  sd   $s3, 152($a0)
  sd   $s4, 160($a0)
  sd   $s5, 168($a0)
  sd   $s6, 176($a0)
  sd   $s7, 184($a0)
  sd   $t8, 192($a0)
  sd   $t9, 200($a0)
  sd   $k0, 208($a0)
  sd   $k1, 216($a0)
  sd   $gp, 224($a0)
  sd   $sp, 232($a0)
  sd   $s8, 240($a0)
  sd   $ra, 248($a0)
  jalr $zero, $ra
  sd   $ra, 256($a0)   // set PC to the calling function

  .set pop
  .cfi_endproc
  .size AsmGetRegs, .-AsmGetRegs
  .end      AsmGetRegs
