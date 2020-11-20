#ifndef _LIBWECHATBACKTRACE_QUICKEN_JNI_H
#define _LIBWECHATBACKTRACE_QUICKEN_JNI_H

#include <jni.h>

#include "Errors.h"

namespace wechat_backtrace {

    QUT_EXTERN_C int QutJNIOnLoaded(JavaVM *vm, JNIEnv *env);

    QUT_EXTERN_C void InvokeJava_RequestQutGenerate();

}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_QUICKEN_JNI_H
