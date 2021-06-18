//
// Created by tomystang on 2021/2/3.
//

#ifndef LIBMATRIX_HOOK_THREADSTACKSHINK_H
#define LIBMATRIX_HOOK_THREADSTACKSHINK_H


namespace thread_stack_shink {
    extern void OnPThreadCreate(pthread_t* pthread, pthread_attr_t const* attr, void* (*start_routine)(void*), void* args);
}


#endif //LIBMATRIX_HOOK_THREADSTACKSHINK_H
