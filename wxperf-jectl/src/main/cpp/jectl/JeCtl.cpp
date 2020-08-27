//
// Created by Yves on 2020/7/15.
//

#include <jni.h>
#include <dlfcn.h>
#include <unistd.h>
#include <asm/mman.h>
#include <sys/mman.h>
#include "EnhanceDlsym.h"
#include "JeLog.h"
#include "JeHooks.h"
#include <stdatomic.h>
//#include "internal/arena_structs_b.h"

// 必须和 java 保持一致
#define JECTL_OK            0
#define ERR_INIT_FAILED     1
#define ERR_VERSION         2
#define ERR_64_BIT          3
#define ERR_CTL             4

#define CACHELINE        64

#define TAG "Wxperf.JeCtl"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*mallctl_t)(const char *name,
                         void *oldp,
                         size_t *oldlenp,
                         void *newp,
                         size_t newlen);

typedef void *(*arena_extent_alloc_large_t)(void *tsdn,
                                            void *arena,
                                            size_t usize,
                                            size_t alignment,
                                            bool *zero);

typedef void (*large_dalloc_t)(void *tsdn, void *extent);

typedef void *(*arena_choose_hard_t)(void *tsd, bool internal);

typedef void (*arena_extent_dalloc_large_prep_t)(void *tsdn, void *arena, void *extent);

typedef void (*arena_extents_dirty_dalloc_t)(void *tsdn, void *arena,
                                             extent_hooks_t **r_extent_hooks, void *extent);

#define MAX_RETRY_TIMES 10

void   *handle     = nullptr;
bool   initialized = false;

mallctl_t                        mallctl                        = nullptr;
arena_extent_alloc_large_t       arena_extent_alloc_large       = nullptr;
large_dalloc_t                   large_dalloc                   = nullptr;
arena_choose_hard_t              arena_choose_hard              = nullptr;
arena_extent_dalloc_large_prep_t arena_extent_dalloc_large_prep = nullptr;
arena_extents_dirty_dalloc_t     arena_extents_dirty_dalloc     = nullptr;


