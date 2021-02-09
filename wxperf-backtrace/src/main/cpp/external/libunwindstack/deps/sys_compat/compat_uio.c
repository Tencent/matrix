//
// Created by tomystang on 2019/6/20.
//

#if __ANDROID_API__ < 23

#include <syscall.h>

#include "compat_uio.h"

ssize_t compat_process_vm_readv(pid_t __pid, const struct iovec* __local_iov, unsigned long __local_iov_count, const struct iovec* __remote_iov, unsigned long __remote_iov_count, unsigned long __flags) {
    return syscall(__NR_process_vm_readv, __pid, __local_iov, __local_iov_count, __remote_iov, __remote_iov_count, __flags);
}


#endif