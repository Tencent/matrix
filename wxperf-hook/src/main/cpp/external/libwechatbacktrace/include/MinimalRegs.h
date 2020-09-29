//
// Created by Carl on 2020-04-27.
//

#ifndef LIBWXPERF_JNI_MINIMALREGS_H
#define LIBWXPERF_JNI_MINIMALREGS_H

#define R7(regs) regs[0]
#define R11(regs) regs[1]
#define SP(regs) regs[2]
#define PC(regs) regs[3]
#define LR(regs) regs[4]

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

#endif

#endif //LIBWXPERF_JNI_MINIMALREGS_H
