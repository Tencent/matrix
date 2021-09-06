//
// Created by 邓沛堆 on 2020-04-27.
//

#ifndef OPENGL_API_HOOK_GET_TLS_H
#define OPENGL_API_HOOK_GET_TLS_H

struct gl_hooks_t {
    struct gl_t {
        void *foo1;
    } gl;
};

#if defined(__aarch64__)
# define __get_tls() ({ void** __val; __asm__("mrs %0, tpidr_el0" : "=r"(__val)); __val; })
#elif defined(__arm__)
# define __get_tls() ({ void** __val; __asm__("mrc p15, 0, %0, c13, c0, 3" : "=r"(__val)); __val; })
#elif defined(__mips__)
# define __get_tls() \
    /* On mips32r1, this goes via a kernel illegal instruction trap that's optimized for v1. */ \
    ({ register void** __val asm("v1"); \
       __asm__(".set    push\n" \
               ".set    mips32r2\n" \
               "rdhwr   %0,$29\n" \
               ".set    pop\n" : "=r"(__val)); \
       __val; })
#elif defined(__i386__)
# define __get_tls() ({ void** __val; __asm__("movl %%gs:0, %0" : "=r"(__val)); __val; })
#elif defined(__x86_64__)
# define __get_tls() ({ void** __val; __asm__("mov %%fs:0, %0" : "=r"(__val)); __val; })
#else
#error unsupported architecture
#endif

gl_hooks_t *get_gl_hooks();

#endif //OPENGL_API_HOOK_GET_TLS_H
