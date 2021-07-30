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

void xh_maps_invalidate()
{
    atomic_store(&s_xh_maps_invalidated, false);
}

void xh_maps_update()
{

    if (atomic_exchange(&s_xh_maps_invalidated, true)) return;

    pthread_rwlock_wrlock(&s_xh_maps_access_lock);

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
            curr_map_item_count += 32;
            s_xh_maps_maps_items.items = realloc(s_xh_maps_maps_items.items, sizeof(maps_item_t) * curr_map_item_count);
            if (s_xh_maps_maps_items.items == NULL)
            {
                XH_LOG_ABORT("Fail to expand memory space to store maps items.");
            }
        }
        maps_item_t* new_maps_item = s_xh_maps_maps_items.items + map_item_idx;
        new_maps_item->start = start;
        new_maps_item->end = end;
        strncpy(new_maps_item->perms, perms, 4);
        new_maps_item->offset = offset;
        new_maps_item->pathname = pathname;
        ++map_item_idx;
    }
    s_xh_maps_maps_items.count = map_item_idx;
    fclose(f_maps);
    f_maps = NULL;

    pthread_rwlock_unlock(&s_xh_maps_access_lock);
}

int xh_maps_query(const void* addr_in, uintptr_t* start_out, uintptr_t* end_out, char** perms_out, int* offset_out,
                  char** pathname_out)
{
    if (!atomic_load(&s_xh_maps_invalidated)) xh_maps_update();

    pthread_rwlock_rdlock(&s_xh_maps_access_lock);

    int left = 0;
    int right = s_xh_maps_maps_items.count;
    int found = 0;
    while (left <= right) {
        int mid = ((right - left) >> 1) + left;
        maps_item_t* item = s_xh_maps_maps_items.items + mid;
        if (addr_in < item->start) {
            right = mid - 1;
        } else if (addr_in >= item->end) {
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

    pthread_rwlock_unlock(&s_xh_maps_access_lock);

    return found;
}

int xh_maps_iterate(xh_maps_iterate_cb_t cb, void* data)
{
    if (!atomic_load(&s_xh_maps_invalidated)) xh_maps_update();

    pthread_rwlock_rdlock(&s_xh_maps_access_lock);

    int ret = 0;
    for (int i = 0; i < s_xh_maps_maps_items.count; ++i)
    {
        maps_item_t* item = s_xh_maps_maps_items.items + i;
        ret = cb(data, item->start, item->end, item->perms, item->offset, item->pathname);
        if (ret != 0) break;
    }

    pthread_rwlock_unlock(&s_xh_maps_access_lock);
    return ret;
}