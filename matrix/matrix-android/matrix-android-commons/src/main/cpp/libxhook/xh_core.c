// Copyright (c) 2018-present, iQIYI, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

// Created by caikelun on 2018-04-11.

#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <regex.h>
#include <setjmp.h>
#include <errno.h>
#include <dlfcn.h>
#include <semi_dlfcn.h>
#include "queue.h"
#include "tree.h"
#include "xh_errno.h"
#include "xh_log.h"
#include "xh_elf.h"
#include "xh_version.h"
#include "xh_core.h"
#include "xh_maps.h"

#define XH_CORE_DEBUG 0

//signal handler for SIGSEGV
//for xh_elf_init(), xh_elf_hook(), xh_elf_check_elfheader()
static int              xh_core_sigsegv_enable = 1; //enable by default
static struct sigaction xh_core_sigsegv_act_old;
static volatile int     xh_core_sigsegv_flag = 0;
static sigjmp_buf       xh_core_sigsegv_env;
static void xh_core_sigsegv_handler(int sig)
{
    (void)sig;

    if(xh_core_sigsegv_flag)
        siglongjmp(xh_core_sigsegv_env, 1);
    else
        sigaction(SIGSEGV, &xh_core_sigsegv_act_old, NULL);
}
static int xh_core_add_sigsegv_handler()
{
    struct sigaction act;

    if(!xh_core_sigsegv_enable) return 0;

    if(0 != sigemptyset(&act.sa_mask)) return (0 == errno ? XH_ERRNO_UNKNOWN : errno);
    act.sa_handler = xh_core_sigsegv_handler;

    if(0 != sigaction(SIGSEGV, &act, &xh_core_sigsegv_act_old))
        return (0 == errno ? XH_ERRNO_UNKNOWN : errno);

    return 0;
}
static void xh_core_del_sigsegv_handler()
{
    if(!xh_core_sigsegv_enable) return;

    sigaction(SIGSEGV, &xh_core_sigsegv_act_old, NULL);
}


//registered hook info collection
typedef struct xh_core_hook_info
{
#if XH_CORE_DEBUG
    char     *pathname_regex_str;
#endif
    regex_t   pathname_regex;
    char     *symbol;
    void     *new_func;
    void    **old_func;
    TAILQ_ENTRY(xh_core_hook_info,) link;
} xh_core_hook_info_t;
typedef TAILQ_HEAD(xh_core_hook_info_queue, xh_core_hook_info,) xh_core_hook_info_queue_t;

//ignored hook info collection
typedef struct xh_core_ignore_info
{
#if XH_CORE_DEBUG
    char     *pathname_regex_str;
#endif
    regex_t   pathname_regex;
    char     *symbol; //NULL meaning for all symbols
    TAILQ_ENTRY(xh_core_ignore_info,) link;
} xh_core_ignore_info_t;
typedef TAILQ_HEAD(xh_core_ignore_info_queue, xh_core_ignore_info,) xh_core_ignore_info_queue_t;

//required for grouped requests
typedef struct xh_core_request_group
{
    int group_id;
    xh_core_hook_info_queue_t hook_infos;
    xh_core_ignore_info_queue_t ignore_infos;
    RB_ENTRY(xh_core_request_group) link;
} xh_core_request_group_t;
static __inline__ int xh_core_request_group_cmp(xh_core_request_group_t *a, xh_core_request_group_t *b)
{
    return a->group_id - b->group_id;
}
typedef RB_HEAD(xh_core_request_group_tree, xh_core_request_group) xh_core_request_group_tree_t;
RB_GENERATE_STATIC(xh_core_request_group_tree, xh_core_request_group, link, xh_core_request_group_cmp)

static void xh_core_init_request_group(xh_core_request_group_t* request_group, int group_id)
{
    request_group->group_id = group_id;
    request_group->hook_infos.tqh_first = NULL;
    request_group->hook_infos.tqh_last = &request_group->hook_infos.tqh_first;
    request_group->ignore_infos.tqh_first = NULL;
    request_group->ignore_infos.tqh_last = &request_group->ignore_infos.tqh_first;
}

