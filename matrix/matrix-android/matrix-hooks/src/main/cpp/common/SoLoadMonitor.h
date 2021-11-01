//
// Created by YinSheng Tang on 2021/7/8.
//

#ifndef MATRIX_ANDROID_SOLOADMONITOR_H
#define MATRIX_ANDROID_SOLOADMONITOR_H


#include "Macros.h"

namespace matrix {
    typedef void (*so_load_callback_t)(const char *__file_name);

    EXPORT bool InstallSoLoadMonitor();
    EXPORT void AddOnSoLoadCallback(so_load_callback_t cb);
    EXPORT void PauseLoadSo();
    EXPORT void ResumeLoadSo();
}


#endif //MATRIX_ANDROID_SOLOADMONITOR_H
