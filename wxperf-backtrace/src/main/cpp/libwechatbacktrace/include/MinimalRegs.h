//
// Created by Carl on 2020-04-27.
//

#ifndef LIBWECHATBACKTRACE_MINIMALREGS_H
#define LIBWECHATBACKTRACE_MINIMALREGS_H

#include "unwindstack/MachineArm64.h"
#include "unwindstack/MachineArm.h"

// For Arm 32-bit.
#define R4(regs) regs[0]
#define R7(regs) regs[1]
#define R10(regs) regs[2]
#define R11(regs) regs[3]

// For Arm 64-bit
#define REG_UNUSE(regs) regs[0]
#define R20(regs) regs[1]
#define R28(regs) regs[2]
#define R29(regs) regs[3]

// Comm
#define SP(regs) regs[4]
#define PC(regs) regs[5]
#define LR(regs) regs[6]

#define QUT_MINIMAL_REG_SIZE 7

#ifdef __arm__
#define REGS_TOTAL ARM_REG_LAST
#else
#define REGS_TOTAL ARM64_REG_EXT_LAST
#endif

#if defined(__arm__)

// Only get 4 registers(r7/r11/sp/pc)
inline __attribute__((__always_inline__)) void RegsMinimalGetLocal(void *reg_data) {
    asm volatile(
    ".align 2\n"
    "bx pc\n"
    "nop\n"
    ".code 32\n"
    "stmia %[base], {r7, r11}\n"
    "add %[base], #8\n"
    "mov r1, r13\n"
    "mov r2, r15\n"
    "stmia %[base], {r1, r2}\n"
    "orr %[base], pc, #1\n"
    "bx %[base]\n"
    : [base] "+r"(reg_data)
    :
    : "r1", "r2", "memory");
}

// Fill reg_data[6] with [r4, r7, r10, r11, sp, pc].
inline __attribute__((__always_inline__)) void GetMinimalRegs(void *reg_data) {
    asm volatile(
    ".align 2\n"
    "bx pc\n"
    "nop\n"
    ".code 32\n"
    "stmia %[base], {r4, r7, r10, r11}\n"
    "add %[base], #16\n"
    "mov r1, r13\n"
    "mov r2, r15\n"
    "stmia %[base], {r1, r2}\n"
    "orr %[base], pc, #1\n"
    "bx %[base]\n"
    : [base] "+r"(reg_data)
    :
    : "r1", "r2", "memory");
}

#elif defined(__aarch64__)

// Only get 4 registers from x29 to x32.
inline __always_inline void RegsMinimalGetLocal(void *reg_data) {
    asm volatile(
    "1:\n"
    "stp x29, x30, [%[base], #0]\n"
    "mov x12, sp\n"
    "adr x13, 1b\n"
    "stp x12, x13, [%[base], #16]\n"
    : [base] "+r"(reg_data)
    :
    : "x12", "x13", "memory");
}

// Fill reg_data[6] with [unset, unuse, x28, x29, sp, pc].
inline __always_inline void GetMinimalRegs(void *reg_data) {
    asm volatile(
    "1:\n"
    "stp x28, x29, [%[base], #16]\n"
    "mov x12, sp\n"
    "adr x13, 1b\n"
    "stp x12, x13, [%[base], #32]\n"
    : [base] "+r"(reg_data)
    :
    : "x12", "x13", "memory");
}

#endif

#endif //LIBWECHATBACKTRACE_MINIMALREGS_H
