/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2022 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#import "WCFPSLogger.h"
#import "MatrixLogDef.h"
#import "WCDumpInterface.h"
#import "WCGetCallStackReportHandler.h"

#include "logger_internal.h"
#include <mach/mach.h>
#include <pthread/pthread.h>
#include <sys/ucontext.h>
#include <execinfo.h>
#include <unistd.h>
#include <stdlib.h>
#include <vector>
#include <atomic>

#if defined(__has_feature) && __has_feature(objc_arc)
#error the file does not support ARC!
#endif

#if defined(__arm64__)
#define BS_THREAD_STATE_COUNT ARM_THREAD_STATE64_COUNT
#define BS_THREAD_STATE ARM_THREAD_STATE64
#if __has_feature(ptrauth_calls)
#define BS_FRAME_POINTER __opaque_fp
#define BS_STACK_POINTER __opaque_sp
#define BS_INSTRUCTION_ADDRESS __opaque_pc
#else
#define BS_FRAME_POINTER __fp
#define BS_STACK_POINTER __sp
#define BS_INSTRUCTION_ADDRESS __pc
#endif
#elif defined(__arm__)
#define BS_THREAD_STATE_COUNT ARM_THREAD_STATE_COUNT
#define BS_THREAD_STATE ARM_THREAD_STATE
#define BS_FRAME_POINTER __r[11]
#define BS_STACK_POINTER __sp
#define BS_INSTRUCTION_ADDRESS __pc
#elif defined(__x86_64__)
#define BS_THREAD_STATE_COUNT x86_THREAD_STATE64_COUNT
#define BS_THREAD_STATE x86_THREAD_STATE64
#define BS_FRAME_POINTER __rbp
#define BS_STACK_POINTER __rsp
#define BS_INSTRUCTION_ADDRESS __rip
#elif defined(__i386__)
#define BS_THREAD_STATE_COUNT x86_THREAD_STATE32_COUNT
#define BS_THREAD_STATE x86_THREAD_STATE32
#define BS_FRAME_POINTER __ebp
#define BS_STACK_POINTER __esp
#define BS_INSTRUCTION_ADDRESS __eip
#endif

static pthread_t s_main_thread;
static mach_port_name_t s_main_thread_port;
static uintptr_t s_main_thread_stack_top;
static uintptr_t s_main_thread_stack_bot;

static malloc_lock_s s_lock;
static std::vector<WCFPSRecorder *> *s_record_list;
static std::atomic_bool s_is_writing_thread_running;

static id<WCFPSWriterDelegate> s_writer_delegate = nil;

extern bool s_enableLocalSymbolicate;

void init_fps_monitor(id<WCFPSWriterDelegate> delegate) {
    s_main_thread = pthread_self();
    s_main_thread_port = pthread_mach_thread_np(s_main_thread);
    s_main_thread_stack_top = (uintptr_t)pthread_get_stackaddr_np(s_main_thread);
    s_main_thread_stack_bot = s_main_thread_stack_top - pthread_get_stacksize_np(s_main_thread);

    s_lock = __malloc_lock_init();
    s_record_list = new std::vector<WCFPSRecorder *>();
    s_is_writing_thread_running = false;

    s_writer_delegate = [delegate retain];
}

__attribute__((noinline, not_tail_called)) static unsigned __main_thread_stack_pcs(uintptr_t fp, uintptr_t *buffer, unsigned max) {
    uintptr_t frame, next;
    unsigned nb = 0;

#if __has_feature(ptrauth_calls)
    fp = (fp & 0x0fffffffff); // PAC strip
#endif

    // Rely on the fact that our caller has an empty stackframe (no local vars)
    // to determine the minimum size of a stackframe (frame ptr & return addr)
    frame = (uintptr_t)fp;

#define INSTACK(a) ((a) >= s_main_thread_stack_bot && (a) < s_main_thread_stack_top)

#if defined(__x86_64__)
#define ISALIGNED(a) ((((uintptr_t)(a)) & 0xf) == 0)
#elif defined(__i386__)
#define ISALIGNED(a) ((((uintptr_t)(a)) & 0xf) == 8)
#elif defined(__arm__) || defined(__arm64__)
#define ISALIGNED(a) ((((uintptr_t)(a)) & 0x1) == 0)
#endif

    if (!INSTACK(frame) || !ISALIGNED(frame)) {
        return nb;
    }

    while (max--) {
        next = *(uintptr_t *)frame;
        uintptr_t retaddr = *((uintptr_t *)frame + 1);
        //uintptr_t retaddr2 = 0;
        //uintptr_t next2 = pthread_stack_frame_decode_np(frame, &retaddr2);
        if (retaddr == 0) {
            return nb;
        }
#if __has_feature(ptrauth_calls)
        //buffer[nb++] = (uintptr_t)ptrauth_strip((void *)retaddr, ptrauth_key_return_address); // PAC strip
        buffer[nb++] = (retaddr & 0x0fffffffff); // PAC strip
#elif defined(__arm__) || defined(__arm64__)
        buffer[nb++] = (retaddr & 0x0fffffffff); // PAC strip
#else
        buffer[nb++] = retaddr;
#endif
        if (!INSTACK(next) || !ISALIGNED(next) || next <= frame) {
            return nb;
        }
        frame = next;
    }

#undef INSTACK
#undef ISALIGNED

    return nb;
}

