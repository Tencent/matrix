//
// Created by YinSheng Tang on 2021/5/11.
//

#ifndef LIBMATRIX_JNI_MACROS_H
#define LIBMATRIX_JNI_MACROS_H


#define LIKELY(cond) (__builtin_expect(!!(cond), 1))

#define UNLIKELY(cond) (__builtin_expect(!!(cond), 0))

#define EXPORT extern __attribute__ ((visibility ("default")))

#define EXPORT_C extern "C" __attribute__ ((visibility ("default")))

#define NELEM(...) ((sizeof(__VA_ARGS__)) / (sizeof(__VA_ARGS__[0])))


#endif //LIBMATRIX_JNI_MACROS_H
