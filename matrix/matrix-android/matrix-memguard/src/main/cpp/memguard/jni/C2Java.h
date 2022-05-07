//
// Created by tomystang on 2021/2/1.
//

#ifndef __MEMGUARD_C2JAVA_H__
#define __MEMGUARD_C2JAVA_H__


#include <jni.h>

namespace memguard {
    namespace c2j {
        extern bool Prepare(jclass memguard_clazz);
        extern void NotifyOnIssueDumpped(const char* dump_file);
    }
}


#endif //__MEMGUARD_C2JAVA_H__