//required info from /proc/self/maps
typedef struct xh_core_map_info
{
    char*       pathname;
    uintptr_t   bias_addr;
    ElfW(Phdr)* phdrs;
    ElfW(Half)  phdr_count;
    xh_elf_t    elf;
    RB_ENTRY(xh_core_map_info) link;
} xh_core_map_info_t;
static __inline__ int xh_core_map_info_cmp(xh_core_map_info_t *a, xh_core_map_info_t *b)
{
    return strcmp(a->pathname, b->pathname);
}
typedef RB_HEAD(xh_core_map_info_tree, xh_core_map_info) xh_core_map_info_tree_t;
RB_GENERATE_STATIC(xh_core_map_info_tree, xh_core_map_info, link, xh_core_map_info_cmp)

static xh_core_hook_info_queue_t    xh_core_hook_info = TAILQ_HEAD_INITIALIZER(xh_core_hook_info);
static xh_core_ignore_info_queue_t  xh_core_ignore_info = TAILQ_HEAD_INITIALIZER(xh_core_ignore_info);
static xh_core_request_group_tree_t xh_core_request_groups = RB_INITIALIZER(&xh_core_request_groups);
static xh_core_map_info_tree_t      xh_core_map_info = RB_INITIALIZER(&xh_core_map_info);
static pthread_mutex_t              xh_core_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t               xh_core_cond = PTHREAD_COND_INITIALIZER;
static volatile int                 xh_core_inited = 0;
static volatile int                 xh_core_init_ok = 0;
static volatile int                 xh_core_async_inited = 0;
static volatile int                 xh_core_async_init_ok = 0;
static pthread_mutex_t              xh_core_refresh_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_rwlock_t             xh_core_refresh_blocker = PTHREAD_RWLOCK_INITIALIZER;
static pthread_key_t                xh_core_refresh_blocker_tls_key;
static pthread_t                    xh_core_refresh_thread_tid;
static volatile int                 xh_core_refresh_thread_running = 0;
static volatile int                 xh_core_refresh_thread_do = 0;


void xh_core_block_refresh()
{
    int reenter_count = (int) pthread_getspecific(xh_core_refresh_blocker_tls_key);
    pthread_setspecific(xh_core_refresh_blocker_tls_key, (const void*) (long) (reenter_count + 1));
    if (reenter_count == 0)
    {
        pthread_rwlock_wrlock(&xh_core_refresh_blocker);
    }
}

void xh_core_unblock_refresh()
{
    int reenter_count = (int) pthread_getspecific(xh_core_refresh_blocker_tls_key);
    if (reenter_count >= 1)
    {
        --reenter_count;
        pthread_setspecific(xh_core_refresh_blocker_tls_key, (const void*) (long) reenter_count);
    }
    if (reenter_count == 0)
    {
        pthread_rwlock_unlock(&xh_core_refresh_blocker);
    }
}

static int xh_core_register_impl(xh_core_hook_info_queue_t *info_queue,
                                 const char *pathname_regex_str, const char *symbol,
                                 void *new_func, void **old_func)
{
    xh_core_hook_info_t *hi;
    regex_t              regex;

    if(NULL == pathname_regex_str || NULL == symbol || NULL == new_func) return XH_ERRNO_INVAL;

    // if(xh_core_inited)
    // {
    //     XH_LOG_ERROR("do not register hook after refresh(): %s, %s", pathname_regex_str, symbol);
    //     return XH_ERRNO_INVAL;
    // }

    if(0 != regcomp(&regex, pathname_regex_str, REG_NOSUB)) return XH_ERRNO_INVAL;

    if(NULL == (hi = malloc(sizeof(xh_core_hook_info_t)))) return XH_ERRNO_NOMEM;
    if(NULL == (hi->symbol = strdup(symbol)))
    {
        free(hi);
        return XH_ERRNO_NOMEM;
    }
#if XH_CORE_DEBUG
    if(NULL == (hi->pathname_regex_str = strdup(pathname_regex_str)))
    {
        free(hi->symbol);
        free(hi);
        return XH_ERRNO_NOMEM;
    }
#endif
    hi->pathname_regex = regex;
    hi->new_func = new_func;
    hi->old_func = old_func;

    pthread_mutex_lock(&xh_core_mutex);
    TAILQ_INSERT_TAIL(info_queue, hi, link);
    pthread_mutex_unlock(&xh_core_mutex);

    return 0;
}

