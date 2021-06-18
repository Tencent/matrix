//
// Created by YinSheng Tang on 2021/4/29.
//

#ifndef LIBMATRIX_ANDROID_PTHREADHOOK_H
#define LIBMATRIX_ANDROID_PTHREADHOOK_H


namespace pthread_hook {
    typedef void* (*pthread_routine_t)(void*);

    extern void SetThreadTraceEnabled(bool enabled);
    extern void SetThreadStackShrinkEnabled(bool enabled);
    extern void SetThreadStackShrinkIgnoredCreatorSoPatterns(const char** patterns, size_t pattern_count);
    extern void InstallHooks(bool enable_debug);
}


#endif //LIBMATRIX_ANDROID_PTHREADHOOK_H
