//
// Created by YinSheng Tang on 2021/4/30.
//

#ifndef LIBMATRIX_JNI_CONDITIONS_H
#define LIBMATRIX_JNI_CONDITIONS_H


#define LIKELY(cond) (__builtin_expect(!!(cond), 1))

#define UNLIKELY(cond) (__builtin_expect(!!(cond), 0))


#endif //LIBMATRIX_JNI_CONDITIONS_H