int xh_core_register(const char *pathname_regex_str, const char *symbol,
                     void *new_func, void **old_func)
{
    return xh_core_register_impl(&xh_core_hook_info, pathname_regex_str, symbol, new_func, old_func);
}

static int xh_core_ignore_impl(xh_core_ignore_info_queue_t *info_queue,
                               const char *pathname_regex_str, const char *symbol)
{
    xh_core_ignore_info_t *ii;
    regex_t                regex;

    if(NULL == pathname_regex_str) return XH_ERRNO_INVAL;

    // if(xh_core_inited)
    // {
    //     XH_LOG_ERROR("do not ignore hook after refresh(): %s, %s", pathname_regex_str, symbol ? symbol : "ALL");
    //     return XH_ERRNO_INVAL;
    // }

    if(0 != regcomp(&regex, pathname_regex_str, REG_NOSUB)) return XH_ERRNO_INVAL;

    if(NULL == (ii = malloc(sizeof(xh_core_ignore_info_t)))) return XH_ERRNO_NOMEM;
    if(NULL != symbol)
    {
        if(NULL == (ii->symbol = strdup(symbol)))
        {
            free(ii);
            return XH_ERRNO_NOMEM;
        }
    }
    else
    {
        ii->symbol = NULL; //ignore all symbols
    }
#if XH_CORE_DEBUG
    if(NULL == (ii->pathname_regex_str = strdup(pathname_regex_str)))
    {
        free(ii->symbol);
        free(ii);
        return XH_ERRNO_NOMEM;
    }
#endif
    ii->pathname_regex = regex;

    pthread_mutex_lock(&xh_core_mutex);
    TAILQ_INSERT_TAIL(info_queue, ii, link);
    pthread_mutex_unlock(&xh_core_mutex);

    return 0;
}

int xh_core_ignore(const char *pathname_regex_str, const char *symbol)
{
    return xh_core_ignore_impl(&xh_core_ignore_info, pathname_regex_str, symbol);
}

int xh_core_grouped_register(int group_id, const char *pathname_regex_str, const char *symbol,
                             void *new_func, void **old_func)
{
    xh_core_request_group_t *group = NULL;

    xh_core_request_group_t group_key = {};
    group_key.group_id = group_id;

    if ((group = RB_FIND(xh_core_request_group_tree, &xh_core_request_groups, &group_key)) != NULL)
    {
        return xh_core_register_impl(&group->hook_infos, pathname_regex_str, symbol, new_func, old_func);
    } else {
        group = (xh_core_request_group_t*) malloc(sizeof(xh_core_request_group_t));
        if (group == NULL) return XH_ERRNO_NOMEM;
        xh_core_init_request_group(group, group_id);
        RB_INSERT(xh_core_request_group_tree, &xh_core_request_groups, group);
        return xh_core_register_impl(&group->hook_infos, pathname_regex_str, symbol, new_func, old_func);
    }
}

int xh_core_grouped_ignore(int group_id, const char *pathname_regex_str, const char *symbol)
{
    xh_core_request_group_t *group = NULL;

    xh_core_request_group_t group_key = {};
    group_key.group_id = group_id;

    if ((group = RB_FIND(xh_core_request_group_tree, &xh_core_request_groups, &group_key)) != NULL)
    {
        return xh_core_ignore_impl(&group->ignore_infos, pathname_regex_str, symbol);
    } else {
        group = (xh_core_request_group_t*) malloc(sizeof(xh_core_request_group_t));
        if (group == NULL) return XH_ERRNO_NOMEM;
        xh_core_init_request_group(group, group_id);
        RB_INSERT(xh_core_request_group_tree, &xh_core_request_groups, group);
        return xh_core_ignore_impl(&group->ignore_infos, pathname_regex_str, symbol);
    }
}

