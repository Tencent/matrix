//
// Created by Yves on 2020-04-28.
//

#ifndef LIBWXPERF_JNI_MEMORYHOOK_H
#define LIBWXPERF_JNI_MEMORYHOOK_H

#define TAG "Wxperf.MemoryHook"

void on_alloc_memory(void *__caller, void *__ptr, size_t __byte_count);

void on_free_memory(void *__ptr);

void on_mmap_memory(void *__caller, void *__ptr, size_t __byte_count);

void on_munmap_memory(void *__ptr);

void memory_hook_on_dlopen(const char *__file_name);

void dump(bool __enable_mmap = false,
          const char *__log_path = "/sdcard/memory_hook.log",
          const char *__json_path = "/sdcard/memory_hook.json");

void enable_stacktrace(bool);

void set_stacktrace_log_threshold(size_t __threshold);

void set_sample_size_range(size_t __min, size_t __max);

void set_sampling(double);

void enable_caller_sampling(bool __enable);

void memory_hook_init();

#endif //LIBWXPERF_JNI_MEMORYHOOK_H
