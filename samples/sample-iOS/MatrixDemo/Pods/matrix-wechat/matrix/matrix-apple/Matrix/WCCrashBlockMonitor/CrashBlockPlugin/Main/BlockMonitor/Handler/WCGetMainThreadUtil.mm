/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
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

#import "WCGetMainThreadUtil.h"
#import <mach/mach_types.h>
#import <mach/mach_init.h>
#import <mach/thread_act.h>
#import <mach/task.h>
#import <mach/mach_port.h>
#import <mach/vm_map.h>
#import "KSStackCursor_SelfThread.h"
#import "KSThread.h"

@interface WCGetMainThreadUtil ()
@end

@implementation WCGetMainThreadUtil

+ (void)getCurrentMainThreadStack:(void (^)(NSUInteger pc))saveResultBlock
{
    [WCGetMainThreadUtil getCurrentMainThreadStack:saveResultBlock withMaxEntries:WXGBackTraceMaxEntries];
}

+ (int)getCurrentMainThreadStack:(void (^)(NSUInteger pc))saveResultBlock
                  withMaxEntries:(NSUInteger)maxEntries
{
    NSUInteger tmpThreadCount;
    return [WCGetMainThreadUtil getCurrentMainThreadStack:saveResultBlock
                                           withMaxEntries:maxEntries
                                          withThreadCount:tmpThreadCount];
}

+ (int)getCurrentMainThreadStack:(void (^)(NSUInteger pc))saveResultBlock
                  withMaxEntries:(NSUInteger)maxEntries
                 withThreadCount:(NSUInteger &)retThreadCount
{
    thread_act_array_t threads;
    mach_msg_type_number_t thread_count;

    if (task_threads(mach_task_self(), &threads, &thread_count) != KERN_SUCCESS) {
        return 0;
    }

    thread_t mainThread = threads[0];

    KSThread currentThread = ksthread_self();
    if (mainThread == currentThread) {
        return 0;
    }

    if (thread_suspend(mainThread) != KERN_SUCCESS) {
        return 0;
    }

    uintptr_t backtraceBuffer[maxEntries];

    int backTraceLength = kssc_backtraceCurrentThread(mainThread, backtraceBuffer, (int) maxEntries);

    for (int i = 0; i < backTraceLength; i++) {
        NSUInteger pc = backtraceBuffer[i];
        saveResultBlock(pc);
    }
    retThreadCount = thread_count;

    thread_resume(mainThread);

    for (mach_msg_type_number_t i = 0; i < thread_count; i++) {
        mach_port_deallocate(mach_task_self(), threads[i]);
    }
    vm_deallocate(mach_task_self(), (vm_address_t) threads, sizeof(thread_t) * thread_count);

    return backTraceLength;
}

@end