static int xh_core_check_elf_header(uintptr_t base_addr, const char *pathname)
{
    if(!xh_core_sigsegv_enable)
    {
        return xh_elf_check_elfheader(base_addr);
    }
    else
    {
        int ret = XH_ERRNO_UNKNOWN;

        xh_core_sigsegv_flag = 1;
        if(0 == sigsetjmp(xh_core_sigsegv_env, 1))
        {
            ret = xh_elf_check_elfheader(base_addr);
        }
        else
        {
            ret = XH_ERRNO_SEGVERR;
            XH_LOG_WARN("catch SIGSEGV when check_elfheader: %s", pathname);
        }
        xh_core_sigsegv_flag = 0;
        return ret;
    }
}

static void xh_core_hook_impl_with_spec_info(xh_core_map_info_t *mi,
                                             xh_core_hook_info_queue_t *hook_infos,
                                             xh_core_ignore_info_queue_t *ignore_infos)
{
    //hook
    xh_core_hook_info_t   *hi;
    xh_core_ignore_info_t *ii;
    int ignore;
    TAILQ_FOREACH(hi, hook_infos, link) //find hook info
    {
        if(0 == regexec(&(hi->pathname_regex), mi->pathname, 0, NULL, 0))
        {
            ignore = 0;
            TAILQ_FOREACH(ii, ignore_infos, link) //find ignore info
            {
                if(0 == regexec(&(ii->pathname_regex), mi->pathname, 0, NULL, 0))
                {
                    if(NULL == ii->symbol) //ignore all symbols
                        return;

                    if(0 == strcmp(ii->symbol, hi->symbol)) //ignore the current symbol
                    {
                        ignore = 1;
                        break;
                    }
                }
            }

            if(0 == ignore)
                xh_elf_hook(&(mi->elf), hi->symbol, hi->new_func, hi->old_func);
        }
    }
}

static void xh_core_hook_impl(xh_core_map_info_t *mi)
{
    //init
    if(0 != xh_elf_init(&(mi->elf), mi->bias_addr, mi->phdrs, mi->phdr_count, mi->pathname)) return;

    xh_core_hook_impl_with_spec_info(mi, &xh_core_hook_info, &xh_core_ignore_info);

    xh_core_request_group_t *group, *tmp_group;
    RB_FOREACH_SAFE(group, xh_core_request_group_tree, &xh_core_request_groups, tmp_group)
    {
        xh_core_hook_impl_with_spec_info(mi, &group->hook_infos, &group->ignore_infos);
    }
}

static void xh_core_hook(xh_core_map_info_t *mi)
{
    if(!xh_core_sigsegv_enable)
    {
        xh_core_hook_impl(mi);
    }
    else
    {
        xh_core_sigsegv_flag = 1;
        if(0 == sigsetjmp(xh_core_sigsegv_env, 1))
        {
            xh_core_hook_impl(mi);
        }
        else
        {
            XH_LOG_WARN("catch SIGSEGV when init or hook: %s", mi->pathname);
        }
        xh_core_sigsegv_flag = 0;
    }
}

/**
 *
 * @param addr
 * @return If the address specified in addr could not be matched to a shared object, then these functions return 0.
 */
static int xh_check_loaded_so(void *addr) {
    Dl_info stack_info;
    return dladdr(addr, &stack_info);
}

