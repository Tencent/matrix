#include <time.h>
#include <cstdio>
#include <android/log.h>
#include <dlfcn.h>
#include <unistd.h>
#include "Log.h"
#include "Predefined.h"

namespace wechat_backtrace {
    extern "C" {

    extern void *fake_dlopen(const char *filename, int flags);
    extern void *fake_dlsym(void *handle, const char *symbol);
    extern void fake_dlclose(void *handle);

    typedef enum {
        kLevelAll = 0,
        kLevelVerbose = 0,
        kLevelDebug,    // Detailed information on the flow through the system.
        kLevelInfo,     // Interesting runtime events (startup/shutdown), should be conservative and keep to a minimum.
        kLevelWarn,     // Other runtime situations that are undesirable or unexpected, but not necessarily "wrong".
        kLevelError,    // Other runtime errors or unexpected conditions.
        kLevelFatal,    // Severe errors that cause premature termination.
        kLevelNone,     // Special level used to disable all log messages.
    } TLogLevel;

    typedef struct XLoggerInfo_t {
        TLogLevel level;
        const char *tag;
        const char *filename;
        const char *func_name;
        int line;

        struct timeval timeval;
        intmax_t pid;
        intmax_t tid;
        intmax_t maintid;
        int traceLog;
    } XLoggerInfo;

    typedef int (*xlogger_IsEnabledFor_func)(TLogLevel _level);
    typedef void (*xlogger_VPrint_func)(const XLoggerInfo *_info, const char *_format,
                                        va_list _list);

    static xlogger_IsEnabledFor_func xlogger_IsEnabledFor;
    static xlogger_VPrint_func xlogger_VPrint;

    int init_xlog_logger(const char *xlog_so_path) {

        void *handle = fake_dlopen(xlog_so_path, RTLD_NOW);
        int ret = 0;

        if (!handle) {
            ret = -1;
            goto exit;
        }

        xlogger_IsEnabledFor = (xlogger_IsEnabledFor_func)
                fake_dlsym(handle, "xlogger_IsEnabledFor");
        xlogger_VPrint = (xlogger_VPrint_func)
                fake_dlsym(handle, "xlogger_VPrint");
        if (!xlogger_IsEnabledFor || !xlogger_VPrint) {
            ret = -2;
            goto exit;
        }

        exit:
        if (handle) {
            fake_dlclose(handle);
        }
        return ret;
    }

    int xlog_vlogger(int log_level, const char *tag, const char *format, va_list varargs) {

        if (!xlogger_IsEnabledFor || !xlogger_VPrint) {
            return -1;
        }

        TLogLevel xlog_level = static_cast<TLogLevel>(log_level - 2);

        if (!xlogger_IsEnabledFor(xlog_level)) {
            return -2;
        }

        XLoggerInfo_t info = {
                .level = xlog_level,
                .tag = tag,
                .filename = "",
                .func_name = "",
                .line = 0,
                .pid = getpid(),
                .tid = gettid(),
                .maintid = getpid(),
                .traceLog = 0
        };

        gettimeofday(&(info.timeval), NULL);

        xlogger_VPrint(&info, format, varargs);

        return 0;
    }

    int xlog_logger(int log_level, const char *tag, const char *format, ...) {
        va_list ap;
        va_start(ap, format);
        xlog_vlogger(log_level, tag ,format, ap);
        va_end(ap);
    }

    }
}