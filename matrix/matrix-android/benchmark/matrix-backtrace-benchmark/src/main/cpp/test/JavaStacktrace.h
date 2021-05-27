#ifndef JAVA_STACKTRACE_H
#define JAVA_STACKTRACE_H

#include <jni.h>

void setJavaVM(JavaVM *vm);
void java_unwind(jobject &throwable);
void pin_object(jobject &throwable);

#endif