static int is_current_pathname_matches_any_requests(xh_core_hook_info_queue_t *hook_infos,
                                                    xh_core_ignore_info_queue_t *ignore_infos,
                                                    const char *pathname)
{
    xh_core_hook_info_t *hi;
    xh_core_ignore_info_t *ii;
    int match = 0;
    TAILQ_FOREACH(hi, hook_infos, link) //find hook info
    {
        if(0 == regexec(&(hi->pathname_regex), pathname, 0, NULL, 0))
        {
            TAILQ_FOREACH(ii, ignore_infos, link) //find ignore info
            {
                if(0 == regexec(&(ii->pathname_regex), pathname, 0, NULL, 0))
                {
                    if(NULL == ii->symbol)
                        goto check_finished;

                    if(0 == strcmp(ii->symbol, hi->symbol))
                        goto check_continue;
                }
            }

            match = 1;
            break;

            check_continue:
            continue;
        }
    }
    check_finished:
    return match;
}

static int xh_core_maps_iterate_cb(struct dl_phdr_info* info, size_t info_size, void* data)
{
    xh_core_map_info_tree_t* map_info_refreshed = (xh_core_map_info_tree_t*) data;

    const char* pathname = info->dlpi_name;

    //check pathname
    if (pathname[0] == '[') {
        XH_LOG_DEBUG("'%s' is not a lib, skip it.", pathname);
        return 0;
    }

    //if we need to hook this elf?
    if (!is_current_pathname_matches_any_requests(&xh_core_hook_info, &xh_core_ignore_info, pathname))
    {
        XH_LOG_INFO("'%s' does not match default request group, try other groups.", pathname);
        xh_core_request_group_t* req_group;
        xh_core_request_group_t* req_group_tmp;
        RB_FOREACH_SAFE(req_group, xh_core_request_group_tree, &xh_core_request_groups, req_group_tmp)
        {
            XH_LOG_DEBUG("loop group: %d", req_group->group_id);
            if (is_current_pathname_matches_any_requests(&req_group->hook_infos,
                                                         &req_group->ignore_infos, pathname))
            {
                goto check_finished;
            }
            XH_LOG_INFO("'%s' does not match group %d, try other groups.", pathname, req_group->group_id);
        }
        // Does not match any requests, parse next map line.
        return 0;
    }

    check_finished:
    XH_LOG_INFO("'%s' matches hook request, do further checks.", pathname);

    //check existed map item
    xh_core_map_info_t mi_key;
    mi_key.pathname = (char*) pathname;
    xh_core_map_info_t* mi;
    if(NULL != (mi = RB_FIND(xh_core_map_info_tree, &xh_core_map_info, &mi_key)))
    {
        //exist
        RB_REMOVE(xh_core_map_info_tree, &xh_core_map_info, mi);

        //repeated?
        //We only keep the first one, that is the real base address
        if(NULL != RB_INSERT(xh_core_map_info_tree, map_info_refreshed, mi))
        {
            free(mi->pathname);
            free(mi);
            return 0;
        }

        //re-hook if base_addr changed
        if(mi->bias_addr != info->dlpi_addr)
        {
            mi->bias_addr = info->dlpi_addr;
            mi->phdrs = (ElfW(Phdr)*) info->dlpi_phdr;
            mi->phdr_count = info->dlpi_phnum;
            xh_core_hook(mi);
        }
    }
    else
    {
        //not exist, create a new map info
        if(NULL == (mi = (xh_core_map_info_t *) malloc(sizeof(xh_core_map_info_t)))) return 0;
        if(NULL == (mi->pathname = strdup(pathname)))
        {
            free(mi);
            return 0;
        }
        mi->bias_addr = info->dlpi_addr;
        mi->phdrs = (ElfW(Phdr)*) info->dlpi_phdr;
        mi->phdr_count = info->dlpi_phnum;

        //repeated?
        //We only keep the first one, that is the real base address
        if(NULL != RB_INSERT(xh_core_map_info_tree, map_info_refreshed, mi))
        {
            free(mi->pathname);
            free(mi);
            return 0;
        }

        //hook
        xh_core_hook(mi); //hook
    }
    return 0;
}

