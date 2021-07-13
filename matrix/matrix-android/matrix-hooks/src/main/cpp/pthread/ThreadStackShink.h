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
// Created by tomystang on 2021/2/3.
//

#ifndef LIBMATRIX_HOOK_THREADSTACKSHINK_H
#define LIBMATRIX_HOOK_THREADSTACKSHINK_H


namespace thread_stack_shink {
    extern void SetIgnoredCreatorSoPatterns(const char** patterns, size_t pattern_count);
    extern void OnPThreadCreate(const Dl_info* caller_info, pthread_t* pthread, pthread_attr_t const* attr, void* (*start_routine)(void*), void* args);
}


#endif //LIBMATRIX_HOOK_THREADSTACKSHINK_H
