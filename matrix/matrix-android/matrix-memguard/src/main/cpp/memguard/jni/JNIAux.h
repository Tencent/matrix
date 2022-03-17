//
// Created by tomystang on 2020/11/18.
//

#ifndef __MEMGUARD_JNIAUX_H__
#define __MEMGUARD_JNIAUX_H__


#include <jni.h>
#include <pthread.h>
#include <type_traits>
#include <functional>
#include <util/Auxiliary.h>

namespace memguard {
    namespace jni {
        extern bool InitializeEnv(JavaVM* vm);
        extern JNIEnv* GetEnv();

        template <typename TRef,
                  typename = typename std::enable_if<std::is_convertible<TRef, jobject>::value, void>::type>
        ScopedObject<TRef, std::function<void(TRef)>> MakeScopedLocalRef(TRef ref) {
            auto dtor_lambda = [](TRef ref) {
                auto env = GetEnv();
                if (ref != nullptr && env->GetObjectRefType(ref) == JNILocalRefType) {
                    env->DeleteLocalRef(ref);
                }
            };
            return MakeScopedObject(std::forward<TRef>(ref), std::function<void(TRef)>(dtor_lambda));
        }

        extern void PrintStackTrace(JNIEnv* env, jthrowable thr);
    }
}

#define RETURN_ON_EXCEPTION(env, ret_value) \
  do { \
    if ((env)->ExceptionCheck()) { \
      return (ret_value); \
    } \
  } while (false)

#define JNI_METHOD(name) __JNI_METHOD_1(JNI_CLASS_PREFIX, name)
#define __JNI_METHOD_1(prefix, name) __JNI_METHOD_2(prefix, name)
#define __JNI_METHOD_2(prefix, name) JNIEXPORT Java_##prefix##_##name


#endif //__MEMGUARD_JNIAUX_H__