static void xh_core_refresh_impl()
{
    pthread_rwlock_rdlock(&xh_core_refresh_blocker);

    xh_core_map_info_tree_t map_info_refreshed = RB_INITIALIZER(&map_info_refreshed);
    semi_dl_iterate_phdr((iterate_callback) xh_core_maps_iterate_cb, &map_info_refreshed);

    xh_core_map_info_t* mi;
    xh_core_map_info_t* mi_tmp;

    //free all missing map item, maybe dlclosed?
    RB_FOREACH_SAFE(mi, xh_core_map_info_tree, &xh_core_map_info, mi_tmp)
    {
#if XH_CORE_DEBUG
        XH_LOG_DEBUG("remove missing map info: %s", mi->pathname);
#endif
        RB_REMOVE(xh_core_map_info_tree, &xh_core_map_info, mi);
        if (mi->pathname) free(mi->pathname);
        free(mi);
    }

    //save the new refreshed map info tree
    xh_core_map_info = map_info_refreshed;

    XH_LOG_INFO("map refreshed");

    pthread_rwlock_unlock(&xh_core_refresh_blocker);

#if XH_CORE_DEBUG
    RB_FOREACH(mi, xh_core_map_info_tree, &xh_core_map_info)
        XH_LOG_DEBUG("  %"PRIxPTR" %s\n", mi->base_addr, mi->pathname);
#endif
}

static void *xh_core_refresh_thread_func(void *arg)
{
    (void)arg;

    pthread_setname_np(pthread_self(), "xh_refresh_loop");

    while(xh_core_refresh_thread_running)
    {
        //waiting for a refresh task or exit
        pthread_mutex_lock(&xh_core_mutex);
        while(!xh_core_refresh_thread_do && xh_core_refresh_thread_running)
        {
            pthread_cond_wait(&xh_core_cond, &xh_core_mutex);
        }
        if(!xh_core_refresh_thread_running)
        {
            pthread_mutex_unlock(&xh_core_mutex);
            break;
        }
        xh_core_refresh_thread_do = 0;
        pthread_mutex_unlock(&xh_core_mutex);

        //refresh
        pthread_mutex_lock(&xh_core_refresh_mutex);
        xh_core_refresh_impl();
        pthread_mutex_unlock(&xh_core_refresh_mutex);
    }

    return NULL;
}

static void xh_core_init_once()
{
    if(xh_core_inited) return;

    pthread_mutex_lock(&xh_core_mutex);

    if(xh_core_inited) goto end;

    xh_core_inited = 1;

    //dump debug info
    XH_LOG_INFO("%s\n", xh_version_str_full());
#if XH_CORE_DEBUG
    xh_core_hook_info_t *hi;
    TAILQ_FOREACH(hi, &xh_core_hook_info, link)
        XH_LOG_INFO("  hook: %s @ %s, (%p, %p)\n", hi->symbol, hi->pathname_regex_str,
                    hi->new_func, hi->old_func);
    xh_core_ignore_info_t *ii;
    TAILQ_FOREACH(ii, &xh_core_ignore_info, link)
        XH_LOG_INFO("  ignore: %s @ %s\n", ii->symbol ? ii->symbol : "ALL ",
                    ii->pathname_regex_str);
#endif

    //register signal handler
    if(0 != xh_core_add_sigsegv_handler()) goto end;

    //initialize refresh blocker tls
    if (0 != pthread_key_create(&xh_core_refresh_blocker_tls_key, NULL)) goto end;
    if (0 != pthread_setspecific(xh_core_refresh_blocker_tls_key, (const void*) 0))
    {
        pthread_key_delete(xh_core_refresh_blocker_tls_key);
        goto end;
    }

    //OK
    xh_core_init_ok = 1;

 end:
    pthread_mutex_unlock(&xh_core_mutex);
}

