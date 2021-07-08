//
// Created by 邓沛堆 on 2021/1/19.
//

#include <jni.h>
#include "com_tencent_matrix_openglleak_statistics_OpenGLInfo.h"
#include <sstream>
#include <cxxabi.h>
#include "unwindstack/Unwinder.h"
#include "BacktraceDefine.h"
#include "Backtrace.h"

void get_native_stack(wechat_backtrace::Backtrace* backtrace, char *&stack) {
    std::string caller_so_name;
    std::stringstream full_stack_builder;
    std::stringstream brief_stack_builder;
    std::string last_so_name;

    auto _callback = [&](wechat_backtrace::FrameDetail it) {
        std::string so_name = it.map_name;

        char *demangled_name = nullptr;
        int status = 0;
        demangled_name = abi::__cxa_demangle(it.function_name, nullptr, 0, &status);

        full_stack_builder << "      | "
                           << "#pc " << std::hex << it.rel_pc << " "
                           << (demangled_name ? demangled_name : "(null)")
                           << " ("
                           << it.map_name
                           << ")"
                           << std::endl;

        if (last_so_name != it.map_name) {
            last_so_name = it.map_name;
            brief_stack_builder << it.map_name << ";";
        }

        brief_stack_builder << std::hex << it.rel_pc << ";";

        if (demangled_name) {
            free(demangled_name);
        }

        if (caller_so_name.empty()) { // fallback
            if (/*so_name.find("com.tencent.mm") == std::string::npos ||*/
                    so_name.find("libwechatbacktrace.so") != std::string::npos ||
                    so_name.find("libmatrix-hooks.so") != std::string::npos) {
                return;
            }
            caller_so_name = so_name;
        }
    };

    wechat_backtrace::restore_frame_detail(backtrace->frames.get(), backtrace->frame_size,
                                           _callback);

    stack = new char[full_stack_builder.str().size() + 1];
    strcpy(stack, full_stack_builder.str().c_str());
}


extern "C" JNIEXPORT void JNICALL Java_com_tencent_matrix_openglleak_statistics_OpenGLInfo_releaseNative
        (JNIEnv *env, jobject thiz, jlong jl) {
    int64_t addr = jl;

    wechat_backtrace::Backtrace* ptr = (wechat_backtrace::Backtrace*) addr;
    delete ptr;
}

extern "C" JNIEXPORT jstring JNICALL Java_com_tencent_matrix_openglleak_statistics_OpenGLInfo_dumpNativeStack
        (JNIEnv *env, jobject thiz, jlong jl) {
    int64_t addr = jl;
    wechat_backtrace::Backtrace* ptr = (wechat_backtrace::Backtrace*)addr;

    char *native_stack = nullptr;
    get_native_stack(ptr, native_stack);

    return env->NewStringUTF(native_stack);
}