//
// Created by YinSheng Tang on 2021/7/21.
//

#include "sd_log.h"

#ifdef EnableLOG
bool g_semi_dlfcn_log_enabled = true;
#else
bool g_semi_dlfcn_log_enabled = false;
#endif

int g_semi_dlfcn_log_level = ANDROID_LOG_DEBUG;