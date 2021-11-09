//
// Created by YinSheng Tang on 2021/6/19.
//

#ifndef MATRIX_ANDROID_GCSEMISPACETRIMMER_H
#define MATRIX_ANDROID_GCSEMISPACETRIMMER_H


#include <jni.h>

namespace matrix {
    namespace gc_ss_trimmer {
        extern bool IsCompatible();
        extern bool Install(JNIEnv *env);
    }
}


#endif //MATRIX_ANDROID_GCSEMISPACETRIMMER_H