static inline bool end_with(std::string const &value, std::string const &ending) {
    if (ending.size() > value.size()) {
        return false;
    }
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

static bool init() {

    handle = enhance::dlopen("libc.so", 0);

    if (handle == nullptr) {
        return false;
    }

    mallctl = (mallctl_t) enhance::dlsym(handle, "je_mallctl");

    if (!mallctl) {
        return false;
    }

    const char *version;
    size_t     size = sizeof(version);
    mallctl("version", &version, &size, nullptr, 0);
    LOGD(TAG, "jemalloc version: %s", version);

    if (0 != strncmp(version, "5.1.0", 5)) {
        return false;
    }

//    Dl_info mallctl_info{};
//    if (0 == dladdr((void *) mallctl, &mallctl_info)
//        || !end_with(mallctl_info.dli_fname, "/libc.so")) {
//        LOGD(TAG, "mallctl = %p, is a fault address, fname = %s, sname = %s", mallctl,
//             mallctl_info.dli_fname, mallctl_info.dli_sname);
//        mallctl = nullptr;
//        return false;
//    }

//    opt_retain = (bool *) enhance::dlsym(handle, "je_opt_retain");
//    if (!opt_retain) {
//        return false;
//    }

    arena_extent_alloc_large =
            (arena_extent_alloc_large_t) enhance::dlsym(handle, "je_arena_extent_alloc_large");
    if (!arena_extent_alloc_large) {
        return false;
    }

    arena_choose_hard = (arena_choose_hard_t) enhance::dlsym(handle, "je_arena_choose_hard");
    if (!arena_choose_hard) {
        return false;
    }

    large_dalloc = (large_dalloc_t) enhance::dlsym(handle, "je_large_dalloc");
    if (!large_dalloc) {
        return false;
    }

    arena_extent_dalloc_large_prep = (arena_extent_dalloc_large_prep_t) enhance::dlsym(handle,
                                                                                       "je_arena_extent_dalloc_large_prep");
    if (!arena_extent_dalloc_large_prep) {
        return false;
    }

    arena_extents_dirty_dalloc = (arena_extents_dirty_dalloc_t) enhance::dlsym(handle,
                                                                               "je_arena_extents_dirty_dalloc");
    if (!arena_extents_dirty_dalloc) {
        return false;
    }

    return true;
}

static void flush_decay_purge() {
    assert(mallctl != nullptr);
    mallctl("thread.tcache.flush", nullptr, nullptr, nullptr, 0);
    mallctl("arena.0.decay", nullptr, nullptr, nullptr, 0);
    mallctl("arena.1.decay", nullptr, nullptr, nullptr, 0);
    mallctl("arena.0.purge", nullptr, nullptr, nullptr, 0);
    mallctl("arena.1.purge", nullptr, nullptr, nullptr, 0);
}

static void enable_dss() {
    int err              = 0;

    char   *stat_dss;
    size_t stat_dss_size = sizeof(char *);
    mallctl("stats.arenas.0.dss", &stat_dss, &stat_dss_size, nullptr, 0);
    LOGD(TAG, "stat_dss.0 = %s", stat_dss);

    mallctl("stats.arenas.1.dss", &stat_dss, &stat_dss_size, nullptr, 0);
    LOGD(TAG, "stat_dss.1 = %s", stat_dss);

//    *opt_retain = false;
    const char *setting         = "primary";
    size_t     setting_size     = sizeof(const char);
    char       *old_setting;
    size_t     old_setting_size = sizeof(char *);
    mallctl("arena.0.dss", &old_setting, &old_setting_size, &setting, sizeof(const char *));
    mallctl("arena.1.dss", &old_setting, &old_setting_size, &setting, 8);

    mallctl("stats.arenas.0.dss", &stat_dss, &stat_dss_size, nullptr, 0);
    LOGD(TAG, "after stat_dss.0 = %s", stat_dss);

    mallctl("stats.arenas.1.dss", &stat_dss, &stat_dss_size, nullptr, 0);
    LOGD(TAG, "after stat_dss.1 = %s", stat_dss);

    bool   old_val;
    size_t old_val_size         = sizeof(bool);
    bool   new_val              = false;
    mallctl("thread.tcache.enabled", &old_val, &old_val_size, &new_val, sizeof(new_val));
    LOGD(TAG, "thread.tcache.enabled: %d", old_val);
    mallctl("thread.tcache.enabled", &old_val, &old_val_size, nullptr, 0);
    LOGD(TAG, "thread.tcache.enabled: %d", old_val);

    ssize_t dirty_ms;
    size_t  dirty_ms_size       = sizeof(ssize_t);
    ssize_t new_dirty_ms        = 0;
    mallctl("arena.0.dirty_decay_ms", &dirty_ms, &dirty_ms_size, &new_dirty_ms, sizeof(ssize_t));
    LOGD(TAG, "arena.0.dirty_decay_ms: %zu", dirty_ms);
    mallctl("arena.0.dirty_decay_ms", &dirty_ms, &dirty_ms_size, nullptr, 0);
    LOGD(TAG, "arena.0.dirty_decay_ms: %zu", dirty_ms);

    bool   background;
    size_t background_size      = sizeof(bool);
    bool   new_background       = true;
    err = mallctl("background_thread", &background, &background_size, &new_background,
                  background_size);
    LOGD(TAG, "background_thread = %d, err = %d", background, err);
    err                        = mallctl("background_thread", &background, &background_size,
                                         nullptr, 0);
    LOGD(TAG, "background_thread = %d, err = %d", background, err);

    bool   opt_background;
    size_t opt_background_size = sizeof(bool);
    mallctl("opt.background_thread", &opt_background, &opt_background_size, nullptr, 0);
    LOGD(TAG, "opt.background_thread = %d", opt_background);


}

JNIEXPORT void JNICALL
Java_com_tencent_wxperf_jectl_JeCtl_initNative(JNIEnv *env, jclass clazz) {
#ifdef __LP64__
    return ;
#else
    if (!initialized) {
        initialized = init();
    }
#endif
}

JNIEXPORT jint JNICALL
Java_com_tencent_wxperf_jectl_JeCtl_compactNative(JNIEnv *env, jclass clazz) {

#ifdef __LP64__
    return ERR_64_BIT;
#else

    if (!initialized) {
        return ERR_INIT_FAILED;
    }
    assert(mallctl != nullptr);

    flush_decay_purge();

    return JECTL_OK;
#endif
}

JNIEXPORT jint JNICALL
Java_com_tencent_wxperf_jectl_JeCtl_extentHookTest(JNIEnv *env, jclass clazz) {

#ifdef __LP64__
    return ERR_64_BIT;
#else

    if (!initialized) {
        return ERR_INIT_FAILED;
    }

//    bool old_val;
//    size_t old_val_size = sizeof(bool);
//    bool new_val = false;
//    mallctl("thread.tcache.enabled", &old_val, &old_val_size, &new_val, sizeof(new_val));
//    LOGD(TAG, "thread.tcache.enabled: %d", old_val);
//    mallctl("thread.tcache.enabled", &old_val, &old_val_size, nullptr, 0);
//    LOGD(TAG, "thread.tcache.enabled: %d", old_val);

    size_t         extent_hooks_size = sizeof(extent_hooks_t *);
    extent_hooks_t *new_hook         = &extent_hooks;

    int ret = mallctl("arena.0.extent_hooks", &origin_extent_hooks,
                      &extent_hooks_size, /*nullptr, 0*/
                      &new_hook,
                      extent_hooks_size);
    LOGD(TAG, "arena.0.extent_hooks: default %p ret = %d", origin_extent_hooks, ret);
    ret = mallctl("arena.1.extent_hooks", &origin_extent_hooks, &extent_hooks_size, /*nullptr, 0*/
                  &new_hook, extent_hooks_size);
    LOGD(TAG, "arena.1.extent_hooks: default %p ret = %d", origin_extent_hooks, ret);

//    extent_hooks_t *hooks;
//    ret = mallctl("arena.0.extent_hooks", &hooks, &extent_hooks_size, nullptr, 0);
//    LOGD(TAG, "arena.0.extent_hooks: default %p ret = %d", hooks, ret);
//    ret = mallctl("arena.0.extent_hooks", &hooks, &extent_hooks_size, nullptr, 0);
//    LOGD(TAG, "arena.0.extent_hooks: default %p ret = %d", hooks, ret);

//    mprotect((void *) PAGE_START((uintptr_t) origin_extent_hooks),
//             PAGE_COVER((uintptr_t) origin_extent_hooks),
//             PROT_READ | PROT_WRITE);
//
//    original_split = origin_extent_hooks->split;
//    origin_extent_hooks->split = extent_hooks.split;

#endif
    LOGD(TAG,
         "origin alloc %p, dalloc %p, destroy %p, commit %p, decommit %p, purge_lazy %p, purge_forced %p, split %p, merge %p",
         origin_extent_hooks->alloc, origin_extent_hooks->dalloc, origin_extent_hooks->destroy,
         origin_extent_hooks->commit, origin_extent_hooks->decommit,
         origin_extent_hooks->purge_lazy, origin_extent_hooks->purge_forced,
         origin_extent_hooks->split, origin_extent_hooks->merge);

    LOGD(TAG,
         "our hook: %p alloc %p, dalloc %p, destroy %p, commit %p, decommit %p, purge_lazy %p, purge_forced %p, split %p, merge %p",
         &extent_hooks, extent_hooks.alloc, extent_hooks.dalloc, extent_hooks.destroy,
         extent_hooks.commit, extent_hooks.decommit, extent_hooks.purge_lazy,
         extent_hooks.purge_forced, extent_hooks.split, extent_hooks.merge);

//    void *npe = (void *)0x00000078;
//    LOGD(TAG, "NPE : %d", npe == nullptr);

//    stub_alloc(nullptr);

//    pthread_t pthread[100];
//    for (int i = 0; i < 100;++i) {
//        pthread_create(&pthread[i], nullptr, stub_alloc, nullptr);
//    }
//    for (int i = 0; i < 10; ++i) {
//        pthread_join(pthread[i], nullptr);
//    }

//    LOGD(TAG, "all thread finished: %d", arena_1);

//    void *p = malloc(100 * 1024 * 1024);
//    memset(p, 'a', 1024 * 1024);
//    free(p);

    return JECTL_OK;
}

static void call_alloc_large_in_arena1(size_t __size) {
    // 延迟时机, 失败也不影响 arena0 的预分配
    auto tsd_tsd = (pthread_t *) enhance::dlsym(handle, "je_tsd_tsd");
    if (!tsd_tsd) {
        LOGE(TAG, "tsd_tsd not found");
        return;
    }

    auto tsd = pthread_getspecific(*tsd_tsd);
    if (tsd == nullptr) {
        LOGE(TAG, "tsd id null");
        return;
    }

    void *arena1 = arena_choose_hard(tsd, false); // choose 另一个 arena

    unsigned which_arena      = 0;
    size_t   which_arena_size = sizeof(unsigned);
    mallctl("thread.arena", &which_arena, &which_arena_size, nullptr, 0);

    bool zero = false;
    LOGD(TAG, "args : tsd=%p, arena1=%p, size=%zu, align=%d, zero=%p", tsd, (void *) arena1, __size,
         CACHELINE, &zero);
    void *extent = arena_extent_alloc_large(tsd, (void *) arena1, __size, CACHELINE, &zero);
//    large_dalloc(tsd, extent);

    arena_extent_dalloc_large_prep(tsd, arena1, extent);
    extent_hooks_t *hooks = nullptr;
    arena_extents_dirty_dalloc(tsd, arena1, &hooks, extent);
}

static void *sub_routine(void *__arg) {
    LOGD(TAG, "arg = %zu ", *((size_t *) __arg));

    void *p = malloc(1024);//  确保当前线程的 tsd 已经初始化

    unsigned which_arena      = 0;
    size_t   which_arena_size = sizeof(unsigned);

    int ret = mallctl("thread.arena", &which_arena, &which_arena_size, nullptr,
                      0);
    LOGD(TAG, "thread.arena: which_arena = %u, ret = %d", which_arena, ret);

    if (which_arena == 0) {
        call_alloc_large_in_arena1(*(size_t *) __arg);
        free(p);
        free(__arg);
        flush_decay_purge();
        return nullptr;
    }

    pthread_t next_thread;
    pthread_create(&next_thread, nullptr, sub_routine, __arg);

    pthread_join(next_thread, nullptr);

    free(p);
    return nullptr;
}

// Android: Force all huge allocations to always take place in the first arena.
// see: https://cs.android.com/android/platform/superproject/+/android-10.0.0_r30:external/jemalloc_new/src/large.c;l=46
// so we have to hack jemalloc to make sure that the large allocation takes place in second arena
static int hack_prealloc_within_arena1(size_t __size) {
    LOGD(TAG, "hack_arena1");
    if (!initialized) {
        LOGD(TAG, "hack_arena1:init fialed");
        return ERR_INIT_FAILED;
    }

    LOGD(TAG, "size1 = %zu", __size);
    auto size = (size_t *) malloc(sizeof(size_t));
    *size = __size;

    pthread_t next_thread;
    pthread_create(&next_thread, nullptr, sub_routine, size);

    return JECTL_OK;
}

static void *alloc_routine(void *arg) {

    unsigned which_arena      = 0;
    size_t   which_arena_size = sizeof(unsigned);

    int ret = mallctl("thread.arena", &which_arena, &which_arena_size, nullptr, 0);

    int  len = 1000;
    void *p[len];

//    for (int i = 0; i < len; ++i) {
//        p[i] = 0;
//    }

    for (int i = 0; i < len; ++i) {
//        if (which_arena == 0) {
        p[i] = malloc(10 * 1024);
        if (which_arena == 1) {
            LOGD(TAG, "which arena: %d, p = %p", which_arena, p[i]);
        }
//        } else {
//            p[i] = 0;
//        }
    }

    sleep(1);

    for (int i = 0; i < len; ++i) {
        free(p[i]);
    }

    sleep(10);
    return nullptr;
}

static void testAlloc() {
    int       len = 10;
    pthread_t threads[len];

    for (int i = 0; i < len; ++i) {
        pthread_create(&threads[i], nullptr, alloc_routine, nullptr);
    }
}

void *arena0_alloc_opt_prevent;

JNIEXPORT jint JNICALL
Java_com_tencent_wxperf_jectl_JeCtl_preAllocRetainNative(JNIEnv *env, jclass clazz, jint __size0,
                                                         jint __size1, jint __limit0, jint __limit1) {

#ifdef __LP64__
    return ERR_64_BIT;
#else
    if (!initialized) {
        return ERR_INIT_FAILED;
    }
    assert(mallctl != nullptr);

    assert(__size0 > 0);
    assert(__size1 > 0);
    assert(__limit0 > 0);
    assert(__limit1 > 0);

    int ret             = 0;

    size_t dirty_ms;
    size_t new_dirty_ms = 0;
    size_t ms_size      = sizeof(size_t);
    ret = mallctl("arena.0.muzzy_decay_ms", &dirty_ms, &ms_size, &new_dirty_ms, ms_size);
    LOGD(TAG, "arena.0.muzzy_decay_ms ret = %d", ret);
    ret = mallctl("arena.1.muzzy_decay_ms", &dirty_ms, &ms_size, &new_dirty_ms, ms_size);
    LOGD(TAG, "arena.1.muzzy_decay_ms ret = %d", ret);

//    ret = mallctl("arena.0.dirty_decay_ms", &dirty_ms, &dirty_ms_size, &new_dirty_ms, dirty_ms_size);
//    LOGD(TAG, "arena.0.dirty_decay_ms ret = %d", ret);
//    ret = mallctl("arena.1.dirty_decay_ms", &dirty_ms, &dirty_ms_size,  &new_dirty_ms, dirty_ms_size);
//    LOGD(TAG, "arena.1.dirty_decay_ms ret = %d", ret);

    size_t old_limit  = 0;
    size_t new_limit  = __limit0;
    size_t limit_size = sizeof(size_t);
    ret = mallctl("arena.0.retain_grow_limit", &old_limit, &limit_size, &new_limit, limit_size);
    LOGD(TAG, "arena.0.retain_grow_limit ret = %d, old limit = %zu", ret, old_limit);
    new_limit = __limit1;
    ret = mallctl("arena.1.retain_grow_limit", &old_limit, &limit_size, &new_limit, limit_size);
    LOGD(TAG, "arena.1.retain_grow_limit ret = %d, old limit = %zu", ret, old_limit);

    LOGD(TAG, "prepare alloc");
    void *p = malloc(__size0);
    arena0_alloc_opt_prevent = p;
    LOGD(TAG, "prepare alloc arena0 done %p", p);
    free(p);
    hack_prealloc_within_arena1(__size1);

    flush_decay_purge();

    return JECTL_OK;
#endif
}

#ifdef __cplusplus
}
#endif