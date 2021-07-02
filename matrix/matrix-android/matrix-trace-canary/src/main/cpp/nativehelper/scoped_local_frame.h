
/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_NATIVEHELPER_SCOPED_LOCAL_FRAME_H_
#define LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_NATIVEHELPER_SCOPED_LOCAL_FRAME_H_

#include "jni.h"

class ScopedLocalFrame {
 public:
    explicit ScopedLocalFrame(JNIEnv* env) : mEnv(env) {
        mEnv->PushLocalFrame(128);
    }

    ~ScopedLocalFrame() {
        mEnv->PopLocalFrame(NULL);
    }

 private:
    JNIEnv* const mEnv;

    DISALLOW_COPY_AND_ASSIGN(ScopedLocalFrame);
};

#endif  // LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_NATIVEHELPER_SCOPED_LOCAL_FRAME_H_