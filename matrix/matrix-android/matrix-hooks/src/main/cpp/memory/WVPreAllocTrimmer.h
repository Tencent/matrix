//
// Created by YinSheng Tang on 2021/5/12.
//

#ifndef MATRIX_WVPREALLOCTRIMMER_H
#define MATRIX_WVPREALLOCTRIMMER_H


#include <jni.h>

namespace matrix {
    namespace wv_prealloc_trimmer {
        bool Install(JNIEnv* env, jint sdk_ver, jobject classloader);
    }
}

#endif //MATRIX_WVPREALLOCTRIMMER_H
