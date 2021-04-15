//
// Created by Yves on 2020-04-28.
//

#ifndef LIBMATRIX_HOOK_MEMORYHOOK_H
#define LIBMATRIX_HOOK_MEMORYHOOK_H

#define TAG "Matrix.MemoryHook"

void on_alloc_memory(void *caller, void *ptr, size_t byte_count);

void on_free_memory(void *ptr);

void on_mmap_memory(void *caller, void *ptr, size_t byte_count);

void on_munmap_memory(void *ptr);

void memory_hook_on_dlopen(const char *file_name);

void dump(bool enable_mmap = false,
          const char *log_path = nullptr,
          const char *json_path = nullptr);

void enable_stacktrace(bool);

void set_stacktrace_log_threshold(size_t threshold);

void set_sample_size_range(size_t min, size_t max);

void set_sampling(double);

void enable_caller_sampling(bool enable);

void memory_hook_init();

#endif //LIBMATRIX_HOOK_MEMORYHOOK_H
