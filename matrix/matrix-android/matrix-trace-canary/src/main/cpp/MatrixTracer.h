/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

// Author: leafjia@tencent.com
//
// MatrixTracer.h

#ifndef LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_MatrixTracer_H_
#define LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_MatrixTracer_H_

bool anrDumpCallback(int status, const char *dumpFile);
bool anrDumpTraceCallback();
bool printTraceCallback();
#endif  // LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_MatrixTracer_H_