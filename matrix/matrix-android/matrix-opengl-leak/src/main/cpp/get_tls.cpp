//
// Created by 邓沛堆 on 2020-05-28.
//

#include "get_tls.h"
#include <sys/system_properties.h>
#include <stdlib.h>

gl_hooks_t *get_gl_hooks() {
    // 获取 TLS
    volatile void *tls_base = __get_tls();

    gl_hooks_t *volatile *tls_hooks =
            reinterpret_cast<gl_hooks_t *volatile *>(tls_base);
    gl_hooks_t *hooks = NULL;

    // android >= 10 TLS 位置有变化
    char sdk[128] = "0";
    __system_property_get("ro.build.version.sdk", sdk);
    int sdk_version = atoi(sdk);

    if (sdk_version >= 29) {
        // android 10
        hooks = tls_hooks[4];
    } else {
        hooks = tls_hooks[3];
    }

    return hooks;
}