void *fps_dumping_thread(void *param) {
    set_curr_thread_ignore_logging(true);

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    WCFPSRecorder *recorder = (WCFPSRecorder *)param;

    char thread_name[32] = { 0 };
    sprintf(thread_name, "FPS Logging %d", recorder.recordID);

    pthread_setname_np(thread_name);

#define MAX_STACKS 500
#define STACK_SIZE 100

    uint64_t current_index = 0;
    uintptr_t stack_size[MAX_STACKS];
    uintptr_t stacks[MAX_STACKS][STACK_SIZE]; // no exceed thread stack size
    double timestamps[MAX_STACKS];

    _STRUCT_MCONTEXT machine_context;
    mach_msg_type_number_t state_count = BS_THREAD_STATE_COUNT;
    useconds_t usleep_time = recorder.dumpInterval * 1000 - 50;

    // 统计信息
    int thread_get_state_fails = 0;
    int thread_stack_pcs_fails = 0;

    while (recorder.shouldExit == false) {
        usleep(usleep_time);

        int index = (current_index % MAX_STACKS);
        kern_return_t kr = thread_get_state(s_main_thread_port, BS_THREAD_STATE, (thread_state_t) & (machine_context.__ss), &state_count);
        if (kr == KERN_SUCCESS) {
            stack_size[index] = __main_thread_stack_pcs((uintptr_t)(machine_context.__ss.BS_FRAME_POINTER), &(stacks[index][0]), STACK_SIZE);
            thread_stack_pcs_fails += (stack_size[index] == 0 ? 1 : 0);
        } else {
            stack_size[index] = 0;
            thread_get_state_fails++;
        }
        timestamps[index] = CFAbsoluteTimeGetCurrent();

        ++current_index;
    }

    // 先屏蔽 custom dump
    /*
    if (recorder.shouldDumpCpuUsage) {
        [WCDumpInterface dumpReportWithReportType:EDumpType_SelfDefinedDump
                              withExceptionReason:@"FPS Dump"
                                suspendAllThreads:YES
                                   enableSnapshot:YES
                                    writeCpuUsage:YES
                                  selfDefinedPath:YES];
    }
     */

    if (recorder.shouldDump) {
        double dumpBegin = recorder.dumpTimeBegin;
        double dumpEnd = dumpBegin + recorder.dumpTimeTotal / 1000.0;
        uint64_t indexEnd = MIN(MAX_STACKS, current_index);

        // 统计数据
        int count[3] = { 0 };

        for (uint64_t i = 0; i < indexEnd; ++i) {
            uint64_t index = (current_index - i - 1) % MAX_STACKS;
            if (stack_size[index] == 0) {
                count[0]++;
                continue;
            }
            if (timestamps[index] >= dumpEnd) {
                count[1]++;
                continue;
            }
            if (timestamps[index] < dumpBegin) {
                count[2]++;
                continue;
            }
            uintptr_t *buf = (uintptr_t *)malloc(stack_size[index] * sizeof(uintptr_t));
            assert(buf != NULL);
            memcpy(buf, &(stacks[index][0]), stack_size[index] * sizeof(uintptr_t));
            [recorder addThreadStack:(uintptr_t *)buf andLength:stack_size[index]];
        }

        int thread_get_state_fails = 0;
        int thread_stack_pcs_fails = 0;
        FPSInfo(@"record id: %d, scene: %@, current_index: %llu, thread_get_state_fails: %d, thread_stack_pcs_fails: %d, c0: %d, c1: %d, c2: %d",
                recorder.recordID,
                recorder.scene,
                current_index,
                thread_get_state_fails,
                thread_stack_pcs_fails,
                count[0],
                count[1],
                count[2]);

        __malloc_lock_lock(&s_lock);
        s_record_list->push_back([recorder retain]);
        __malloc_lock_unlock(&s_lock);

        create_fps_writing_thread();
    }

    [recorder release];

    [pool drain];

    return NULL;
}

