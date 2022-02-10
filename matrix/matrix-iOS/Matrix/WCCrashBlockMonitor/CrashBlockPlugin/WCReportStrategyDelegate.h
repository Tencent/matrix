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

#ifndef WCReportStrategyDelegate_h
#define WCReportStrategyDelegate_h

@protocol MatrixPluginProtocol;

@protocol WCReportStrategyDelegate <NSObject>

- (BOOL)isReportCrashLimit:(id<MatrixPluginProtocol>)plugin;
- (BOOL)isReportLagLimit:(id<MatrixPluginProtocol>)plugin;
- (BOOL)isCanAutoReportLag:(id<MatrixPluginProtocol>)plugin;
- (BOOL)isNetworkAllowAutoReportLag:(id<MatrixPluginProtocol>)plugin;

// Custom Strategy
- (BOOL)isReportSupportCustomLagStrategy;
- (BOOL)isReportCustomLagLimit:(id<MatrixPluginProtocol>)plugin customStrategy:(NSString *)strategyName;
- (BOOL)isCanCustomAutoReportLag:(id<MatrixPluginProtocol>)plugin customStrategy:(NSString *)strategyName;
- (NSArray<NSString *> *)tryGetMatrixReportCustomLagLimitReportIDs:(NSString *)strategyName;

@end

#endif /* WCReportStrategyDelegate_h */
