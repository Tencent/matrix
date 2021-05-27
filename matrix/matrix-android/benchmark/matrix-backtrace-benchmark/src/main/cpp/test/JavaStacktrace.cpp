
#include "JavaStacktrace.h"

static JavaVM *g_vm;
static jclass m_class_Throwable;
static jmethodID m_method_constructor;

void setJavaVM(JavaVM *vm) {
    g_vm = vm;
    JNIEnv *env;
    g_vm->GetEnv((void **) &env, JNI_VERSION_1_6);
    jclass j_Throwable = env->FindClass("java/lang/Throwable");
    m_class_Throwable = (jclass) env->NewGlobalRef(j_Throwable);
    m_method_constructor = env->GetMethodID(m_class_Throwable, "<init>",
                                            "()V");
}

void java_unwind(jobject &throwable) {
    JNIEnv *env;
    g_vm->GetEnv((void **) &env, JNI_VERSION_1_6);
    throwable = env->NewObject(m_class_Throwable, m_method_constructor);
}

void pin_object(jobject &throwable) {
    JNIEnv *env;
    g_vm->GetEnv((void **) &env, JNI_VERSION_1_6);
    env->NewGlobalRef(throwable);
}