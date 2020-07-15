//
// Created by tomystang on 2019/6/20.
//

#ifndef LIBUNWINDSTACK_COMPAT_UIO_H
#define LIBUNWINDSTACK_COMPAT_UIO_H


#include <sys/uio.h>

#if __ANDROID_API__ >= 23

#define compat_process_vm_readv process_vm_readv

#else

#include <sys/types.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

ssize_t compat_process_vm_readv(pid_t __pid, const struct iovec *__local_iov,
                                unsigned long __local_iov_count, const struct iovec *__remote_iov,
                                unsigned long __remote_iov_count, unsigned long __flags);

#ifdef __cplusplus
}
#endif

#endif //__ANDROID_API__ >= 23


#endif //LIBUNWINDSTACK_COMPAT_UIO_H
