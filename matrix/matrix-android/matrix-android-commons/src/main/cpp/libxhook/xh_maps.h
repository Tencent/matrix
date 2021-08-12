//
// Created by YinSheng Tang on 2021/7/30.
//

#ifndef MATRIX_ANDROID_XH_MAPS_H
#define MATRIX_ANDROID_XH_MAPS_H


#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

    void xh_maps_invalidate();

    void xh_maps_update();

    int xh_maps_query(const void* addr_in, uintptr_t* start_out, uintptr_t* end_out, char** perms_out,
                      int* offset_out, char** pathname_out);

    typedef int (*xh_maps_iterate_cb_t)(void* data, uintptr_t start, uintptr_t end, char* perms,
                                        int offset, char* pathname);
    int xh_maps_iterate(xh_maps_iterate_cb_t cb, void* data);

#ifdef __cplusplus
}
#endif


#endif //MATRIX_ANDROID_XH_MAPS_H
