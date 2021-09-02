//
// Created by YinSheng Tang on 2021/6/19.
//

#ifndef MATRIX_ANDROID_MAPS_H
#define MATRIX_ANDROID_MAPS_H


#include <functional>
#include "Macros.h"

namespace matrix {
    typedef std::function<bool(uintptr_t, uintptr_t, char[4], const char*, void*)> MapsEntryCallback;

    EXPORT bool IterateMaps(const MapsEntryCallback& cb, void* args = nullptr);
}


#endif //MATRIX_ANDROID_MAPS_H
