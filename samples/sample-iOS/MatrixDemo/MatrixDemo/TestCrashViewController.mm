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

#import "TestCrashViewController.h"
#import <Matrix/MatrixTester.h>
#import "Utility.h"

@interface TestCrashViewController ()

@property (nonatomic, strong) UIButton *getCrashBtn;
@property (nonatomic, strong) UIButton *expCrashBtn;
@property (nonatomic, strong) UIButton *cppExpCrashBtn;
@property (nonatomic, strong) UIButton *childExpBtn;

@end

@implementation TestCrashViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    self.title = @"Crash";
    self.view.backgroundColor = [UIColor whiteColor];
    
    [self setupView];
}

- (void)setupView
{
    CGFloat btnHeight = 50.;
    CGFloat btnWidth = 200.;
    CGFloat btnGap = 44.;
    CGFloat contentX = (self.view.frame.size.width - btnWidth) / 2;
    CGFloat contentY = (self.view.frame.size.height - (btnHeight * 1 + btnGap * 0)) / 2;
    
    _getCrashBtn = [Utility genBigRedButtonWithFrame:CGRectMake(contentX, contentY, btnWidth, btnHeight)];
    [_getCrashBtn setBackgroundColor:[UIColor redColor]];
    [_getCrashBtn addTarget:self action:@selector(wantToCrash) forControlEvents:UIControlEventTouchUpInside];
    [_getCrashBtn setTitle:@"Make Crash" forState:UIControlStateNormal];
    [self.view addSubview:_getCrashBtn];
    
//    contentY = contentY + btnHeight + btnGap;
//
//    _expCrashBtn = [Utility genBigRedButtonWithFrame:CGRectMake(contentX, contentY, btnWidth, btnHeight)];
//    [_expCrashBtn setBackgroundColor:[UIColor redColor]];
//    [_expCrashBtn addTarget:self action:@selector(nsexcpetionTest) forControlEvents:UIControlEventTouchUpInside];
//    [_expCrashBtn setTitle:@"NSException" forState:UIControlStateNormal];
//    [self.view addSubview:_expCrashBtn];
//
//    contentY = contentY + btnHeight + btnGap;
//
//    _childExpBtn = [Utility genBigRedButtonWithFrame:CGRectMake(contentX, contentY, btnWidth, btnHeight)];
//    [_childExpBtn setBackgroundColor:[UIColor redColor]];
//    [_childExpBtn addTarget:self action:@selector(childNSexceptionTest) forControlEvents:UIControlEventTouchUpInside];
//    [_childExpBtn setTitle:@"Inherited NSException" forState:UIControlStateNormal];
//    [self.view addSubview:_childExpBtn];
//
//    contentY = contentY + btnHeight + btnGap;
//
//    _cppExpCrashBtn = [Utility genBigRedButtonWithFrame:CGRectMake(contentX, contentY, btnWidth, btnHeight)];
//    [_cppExpCrashBtn setBackgroundColor:[UIColor redColor]];
//    [_cppExpCrashBtn addTarget:self action:@selector(throwUncaughtCPPException) forControlEvents:UIControlEventTouchUpInside];
//    [_cppExpCrashBtn setTitle:@"C++ Exception" forState:UIControlStateNormal];
//    [self.view addSubview:_cppExpCrashBtn];
}

// ============================================================================
#pragma mark - Action
// ============================================================================

- (void)wantToCrash
{
    [[[MatrixTester alloc] init] notFoundSelectorCrash];
}

- (void)nsexcpetionTest
{
    [[[MatrixTester alloc] init] nsexceptionCrash];
}

- (void)childNSexceptionTest
{
    [[[MatrixTester alloc] init] childNsexceptionCrash];
}

- (void)throwUncaughtCPPException
{
    [[[MatrixTester alloc] init] cppexceptionCrash];
}

@end
