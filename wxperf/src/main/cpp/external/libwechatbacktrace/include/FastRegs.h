//
// Created by Carl on 2020-04-27.
//

#ifndef LIBWXPERF_JNI_FASTREGS_H
#define LIBWXPERF_JNI_FASTREGS_H

#if defined(__arm__)

// TODO minimal
inline __always_inline void RegsMinimalGetLocal(void *reg_data) {
  asm volatile(
      ".align 2\n"
      "bx pc\n"
      "nop\n"
      ".code 32\n"
      "stmia %[base], {r0-r12}\n"
      "add %[base], #52\n"
      "mov r1, r13\n"
      "mov r2, r14\n"
      "mov r3, r15\n"
      "stmia %[base], {r1-r3}\n"
      "orr %[base], pc, #1\n"
      "bx %[base]\n"
      : [base] "+r"(reg_data)
      :
      : "memory");
}

#elif defined(__aarch64__)

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

#endif //LIBWXPERF_JNI_FASTREGS_H
