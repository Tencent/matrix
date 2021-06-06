
#include "JavaStacktrace.h"

static JavaVM *g_vm;
static jclass m_class_Throwable;
static jmethodID m_method_constructor;
static jclass m_class_StacktraceHelper;
static jmethodID m_method_getStackTraces;

JavaVM *getJavaVM() {
    return g_vm;
}

void setJavaVM(JavaVM *vm) {
    g_vm = vm;
    JNIEnv *env;
    g_vm->GetEnv((void **) &env, JNI_VERSION_1_6);
    jclass j_Throwable = env->FindClass("java/lang/Throwable");
    m_class_Throwable = (jclass) env->NewGlobalRef(j_Throwable);
    m_method_constructor = env->GetMethodID(m_class_Throwable, "<init>",
                                            "()V");
    jclass j_StacktraceHelper = env->FindClass("com/tencent/matrix/benchmark/test/StacktraceHelper");
    m_class_StacktraceHelper = (jclass) env->NewGlobalRef(j_StacktraceHelper);
    m_method_getStackTraces = env->GetStaticMethodID(m_class_StacktraceHelper, "getStackTraces",
                                            "()[Ljava/lang/String;");
}

void java_unwind(jobject &throwable) {
    JNIEnv *env;
    g_vm->GetEnv((void **) &env, JNI_VERSION_1_6);
    throwable = env->NewObject(m_class_Throwable, m_method_constructor);
}

void java_get_stack_traces(JNIEnv *env, jobjectArray &traces) {
    traces = static_cast<jobjectArray>(env->CallStaticObjectMethod(m_class_StacktraceHelper,
                                                                   m_method_getStackTraces));
}

void pin_object(jobject &throwable) {
    JNIEnv *env;
    g_vm->GetEnv((void **) &env, JNI_VERSION_1_6);
    env->NewGlobalRef(throwable);
}