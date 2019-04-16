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

#import "TextViewController.h"

@interface TextViewController ()

@property (nonatomic, copy) NSString *showTextString;
@property (nonatomic, strong) UITextView *reportTextView;

@end

@implementation TextViewController

- (id)initWithString:(NSString *)textString
{
    return [self initWithString:textString withTitle:@"Issue Detail"];
}

- (id)initWithString:(NSString *)textString withTitle:(NSString *)title
{
    self = [super init];
    if (self) {
        self.showTextString = textString;
        self.title = title;
    }
    return self;
}

- (id)initWithFilePath:(NSString *)filePath
{
    return [self initWithFilePath:filePath withTitle:@"Issue Detail"];
}

- (id)initWithFilePath:(NSString *)filePath withTitle:(NSString *)title
{
    self = [super init];
    if (self) {
        NSData *data = [[NSData alloc] initWithContentsOfFile:filePath];
        self.showTextString = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
        self.title = title;
    }
    return self;
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    self.reportTextView = [[UITextView alloc] initWithFrame:CGRectMake(0, 0, self.view.frame.size.width, self.view.frame.size.height)];
    self.reportTextView.editable = NO;
    self.reportTextView.text = self.showTextString;
    self.reportTextView.alwaysBounceVertical = YES;
    [self.reportTextView scrollsToTop];
    [self.view addSubview:self.reportTextView];
}

@end
