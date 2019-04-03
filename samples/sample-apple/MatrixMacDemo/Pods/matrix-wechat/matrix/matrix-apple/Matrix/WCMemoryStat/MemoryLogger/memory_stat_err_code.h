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

#ifndef memory_stat_err_code_h
#define memory_stat_err_code_h

#define MS_ERRC_SUCCESS 0

#define MS_ERRC_ANALYSIS_TOOL_RUNNING 10
#define MS_ERRC_WORKING_THREAD_CREATE_FAIL 11
#define MS_ERRC_SIGNAL_CRASH 12
#define MS_ERRC_DATA_CORRUPTED 17
#define MS_ERRC_DATA_CORRUPTED2 18

// Object Event/OE
#define MS_ERRC_OE_FILE_OPEN_FAIL 20
#define MS_ERRC_OE_FILE_SIZE_FAIL 21
#define MS_ERRC_OE_FILE_MMAP_FAIL 22

// DYLD Image/DI
#define MS_ERRC_DI_FILE_OPEN_FAIL 30
#define MS_ERRC_DI_FILE_SIZE_FAIL 31
#define MS_ERRC_DI_FILE_TRUNCATE_FAIL 32
#define MS_ERRC_DI_FILE_MMAP_FAIL 33

// Allocation Event/AE
#define MS_ERRC_AE_FILE_OPEN_FAIL 40
#define MS_ERRC_AE_FILE_SIZE_FAIL 41
#define MS_ERRC_AE_FILE_MMAP_FAIL 42

// Stack Frames/SF
#define MS_ERRC_SF_TABLE_FILE_OPEN_FAIL 50
#define MS_ERRC_SF_TABLE_FILE_SIZE_FAIL 51
#define MS_ERRC_SF_TABLE_FILE_TRUNCATE_FAIL 52
#define MS_ERRC_SF_TABLE_FILE_MMAP_FAIL 53
#define MS_ERRC_SF_INDEX_FILE_OPEN_FAIL 54
#define MS_ERRC_SF_INDEX_FILE_SIZE_FAIL 55
#define MS_ERRC_SF_INDEX_FILE_MMAP_FAIL 56

#endif /* memory_stat_err_code_h */
