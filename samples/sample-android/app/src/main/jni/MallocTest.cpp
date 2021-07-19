//
// Created by Yves on 2020/8/7.
//

#include <pthread.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <android/log.h>

#define LOGD(TAG, FMT, args...) __android_log_print(ANDROID_LOG_DEBUG, TAG, FMT, ##args)

#define TAG "Wxperf.sample.malloc"

#define MALLOC_THREAD_COUNT 50

pthread_t hThreads[MALLOC_THREAD_COUNT];

size_t sizeClass[] =
               {8, 16, 32, 48, 64,
                80, 96, 112, 128, 160,
                192, 224, 256, 320, 384,
                448, 512, 640, 768, 896,
                1024, 1280, 1536, 1792, 2048,
                2560, 3072, 3584, 4096, 5120,
                6144, 7168, 8192, 10240, 12288,
                14336, 14337, 32 * 1024, 32 * 1024 + 1, 64 * 1024 - 3,
                64 * 1024 - 2, 64 * 1024 - 1};

#define LARGE_SIZE (4 * 1024 * 1024)

#define LEAKING_LIMIT  1024

static void *MAllocLogic(void *params) {
    int largeRound      = 600;

    char     *lastBuf[largeRound];
    int      lastBufIdx = 0;
    for (int i          = 0; i < largeRound; ++i) {
        lastBuf[i] = NULL;
    }
//    int diff = 0;

    int leakCount = 0;

    for (int i = 0; i < largeRound; i++) {

        char *buf = (char *) malloc(LARGE_SIZE);
        if (buf == nullptr) {
            LOGD(TAG, "[-] Fail to allocate buffer, tid: %d", gettid());
            return nullptr;
        }
        LOGD(TAG, "[+] large = %p", buf);

        bool     contains = false;
        for (int j        = 0; j < lastBufIdx; ++j) {
            if (lastBuf[j] == buf) {
                contains = true;
            }
        }
        if (!contains) {
            lastBuf[lastBufIdx++] = buf;
            LOGD(TAG, "diff!");
//            diff++;
        }

        memset(buf, 'a', LARGE_SIZE);
//        LOGD("VssWaster", "[-] malloc success, tid: %d, i = %d, buf[1] = %d", gettid(), i, buf[1]);
//        sleep(1);
        usleep(100);
//        if (leakCount < LEAKING_LIMIT && i % 100 == 0) {
//            LOGD(TAG, "leak %d", ++leakCount);
//        } else {
        free(buf);
//        }

        if (true) {
//            for (int j = i * 10; j < i * 10 + 10; ++j) {
//                void *p = malloc(sizeClass[j]);
//                memset(p, 100, sizeClass[j]);
////                usleep(100000);
//
//                void *p2 = malloc(sizeClass[j]);
//
//                LOGD("j = %6d, size = %6d, p = %p, p2 = %p", j, sizeClass[j], p, p2);
//
////                free(p2);
//                free(p);
//            }

/******************************* 2 *******************************/
//            int round = 1;
//            int len = 42;
//            void *q[round][len];
//
//            for (int r = 0; r < round; ++r) {
//                for (int j = 0; j < len; ++j) {
//                    q[r][j] = NULL;
//                }
//            }
//
//            for (int r = 0; r < round; ++r) {
//                for (int l = 0; l < len; ++l) {
//                    void *tmp = malloc(sizeClass[l]);
//                    memset(tmp, 0, sizeClass[l]);
//                    q[r][l] = tmp;
////                    LOGD("alloc %d", l);
//                }
////                LOGD(" q(%d-%d) = %p", i, k, *(q + k));
////                memset(*(q+k), 100, 16);
//            }
////            LOGD("alloc");
//
//
//
//            for (int r = 0; r < round; ++r) {
//                for (int l = 0; l < len; ++l) {
//                    if (q[r][l]) {
//                        free(q[r][l]);
//                    }
//                }
//
//            }
//
////            LOGD("free");
//            LOGD("======================");

/******************************* 3 *******************************/
            for (int z = 0; z < 100; ++z) {

                int      len = 42 - z % 42 + z / 42;
                void     *q[len];
                for (int r   = 0; r < len; ++r) {
                    q[r] = NULL;
                }

                int      c = (i + z) % 42;
//                LOGD("i = %d, sizeClass = %d, len = %d", i, sizeClass[c], len);
                for (int r = 0; r < len; ++r) {
                    void *tmp = malloc(sizeClass[c]);
                    memset(tmp, 1, sizeClass[c]);
                    q[r] = tmp;
                }

                for (int r = 0; r < len; ++r) {
                    if (q[r]) {
                        free(q[r]);
                    }
                }
            }

//            for (int k = 14336; k < 64 * 1024 + 500; ++k) {
//                void *p = malloc(k);
//                memset(p, 100, k);
//                usleep(10000);
//
//                void *p2 = malloc(k);
//
//                LOGD("j = %6d, size = %6d, p = %p, p2 = %p", k, k, p, p2);
//
//                free(p2);
//                free(p);
//            }
        }
    }
    return nullptr;
}

static void *MAllocFinalLogPrinter(void *params) {
    int finishedThreadCount = 0;
    while (finishedThreadCount < MALLOC_THREAD_COUNT) {
        for (int i = 0; i < MALLOC_THREAD_COUNT; ++i) {
            pthread_join(hThreads[i], nullptr);
            ++finishedThreadCount;
        }
    }
    LOGD(TAG, "[+] All malloc threads are finished.");
    return nullptr;
}

void malloc_test() {
    for (int  i = 0; i < MALLOC_THREAD_COUNT; ++i) {
        pthread_create(&hThreads[i], nullptr, MAllocLogic, nullptr);
    }
    pthread_t hPrintThread;
    pthread_create(&hPrintThread, nullptr, MAllocFinalLogPrinter, nullptr);
}