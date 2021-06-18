//
// Created by YinSheng Tang on 2021/4/29.
//

#ifndef LIBMATRIX_ANDROID_PTHREADHOOK_H
#define LIBMATRIX_ANDROID_PTHREADHOOK_H


namespace pthread_hook {
    typedef void* (*pthread_routine_t)(void*);

    extern void SetThreadTraceEnabled(bool enabled);
    extern void SetThreadStackShinkEnabled(bool enabled);
    extern void InstallHooks(bool enable_debug);
}


#endif //LIBMATRIX_ANDROID_PTHREADHOOK_H
