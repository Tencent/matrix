// Copyright 2020 Tencent Inc.  All rights reserved.
//
// Author: leafjia@tencent.com
//
// managed_jnienv.h

#ifndef LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_NATIVEHELPER_MANAGED_JNIENV_H_
#define LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_NATIVEHELPER_MANAGED_JNIENV_H_

#include <jni.h>

namespace JniInvocation {

    void init(JavaVM *vm);
    JavaVM *getJavaVM();
    JNIEnv *getEnv();

}

#endif  // LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_NATIVEHELPER_MANAGED_JNIENV_H_

