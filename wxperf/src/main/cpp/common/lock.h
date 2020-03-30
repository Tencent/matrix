//
// Created by Yves on 2019/8/5.
//

#ifndef LIBPRELOADHOOK_LOCK_H
#define LIBPRELOADHOOK_LOCK_H

//#include <atomic>

#ifdef __cplusplus
extern "C" {
#endif

//    void init_lock(unsigned char*);

//    void acquire_lock_log();

// fixme deprecated
    void acquire_lock();
// fixme deprecated
    void release_lock();

#ifdef __cplusplus
}
#endif

#endif //LIBPRELOADHOOK_LOCK_H
