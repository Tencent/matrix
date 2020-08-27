/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _LIBUNWINDSTACK_LOG_H
#define _LIBUNWINDSTACK_LOG_H

#include <stdint.h>
#include <time.h>

namespace unwindstack {

void log_to_stdout(bool enable);
void log(uint8_t indent, const char* format, ...);

}  // namespace unwindstack

#define UNWIND_LOG(fmt, args...) \
//  do { unwindstack::log(0, fmt, ##args); } while (0)

#define TS_Start(timestamp) \
//        long timestamp = 0; \
//        { \
//            struct timespec tms; \
//            if (clock_gettime(CLOCK_REALTIME, &tms)) { \
//                UNWIND_LOG("UnwindTS Err: Get time failed."); \
//            } else { \
//                timestamp = tms.tv_nsec; \
//            } \
//        }

#define TS_End(tag, timestamp) \
//        { \
//            struct timespec tms; \
//            if (clock_gettime(CLOCK_REALTIME, &tms)) { \
//                UNWIND_LOG("UnwindTS Err: Get time failed."); \
//            } \
//            UNWIND_LOG("UnwindTS "#tag" %ld - %ld = costs: %ldns", tms.tv_nsec, timestamp, (tms.tv_nsec - timestamp)); \
//        }

#endif  // _LIBUNWINDSTACK_LOG_H
