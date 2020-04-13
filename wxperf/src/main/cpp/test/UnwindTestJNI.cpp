//
// Created by yves on 20-4-11.
//

#include <jni.h>
#include <dlfcn.h>
#include <cinttypes>
#include <cxxabi.h>
#include "log.h"
#include "StackTrace.h"

#ifdef __cplusplus
extern "C" {
#endif

void func1() {
    LOGD("Yves-test", "func1");

    LOGD("Unwind-test", "unwind begin");

    auto *tmp_ns = new std::vector<unwindstack::FrameData>;

    unwindstack::do_unwind(tmp_ns);

    for (auto p_frame = tmp_ns->begin(); p_frame != tmp_ns->end(); ++p_frame) {
        Dl_info stack_info;
        dladdr((void *) p_frame->pc, &stack_info);

        std::string so_name = std::string(stack_info.dli_fname);

        int status = 0;
        char *demangled_name = abi::__cxa_demangle(stack_info.dli_sname, nullptr, 0,
                                                   &status);

        LOGD("Unwind-debug", "  #pc %"
                PRIxPTR
                " %lu %lx %s (%s)", p_frame->rel_pc, p_frame->pc, p_frame->pc,
        /*demangled_name ? demangled_name : "(null)",*/ stack_info.dli_sname, stack_info.dli_fname);
    }

    LOGD("Unwind-test", "unwind done");
}

void func0() {
    LOGD("Yves-test", "func0");

    int j = 0;

    int k = 0;

    int l =0;

    long m = 0;

    for (int i = 0; i < 100; ++i) {
        LOGD("Yves-test", "j = %d, k = %d, l%d, m%ld", j++, k, l, m);
        k+=2;
        l += 3;
        m += 4;
    }

    func1();
}


JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_test_UnwindTest_testNative(JNIEnv *env, jclass clazz) {
    func0();
}

void func3(JNIEnv *env, jclass clazz) {
    LOGD("Yves-test", "func3");
    jmethodID j_test = env->GetStaticMethodID(clazz, "test", "()V");
    env->CallStaticVoidMethod(clazz, j_test);
}

void func2(JNIEnv *env, jclass clazz) {
    LOGD("Yves-test", "func2");
    func3(env, clazz);
}

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_test_UnwindTest_testNative2(JNIEnv *env, jclass clazz) {
    func2(env, clazz);
}

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_test_UnwindTest_initNative(JNIEnv *env, jclass clazz) {
    LOGD("Yves-test", "Unwind init");
    unwindstack::update_maps();
}

#ifdef __cplusplus
}
#endif