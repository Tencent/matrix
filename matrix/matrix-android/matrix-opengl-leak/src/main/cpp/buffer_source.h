/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef buffer_source_h
#define buffer_source_h

#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

class buffer_source {
public:
    buffer_source() {
        _buffer = NULL;
        _buffer_size = 0;
    }

    virtual ~buffer_source() {}

    inline void *buffer() { return _buffer; }

    inline size_t buffer_size() { return _buffer_size; }

    virtual void *realloc(size_t new_size) = 0;
    virtual void free() = 0;
    virtual bool init_fail() = 0;

protected:
    void *_buffer;
    size_t _buffer_size;
};

class buffer_source_memory : public buffer_source {
public:
    ~buffer_source_memory() { free(); }

    virtual bool init_fail() { return false; }

    virtual void *realloc(size_t new_size) {

        void *ptr = ::realloc(_buffer, new_size);
        if (ptr != NULL) {

            _buffer = ptr;
            _buffer_size = new_size;
        }

        return ptr;
    }

    virtual void free() {
        if (_buffer) {

            ::free(_buffer);
            _buffer = NULL;
            _buffer_size = 0;
        }
    }
};

#endif /* buffer_source_h */
