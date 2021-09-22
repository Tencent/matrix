//
// Created by YinSheng Tang on 2021/7/30.
//

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>
#include "queue.h"
#include "xh_maps.h"
#include "xh_log.h"
#include "xh_errno.h"

typedef struct maps_item {
    uintptr_t start;
    uintptr_t end;
    char perms[4];
    int offset;
    char* pathname;
} maps_item_t;

typedef struct maps_item_list {
    maps_item_t* items;
    size_t count;
} maps_item_list_t;

static maps_item_list_t s_xh_maps_maps_items = { NULL, 0 };
static atomic_bool s_xh_maps_invalidated = ATOMIC_VAR_INIT(false);
static pthread_rwlock_t s_xh_maps_access_lock = PTHREAD_RWLOCK_INITIALIZER;
static __thread bool s_xh_maps_access_locked = false;

static void _xh_lock_read_accesslock()
{
    if (!s_xh_maps_access_locked)
    {
        s_xh_maps_access_locked = true;
        pthread_rwlock_rdlock(&s_xh_maps_access_lock);
    }
}

static void _xh_lock_write_accesslock()
{
    if (!s_xh_maps_access_locked)
    {
        s_xh_maps_access_locked = true;
        pthread_rwlock_wrlock(&s_xh_maps_access_lock);
    }
}

static void _xh_unlock_accesslock()
{
    if (s_xh_maps_access_locked)
    {
        pthread_rwlock_unlock(&s_xh_maps_access_lock);
        s_xh_maps_access_locked = false;
    }
}

void xh_maps_invalidate()
{
    atomic_store(&s_xh_maps_invalidated, false);
}

void xh_maps_update()
{
    _xh_lock_write_accesslock();

    if (atomic_exchange(&s_xh_maps_invalidated, true)) goto bail;

    FILE* f_maps = fopen("/proc/self/maps", "r");
    if (f_maps == NULL) {
        XH_LOG_ABORT("fopen /proc/self/maps failed");
    }

    char line[PATH_MAX] = {};
    int map_item_idx = 0;
    size_t curr_map_item_count = s_xh_maps_maps_items.count;
    while (fgets(line, sizeof(line), f_maps) != NULL)
    {
        uintptr_t start, end;
        char perms[5] = {};
        unsigned long offset = 0;
        int pathname_pos = 0;
        if (sscanf(line, "%" PRIxPTR "-%" PRIxPTR " %4s %lx %*x:%*x %*d%n",
                &start, &end, perms, &offset, &pathname_pos) != 4)
        {
            continue;
        }

        //get pathname
        while (isspace(line[pathname_pos]) && pathname_pos < (int)(sizeof(line) - 1)) pathname_pos += 1;
        if (pathname_pos >= (int)(sizeof(line) - 1)) continue;
        char* pathname = line + pathname_pos;
        size_t pathname_len = strlen(pathname);
        if(0 == pathname_len) continue;
        if(pathname[pathname_len - 1] == '\n')
        {
            pathname[pathname_len - 1] = '\0';
            pathname_len -= 1;
        }
        if(0 == pathname_len) continue;

        pathname = strdup(pathname);
        if (pathname == NULL)
        {
            XH_LOG_ABORT("Fail to allocate memory space to store pathname in maps item.");
        }

        if (s_xh_maps_maps_items.items == NULL || map_item_idx >= curr_map_item_count)
        {
            size_t new_map_item_count = curr_map_item_count + 32;
            s_xh_maps_maps_items.items = realloc(s_xh_maps_maps_items.items, sizeof(maps_item_t) * new_map_item_count);
            if (s_xh_maps_maps_items.items == NULL)
            {
                XH_LOG_ABORT("Fail to expand memory space to store maps items, exp_item_cnt: %zu, exp_size: %zu",
                             new_map_item_count, sizeof(maps_item_t) * new_map_item_count);
            }
            memset(s_xh_maps_maps_items.items + curr_map_item_count, 0,
                   sizeof(maps_item_t) * (new_map_item_count - curr_map_item_count));
            curr_map_item_count = new_map_item_count;
        }
        maps_item_t* new_maps_item = s_xh_maps_maps_items.items + map_item_idx;
        new_maps_item->start = start;
        new_maps_item->end = end;
        strncpy(new_maps_item->perms, perms, 4);
        new_maps_item->offset = offset;
        if (new_maps_item->pathname != NULL) {
            free(new_maps_item->pathname);
        }
        new_maps_item->pathname = pathname;
        ++map_item_idx;
    }
    s_xh_maps_maps_items.count = map_item_idx;
    fclose(f_maps);
    f_maps = NULL;

    bail:
    _xh_unlock_accesslock();
}

int xh_maps_query(const void* addr_in, uintptr_t* start_out, uintptr_t* end_out, char** perms_out, int* offset_out,
                  char** pathname_out)
{
    xh_maps_update();

    _xh_lock_read_accesslock();

    int left = 0;
    int right = s_xh_maps_maps_items.count;
    int found = 0;
    while (left <= right) {
        int mid = ((right - left) >> 1) + left;
        maps_item_t* item = s_xh_maps_maps_items.items + mid;
        if (addr_in < (const void*) item->start) {
            right = mid - 1;
        } else if (addr_in >= (const void*) item->end) {
            left = mid;
        } else {
            if (start_out != NULL) *start_out = item->start;
            if (end_out != NULL) *end_out = item->end;
            if (perms_out != NULL) *perms_out = item->perms;
            if (offset_out != NULL) *offset_out = item->offset;
            if (pathname_out != NULL) *pathname_out = item->pathname;
            found = 1;
            break;
        }
    }

    _xh_unlock_accesslock();

    return found;
}

int xh_maps_iterate(xh_maps_iterate_cb_t cb, void* data)
{
    xh_maps_update();

    _xh_lock_read_accesslock();

    int ret = 0;
    for (int i = 0; i < s_xh_maps_maps_items.count; ++i)
    {
        maps_item_t* item = s_xh_maps_maps_items.items + i;
        ret = cb(data, item->start, item->end, item->perms, item->offset, item->pathname);
        if (ret != 0) break;
    }

    _xh_unlock_accesslock();

    return ret;
}