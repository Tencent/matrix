//
// Created by tomystang on 2020/11/19.
//

#ifndef __MEMGUARD_COM_TENCENT_MM_TOOLS_MEMGUARD_MEMGUARD_00024OPTIONS_H__
#define __MEMGUARD_COM_TENCENT_MM_TOOLS_MEMGUARD_MEMGUARD_00024OPTIONS_H__


#include <jni.h>

namespace memguard {
    namespace jni {
        extern bool FillOptWithJavaOptions(JNIEnv *env, jobject jopts, Options* opts);
    }
}


#endif //__MEMGUARD_COM_TENCENT_MM_TOOLS_MEMGUARD_MEMGUARD_00024OPTIONS_H__
