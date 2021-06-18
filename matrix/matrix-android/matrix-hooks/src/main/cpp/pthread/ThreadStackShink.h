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
