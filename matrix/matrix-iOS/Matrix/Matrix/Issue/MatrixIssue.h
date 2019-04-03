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

@protocol MatrixPluginDelegate;

typedef NS_ENUM(NSUInteger, EMatrixIssueDataType) {
    EMatrixIssueDataType_Unknown = 0,
    EMatrixIssueDataType_Data = 1,
    EMatrixIssueDataType_FilePath = 2,
};

/**
 * MatrixPlguin produces MatrixIssue,
 * the issue contains the info that plugins generate.
 */
@interface MatrixIssue : NSObject

/// belong to which plugin (the plugin tag +[MatrixPlugin getTag])
@property (nonatomic, copy) NSString *issueTag;

/// the issue's unique identifier
@property (nonatomic, copy) NSString *issueID;

/// data type of the issue data, path or binary data
@property (nonatomic, assign) EMatrixIssueDataType dataType;

/// file path for EMatrixIssueDataType_FilePath
@property (nonatomic, copy) NSString *filePath;

/// data for EMatrixIssueDataType_Data
@property (nonatomic, strong) NSData *issueData;

/// the report type, you can use the report type to recognize the issue's type
@property (nonatomic, assign) int reportType;

@property (nonatomic, strong) NSDictionary *customInfo;

@end