static void xh_core_init_async_once()
{
    if(xh_core_async_inited) return;

    pthread_mutex_lock(&xh_core_mutex);

    if(xh_core_async_inited) goto end;

    xh_core_async_inited = 1;

    //create async refresh thread
    xh_core_refresh_thread_running = 1;
    if(0 != pthread_create(&xh_core_refresh_thread_tid, NULL, &xh_core_refresh_thread_func, NULL))
    {
        xh_core_refresh_thread_running = 0;
        goto end;
    }

    //OK
    xh_core_async_init_ok = 1;

 end:
    pthread_mutex_unlock(&xh_core_mutex);
}

int xh_core_refresh(int async)
{
    //init
    xh_core_init_once();
    if(!xh_core_init_ok) return XH_ERRNO_UNKNOWN;

    if(async)
    {
        //init for async
        xh_core_init_async_once();
        if(!xh_core_async_init_ok) return XH_ERRNO_UNKNOWN;

        //refresh async
        pthread_mutex_lock(&xh_core_mutex);
        xh_core_refresh_thread_do = 1;
        pthread_cond_signal(&xh_core_cond);
        pthread_mutex_unlock(&xh_core_mutex);
    }
    else
    {
        //refresh sync
        pthread_mutex_lock(&xh_core_refresh_mutex);
        xh_core_refresh_impl();
        pthread_mutex_unlock(&xh_core_refresh_mutex);
    }

    return 0;
}

void xh_core_clear()
{
    //stop the async refresh thread
    if(xh_core_async_init_ok)
    {
        pthread_mutex_lock(&xh_core_mutex);
        xh_core_refresh_thread_running = 0;
        pthread_cond_signal(&xh_core_cond);
        pthread_mutex_unlock(&xh_core_mutex);

        pthread_join(xh_core_refresh_thread_tid, NULL);
        xh_core_async_init_ok = 0;
    }
    xh_core_async_inited = 0;

    //unregister the sig handler
    if(xh_core_init_ok)
    {
        xh_core_del_sigsegv_handler();
        xh_core_init_ok = 0;
    }
    xh_core_inited = 0;

    pthread_mutex_lock(&xh_core_mutex);
    pthread_mutex_lock(&xh_core_refresh_mutex);

    //free all map info
    xh_core_map_info_t *mi, *mi_tmp;
    RB_FOREACH_SAFE(mi, xh_core_map_info_tree, &xh_core_map_info, mi_tmp)
    {
        RB_REMOVE(xh_core_map_info_tree, &xh_core_map_info, mi);
        if(mi->pathname) free(mi->pathname);
        free(mi);
    }

    //free all hook info
    xh_core_hook_info_t *hi, *hi_tmp;
    TAILQ_FOREACH_SAFE(hi, &xh_core_hook_info, link, hi_tmp)
    {
        TAILQ_REMOVE(&xh_core_hook_info, hi, link);
#if XH_CORE_DEBUG
        free(hi->pathname_regex_str);
#endif
        regfree(&(hi->pathname_regex));
        free(hi->symbol);
        free(hi);
    }

    //free all ignore info
    xh_core_ignore_info_t *ii, *ii_tmp;
    TAILQ_FOREACH_SAFE(ii, &xh_core_ignore_info, link, ii_tmp)
    {
        TAILQ_REMOVE(&xh_core_ignore_info, ii, link);
#if XH_CORE_DEBUG
        free(ii->pathname_regex_str);
#endif
        regfree(&(ii->pathname_regex));
        free(ii->symbol);
        free(ii);
    }

    pthread_mutex_unlock(&xh_core_refresh_mutex);
    pthread_mutex_unlock(&xh_core_mutex);
}

void xh_core_enable_debug(int flag)
{
    xh_log_priority = (flag ? ANDROID_LOG_DEBUG : ANDROID_LOG_WARN);
}

void xh_core_enable_sigsegv_protection(int flag)
{
    xh_core_sigsegv_enable = (flag ? 1 : 0);
}

typedef struct xh_single_so_iterate_args {
    const char* path_suffix;
    xh_core_map_info_t* mi;
} xh_single_so_iterate_args_t;

