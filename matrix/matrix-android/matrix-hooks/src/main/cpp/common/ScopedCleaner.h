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
// Created by YinSheng Tang on 2021/5/11.
//

#ifndef MATRIX_ANDROID_SCOPEDCLEANER_H
#define MATRIX_ANDROID_SCOPEDCLEANER_H


#include <cstddef>

namespace matrix {
    template <class TDtor>
    class ScopedCleaner {
    public:
        ScopedCleaner(const TDtor& dtor): mDtor(dtor), mOmitted(false) {}

        ScopedCleaner(ScopedCleaner&& other): mDtor(other.mDtor), mOmitted(other.mOmitted) {
            other.Omit();
        }

        ~ScopedCleaner() {
            if (!mOmitted) {
                mDtor();
                mOmitted = true;
            }
        }

        void Omit() {
            mOmitted = true;
        }

    private:
        void* operator new(size_t) = delete;
        void* operator new[](size_t) = delete;
        ScopedCleaner(const ScopedCleaner&) = delete;
        ScopedCleaner& operator =(const ScopedCleaner&) = delete;
        ScopedCleaner& operator =(ScopedCleaner&&) = delete;

        TDtor mDtor;
        bool  mOmitted;
    };

    template <class TDtor>
    static ScopedCleaner<TDtor> MakeScopedCleaner(const TDtor& dtor) {
        return ScopedCleaner<TDtor>(std::forward<const TDtor&>(dtor));
    }
}


#endif //MATRIX_ANDROID_SCOPEDCLEANER_H
