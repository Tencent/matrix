/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
