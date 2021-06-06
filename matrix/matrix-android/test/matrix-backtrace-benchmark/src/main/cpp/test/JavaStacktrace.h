#ifndef JAVA_STACKTRACE_H
#define JAVA_STACKTRACE_H

#include <jni.h>

JavaVM *getJavaVM();

void setJavaVM(JavaVM *vm);

void java_unwind(jobject &throwable);

void java_get_stack_traces(JNIEnv *env, jobjectArray &traces);

void pin_object(jobject &obj);

#endif