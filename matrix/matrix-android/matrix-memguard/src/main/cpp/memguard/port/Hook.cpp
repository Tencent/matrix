//
// Created by tomystang on 2020/10/15.
//

#include <string>
#include <xhook_ext.h>
#include <util/Auxiliary.h>
#include <util/Hook.h>
#include <util/Log.h>
#include <HookCommon.h>

using namespace memguard;

#define LOG_TAG "MemGuard.Hook"

bool memguard::BeginHook() {
    LOGD(LOG_TAG, "BeginHook()");
    return true;
}

bool memguard::DoHook(int rule_group_id, const char* pathname_regex, const char* sym_name, void* handler_func, void** original_func) {
    int ret = xhook_grouped_register(rule_group_id, pathname_regex, sym_name, handler_func, original_func);
    if (UNLIKELY(ret != 0)) {
        LOGE(LOG_TAG, "Fail to hook symbol '%s' of libs match pattern '%s', ret: %d", sym_name, pathname_regex, ret);
        return false;
    }
    LOGD(LOG_TAG, "Success! DoHook(%s, %s, %p, %p)", pathname_regex, sym_name, handler_func, original_func);
    return true;
}

bool memguard::EndHook(int rule_group_id, const std::vector<std::string>& ignore_pathname_regex_list) {
    int ret = 0;
    for (auto & pattern : ignore_pathname_regex_list) {
        if ((ret = xhook_grouped_ignore(rule_group_id, pattern.c_str(), nullptr)) != 0) {
            LOGE(LOG_TAG, "Fail to ignore all symbols in library matches pattern %s, ret: %d", pattern.c_str(), ret);
            return false;
        }
    }
    if ((ret = xhook_grouped_ignore(rule_group_id, ".*/libmatrix-memguard\\.so$", nullptr)) != 0) {
        LOGE(LOG_TAG, "Fail to ignore all symbols in libmemguard.so, ret: %d", ret);
        return false;
    }
#if defined(__LP64__)
    if ((ret = xhook_grouped_ignore(rule_group_id, ".*/linker64$", nullptr)) != 0) {
        LOGE(LOG_TAG, "Fail to ignore all symbols in linker64, ret: %d", ret);
        return false;
    }
#else
    if ((ret = xhook_grouped_ignore(rule_group_id, ".*/linker$", nullptr)) != 0) {
        LOGE(LOG_TAG, "Fail to ignore all symbols in linker, ret: %d", ret);
        return false;
    }
#endif
    if ((ret = xhook_grouped_ignore(rule_group_id, ".*/libc\\.so$", nullptr)) != 0) {
        LOGE(LOG_TAG, "Fail to ignore all symbols in libc.so, ret: %d", ret);
        return false;
    }
    if ((ret = xhook_grouped_ignore(rule_group_id, ".*/libwechatcrash\\.so$", nullptr)) != 0) {
        LOGE(LOG_TAG, "Fail to ignore all symbols in libwechatcrash.so, ret: %d", ret);
        return false;
    }
    if ((ret = xhook_grouped_ignore(rule_group_id, ".*/liblog\\.so$", nullptr)) != 0) {
        LOGE(LOG_TAG, "Fail to ignore all symbols in liblog.so, ret: %d", ret);
        return false;
    }
    if ((ret = xhook_grouped_ignore(rule_group_id, ".*/libmatrix-hookcommon\\.so$", nullptr)) != 0) {
        LOGE(LOG_TAG, "Fail to ignore all symbols in libmatrix-hookcommon.so, ret: %d", ret);
        return false;
    }
    if ((ret = xhook_grouped_ignore(rule_group_id, ".*/libwechatbacktrace\\.so$", nullptr)) != 0) {
        LOGE(LOG_TAG, "Fail to ignore all symbols in libwechatbacktrace.so, ret: %d", ret);
        return false;
    }
    xhook_enable_debug(0);
    xhook_enable_sigsegv_protection(1);
    if (!UpdateHook()) {
        return false;
    }
    LOGD(LOG_TAG, "EndHook()");
    return true;
}

bool memguard::UpdateHook() {
    int ret = 0;
    if ((ret = xhook_refresh(0)) != 0) {
        LOGE(LOG_TAG, "Fail to call xhook_refresh, ret: %d", ret);
        return false;
    }
    LOGD(LOG_TAG, "UpdateHook()");
    return true;
}