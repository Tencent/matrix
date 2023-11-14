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

#include "logger_internal.h"

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
        void *ptr = inter_realloc(_buffer, new_size);
        if (ptr != NULL) {
            _buffer = ptr;
            _buffer_size = new_size;
        }
        return ptr;
    }

    virtual void free() {
        if (_buffer) {
            inter_free(_buffer);
            _buffer = NULL;
            _buffer_size = 0;
        }
    }
};

class buffer_source_file : public buffer_source {
public:
    buffer_source_file(const char *dir, const char *file_name) {
        _fd = open_file(dir, file_name);
        _file_name = file_name;

        if (_fd < 0) {
            goto init_fail;
        } else {
            struct stat st = { 0 };
            if (fstat(_fd, &st) == -1) {
                goto init_fail;
            } else {
                if (st.st_size > 0) {
                    void *buff = inter_mmap(NULL, (size_t)st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, _fd, 0);
                    if (buff == MAP_FAILED) {
                        __malloc_printf("fail to mmap, %s", strerror(errno));
                        goto init_fail;
                    } else {
                        _fs = (size_t)st.st_size;
                        _buffer = buff;
                        _buffer_size = _fs;
                    }
                } else {
                    _fs = 0;
                    _buffer = NULL;
                    _buffer_size = 0;
                }
            }
        }
        return;

    init_fail:
        if (_fd >= 0) {
            close(_fd);
            _fd = -1;
        }
    }

    ~buffer_source_file() {
        if (_fd >= 0) {
            free();
            close(_fd);
        }
    }

    virtual bool init_fail() { return _fd < 0; }

    virtual void *realloc(size_t new_size) {
        new_size = round_page(new_size);
        if (ftruncate(_fd, new_size) != 0) {
            disable_memory_logging();
            __malloc_printf("%s fail to ftruncate, %s, new_size: %llu, errno: %d", _file_name, strerror(errno), (uint64_t)new_size, errno);
            return NULL;
        }

        void *new_mem = inter_mmap(NULL, new_size, PROT_READ | PROT_WRITE, MAP_SHARED, _fd, 0);
        if (new_mem == MAP_FAILED) {
            disable_memory_logging();
            __malloc_printf("%s fail to mmap, %s, new_size: %llu, errno: %d", _file_name, strerror(errno), (uint64_t)new_size, errno);
            return NULL;
        }

        free();

        _fs = new_size;
        _buffer = new_mem;
        _buffer_size = new_size;
        //__malloc_printf("%s new file size: %lu", _file_name, _fs);

        return _buffer;
    }

    virtual void free() {
        if (_buffer && _buffer != MAP_FAILED) {
            inter_munmap(_buffer, _fs);
            _buffer = NULL;
            _buffer_size = 0;
        }
    }

private:
    int _fd;
    size_t _fs;
    const char *_file_name;
};

class memory_pool_file {
public:
    memory_pool_file(const char *dir, const char *file_name) {
        _fs = 0;
        _fd = open_file(dir, file_name);
        _file_name = file_name;

        if (_fd < 0) {
            goto init_fail;
        } else {
            struct stat st = { 0 };
            if (fstat(_fd, &st) == -1) {
                goto init_fail;
            } else {
                _fs = (size_t)st.st_size;
            }
        }
        return;

    init_fail:
        if (_fd >= 0) {
            close(_fd);
            _fd = -1;
        }
    }

    ~memory_pool_file() {
        if (_fd >= 0) {
            close(_fd);
        }
    }

    inline int fd() { return _fd; }
    inline size_t fs() { return _fs; }

    bool init_fail() { return _fd < 0; }

    void *malloc(size_t size) {
        size_t new_size = round_page(size);
        if (ftruncate(_fd, _fs + new_size) != 0) {
            disable_memory_logging();
            __malloc_printf("%s fail to ftruncate, %s, new_size: %llu, errno: %d", _file_name, strerror(errno), (uint64_t)_fs + new_size, errno);
            return NULL;
        }

        void *new_mem = inter_mmap(NULL, new_size, PROT_READ | PROT_WRITE, MAP_SHARED, _fd, _fs);
        if (new_mem == MAP_FAILED) {
            disable_memory_logging();
            __malloc_printf("%s fail to mmap, %s, new_size: %llu, offset: %llu, errno: %d",
                            _file_name,
                            strerror(errno),
                            (uint64_t)new_size,
                            (uint64_t)_fs,
                            errno);
            return NULL;
        }

        _fs += new_size;

        return new_mem;
    }

    void free(void *ptr, size_t size) {
        if (ptr != MAP_FAILED && ptr != NULL) {
            inter_munmap(ptr, size);
        }
    }

private:
    int _fd;
    size_t _fs;
    const char *_file_name;
};

bool shared_memory_pool_file_init(const char *dir);
void *shared_memory_pool_file_alloc(size_t size);

#endif /* buffer_source_h */