void create_fps_dumping_thread(WCFPSRecorder *recorder) {
    int ret;
    pthread_attr_t tattr;
    sched_param param;

    /* initialized with default attributes */
    ret = pthread_attr_init(&tattr);

    /* safe to get existing scheduling param */
    ret = pthread_attr_getschedparam(&tattr, &param);

    /* set the highest priority; others are unchanged */
    param.sched_priority = MAX(sched_get_priority_max(SCHED_RR), param.sched_priority);

    /* setting the new scheduling param */
    ret = pthread_attr_setschedparam(&tattr, &param);
    pthread_t thread = 0;

    if (pthread_create(&thread, &tattr, fps_dumping_thread, recorder) == KERN_SUCCESS) {
        [recorder retain];
        pthread_detach(thread);
    } else {
    }
}

void *fps_writing_thread(void *param) {
    set_curr_thread_ignore_logging(true);

    pthread_setname_np("FPS Writing");

    s_is_writing_thread_running = true;
    while (true) {
        __malloc_lock_lock(&s_lock);

        if (s_record_list->size() == 0) {
            s_is_writing_thread_running = false;
            __malloc_lock_unlock(&s_lock);
            break;
        }

        WCFPSRecorder *recorder = s_record_list->front();
        s_record_list->erase(s_record_list->begin());

        __malloc_lock_unlock(&s_lock);

        NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

        NSArray<NSDictionary *> *call_tree = [recorder makeCallTree];

        NSString *report_id = [[NSUUID UUID] UUIDString];
        NSData *report_data = [WCGetCallStackReportHandler getReportJsonDataWithFPSStack:call_tree
                                                                          withCustomInfo:@{
                                                                              @"record_id" : @(recorder.recordID),
                                                                              @"scene" : recorder.scene,
                                                                              @"frame_interval" : @(recorder.dumpTimeTotal)
                                                                          }
                                                                            withReportID:report_id
                                                                            withDumpType:EDumpType_FPS];
        recorder.reportID = report_id;
        recorder.reportPath = [WCDumpInterface saveDump:report_data withReportType:EDumpType_FPS withReportID:report_id];
        if (recorder.reportPath) {
            [s_writer_delegate onFPSReportSaved:recorder];
        }

        if (s_enableLocalSymbolicate) {
            //FPSInfo(@"fps dump, record id: %d, dump_timestamp: %lf, scene: %@, recorder data: \n%@", recorder.recordID, recorder.dumpTimestamp, recorder.scene, [recorder description]);
        }

        FPSInfo(@"fps dump, record id: %d, dump_timestamp: %lf, scene: %@, report_id: %@",
                recorder.recordID,
                recorder.dumpTimeTotal,
                recorder.scene,
                report_id);

        [recorder release];

        [pool drain];
    }

    return NULL;
}

void create_fps_writing_thread() {
    if (s_is_writing_thread_running) {
        return;
    }

    int ret;
    pthread_attr_t tattr;
    sched_param param;

    /* initialized with default attributes */
    ret = pthread_attr_init(&tattr);

    /* safe to get existing scheduling param */
    ret = pthread_attr_getschedparam(&tattr, &param);

    /* set the highest priority; others are unchanged */
    param.sched_priority = MAX(sched_get_priority_max(SCHED_RR), param.sched_priority);

    /* setting the new scheduling param */
    ret = pthread_attr_setschedparam(&tattr, &param);
    pthread_t thread = 0;

    if (pthread_create(&thread, &tattr, fps_writing_thread, NULL) == KERN_SUCCESS) {
        pthread_detach(thread);
    } else {
    }
}
