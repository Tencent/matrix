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

#ifndef WCBlockTypeDef_h
#define WCBlockTypeDef_h

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSUInteger, EFilterType) {
    EFilterType_None = 0,
    EFilterType_Meaningless = 1, // the adress count of the stack is too little
    EFilterType_Annealing = 2, // the Annealing algorithm, filter the continuous same stack
    EFilterType_TrigerByTooMuch = 3, // filter the stack that appear too much one day
};

// Define the type of the lag
typedef NS_ENUM(NSUInteger, EDumpType) {
    EDumpType_Unlag = 2000,
    EDumpType_MainThreadBlock = 2001,            // foreground main thread block
    EDumpType_BackgroundMainThreadBlock = 2002,  // background main thread block
    EDumpType_CPUBlock = 2003,                   // CPU too high
    //EDumpType_FrameDropBlock = 2004,             // frame drop too much,no use currently
    //EDumpType_SelfDefinedDump = 2005,             // no use currently
    //EDumpType_B2FBlock = 2006,                   // no use currently
    EDumpType_LaunchBlock = 2007,                // main thread block during the launch of the app
    //EDumpType_CPUIntervalHigh = 2008,            // CPU too high within a time period
    EDumpType_BlockThreadTooMuch = 2009,         // main thread block and the thread is too much. (more than 64 threads)
    EDumpType_BlockAndBeKilled = 2010,           // main thread block and killed by the system
    //EDumpType_JSStack = 2011,                    // no use currently
    EDumpType_PowerConsume = 2011,                // battery cost stack report
    EDumpType_Test = 10000,
};

#endif /* WCBlockTypeDef_h */
