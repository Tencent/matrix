#if !defined(__LIBUDF_UNWIND_P_H__)

/*
#include <private/libc_logging.h>
*/

#define LIBUDF_UNWIND_DEBUG 0

#if LIBUDF_UNWIND_DEBUG
#define LIBUDF_LOG(format, ...) \
    __libc_format_log(ANDROID_LOG_DEBUG, "libudf_unwind", (format), ##__VA_ARGS__ )
#else
#define LIBUDF_LOG(format, ...)
#endif

#endif

