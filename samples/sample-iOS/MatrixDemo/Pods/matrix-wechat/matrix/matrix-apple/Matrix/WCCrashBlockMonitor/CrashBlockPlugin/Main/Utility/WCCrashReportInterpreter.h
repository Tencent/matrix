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

#import <Foundation/Foundation.h>

@interface WCCrashReportInterpreter : NSObject

/**
 * @brief use current binary's symbol to symbolicate .crash or .ips
 * @param reportFileData the raw data of .crash or .ips
 * @return NSData * the symbolicated data
 */
+ (NSData *)interpretReport:(NSData *)reportFileData;

/**
 * @brief interprete any address.
 * the format reqired, exp：
 * 0   WeChat  0x0000000104ee242c 0x100100000 + 81667116
 * The binaray image should be in the second column,
 * The bias address should have a '+' before.
 * @return NSString * The symbolicated address.
 */
+ (NSString *)interpretText:(NSString *)stackText;

@end