static int xh_single_so_search_iterate_cb(struct dl_phdr_info* info, size_t info_size, void* data) {
    xh_single_so_iterate_args_t* args = (xh_single_so_iterate_args_t*) data;

    const char* pathname = info->dlpi_name;
    size_t path_len = strlen(pathname);
    size_t path_suffix_len = strlen(args->path_suffix);
    if (strncmp(pathname + path_len - path_suffix_len, args->path_suffix, path_suffix_len) != 0) {
        // Continue to process next entry.
        return 0;
    }

    int check_elf_ret = xh_core_check_elf_header(info->dlpi_addr, pathname);
    if (0 != check_elf_ret) {
        XH_LOG_ERROR("Fail to check elf header: %s, ret: %d.", pathname, check_elf_ret);
        // Continue to process next entry.
        return 0;
    }

    args->mi->pathname = strdup(pathname);
    if (args->mi->pathname == NULL) {
        XH_LOG_ERROR("Fail to allocate memory to store path: %s.", pathname);
        return -1;
    }
    args->mi->bias_addr = info->dlpi_addr;
    args->mi->phdrs = (ElfW(Phdr)*) info->dlpi_phdr;
    args->mi->phdr_count = info->dlpi_phnum;
    return 1;
}

void* xh_core_elf_open(const char *path_suffix) {
    if (path_suffix == NULL) {
        XH_LOG_ERROR("path_suffix is null.");
        return NULL;
    }
    xh_core_map_info_t* mi = malloc(sizeof(xh_core_map_info_t));
    if (mi == NULL) {
        XH_LOG_ERROR("Fail to allocate memory.");
        return NULL;
    }
    memset(mi, 0, sizeof(xh_core_map_info_t));

    xh_single_so_iterate_args_t iter_args = {
            .path_suffix = path_suffix,
            .mi = mi
    };
    int iter_ret = semi_dl_iterate_phdr(xh_single_so_search_iterate_cb, &iter_args);
    if (iter_ret > 0) {
        XH_LOG_INFO("Open so with path suffix %s successfully, realpath: %s.", path_suffix, mi->pathname);
        return mi;
    } else {
        if (mi->pathname != NULL) {
            free(mi->pathname);
            mi->pathname = NULL;
        }
        free(mi);
        XH_LOG_ERROR("Fail to open %s", path_suffix);
        return NULL;
    }
}

static int xh_core_hook_single_sym_impl(xh_core_map_info_t *mi, const char *symbol, void *new_func,
                                        void **old_func)
{
    int ret;

    if (mi == NULL || symbol == NULL || new_func == NULL)
    {
        return XH_ERRNO_INVAL;
    }

    //init
    ret = xh_elf_init(&(mi->elf), mi->bias_addr, mi->phdrs, mi->phdr_count, mi->pathname);
    if (ret != 0)
    {
        return ret;
    }

    //hook single.
    return xh_elf_hook(&(mi->elf), symbol, new_func, old_func);
}

int xh_core_got_hook_symbol(void* h_lib, const char* symbol, void* new_func, void** old_func) {
    int ret;
    xh_core_map_info_t* mi = (xh_core_map_info_t*) h_lib;

    if(!xh_core_sigsegv_enable)
    {
        return xh_core_hook_single_sym_impl(mi, symbol, new_func, old_func);
    }
    else
    {
        ret = XH_ERRNO_UNKNOWN;
        xh_core_sigsegv_flag = 1;
        if(0 == sigsetjmp(xh_core_sigsegv_env, 1))
        {
            ret = xh_core_hook_single_sym_impl(mi, symbol, new_func, old_func);
        }
        else
        {
            ret = XH_ERRNO_SEGVERR;
            XH_LOG_WARN("catch SIGSEGV when init or hook: %s", mi->pathname);
        }
        xh_core_sigsegv_flag = 0;
        return ret;
    }
}

void xh_core_elf_close(void *h_lib) {
    if (h_lib == NULL) return;

    xh_core_map_info_t* mi = h_lib;
    if (mi->pathname != NULL) {
        free(mi->pathname);
    }

    free(mi);
}