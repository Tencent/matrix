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

#import "TestLagViewController.h"
#import <Matrix/MatrixTester.h>
#import "Utility.h"
#import "MatrixHandler.h"

@interface TestLagViewController ()

@property (nonatomic, strong) UIButton *getLagBtn;
@property (nonatomic, strong) UIButton *getBlockAndBeKilledLagBtn;
@property (nonatomic, strong) UIButton *startBlockMonitorBtn;
@property (nonatomic, strong) UIButton *stopBlockMonitorBtn;
@property (nonatomic, strong) UIButton *costCPUBtn;
@property (nonatomic, strong) UIButton *testResponseBtn;
@property (nonatomic, strong) UIButton *testIOBtn;

@property (nonatomic, assign) BOOL bCostCPUNow;

@property (nonatomic, strong) UIScrollView *mainScrollView;

@property (nonatomic, strong) MatrixTester *maTester;

@end

@implementation TestLagViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    self.title = @"Lag";
    self.view.backgroundColor = [UIColor whiteColor];
    
    self.maTester = [[MatrixTester alloc] init];
    // you can push code here, to test the "2002" lag (background main thread lag)
    // [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(testForegroundMainthreadLog) name:UIApplicationDidEnterBackgroundNotification object:nil];

    [self setupView];
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    if (_bCostCPUNow) {
        [self.maTester costCPUALot];
    }
}

- (void)setupView
{
    _mainScrollView = [[UIScrollView alloc] initWithFrame:CGRectMake(0, 0, self.view.frame.size.width, self.view.frame.size.height)];
    [self.view addSubview:_mainScrollView];
    
    CGFloat btnHeight = 50.;
    CGFloat btnWidth = 260;
    CGFloat btnGap = 44.;
    CGFloat contentX = (self.view.frame.size.width - btnWidth) / 2;
    CGFloat contentY = btnGap; //(self.view.frame.size.height - (btnHeight * 6 + btnGap * 5)) / 2;

    _getLagBtn = [Utility genBigRedButtonWithFrame:CGRectMake(contentX, contentY, btnWidth, btnHeight)];
    [_getLagBtn addTarget:self action:@selector(testForegroundMainthreadLog) forControlEvents:UIControlEventTouchUpInside];
    [_getLagBtn setTitle:@"Block Main Thread A While" forState:UIControlStateNormal];
    [_mainScrollView addSubview:_getLagBtn];
    
    contentY = contentY + btnHeight + btnGap;
    
    _getBlockAndBeKilledLagBtn = [Utility genBigRedButtonWithFrame:CGRectMake(contentX, contentY, btnWidth, btnHeight)];
    [_getBlockAndBeKilledLagBtn addTarget:self action:@selector(testBlockAndBeKilled) forControlEvents:UIControlEventTouchUpInside];
    [_getBlockAndBeKilledLagBtn setTitle:@"Block Main Thread And Killed" forState:UIControlStateNormal];
    [_mainScrollView addSubview:_getBlockAndBeKilledLagBtn];
    
    contentY = contentY + btnHeight + btnGap;
    
    _testResponseBtn = [Utility genBigGreenButtonWithFrame:CGRectMake(contentX, contentY, btnWidth, btnHeight)];
    [_testResponseBtn addTarget:self action:@selector(testResponse) forControlEvents:UIControlEventTouchUpInside];
    [_testResponseBtn setTitle:@"Test Response" forState:UIControlStateNormal];
    [_mainScrollView addSubview:_testResponseBtn];

    contentY = contentY + btnHeight + btnGap;

    _bCostCPUNow = NO;
    
    _costCPUBtn = [Utility genBigRedButtonWithFrame:CGRectMake(contentX, contentY, btnWidth, btnHeight)];
    [_costCPUBtn addTarget:self action:@selector(costAlotCPU) forControlEvents:UIControlEventTouchUpInside];
    [_costCPUBtn setTitle:@"Cost CPU A Lot" forState:UIControlStateNormal];
    [_mainScrollView addSubview:_costCPUBtn];
    
    contentY = contentY + btnHeight + btnGap;
    
    _testIOBtn = [Utility genBigGreenButtonWithFrame:CGRectMake(contentX, contentY, btnWidth, btnHeight)];
    [_testIOBtn addTarget:self action:@selector(costDiskIOLag) forControlEvents:UIControlEventTouchUpInside];
    [_testIOBtn setTitle:@"Disk IO" forState:UIControlStateNormal];
    [_mainScrollView addSubview:_testIOBtn];

    contentY = contentY + btnHeight + btnGap;
    
    _startBlockMonitorBtn = [Utility genBigGreenButtonWithFrame:CGRectMake(contentX, contentY, btnWidth, btnHeight)];
    [_startBlockMonitorBtn addTarget:self action:@selector(startBlockMonitor) forControlEvents:UIControlEventTouchUpInside];
    [_startBlockMonitorBtn setTitle:@"Start Block Monitor" forState:UIControlStateNormal];
    [_mainScrollView addSubview:_startBlockMonitorBtn];
    
    contentY = contentY + btnHeight + btnGap;
    
    _stopBlockMonitorBtn = [Utility genBigGreenButtonWithFrame:CGRectMake(contentX, contentY, btnWidth, btnHeight)];
    [_stopBlockMonitorBtn addTarget:self action:@selector(stopBlockMonitor) forControlEvents:UIControlEventTouchUpInside];
    [_stopBlockMonitorBtn setTitle:@"Stop Block Monitor" forState:UIControlStateNormal];
    [_mainScrollView addSubview:_stopBlockMonitorBtn];
    
    [_mainScrollView setContentSize:CGSizeMake(self.view.frame.size.width, _stopBlockMonitorBtn.frame.origin.y + btnHeight + btnGap)];
    
}

// ============================================================================
#pragma mark - Action
// ============================================================================

- (void)testForegroundMainthreadLog
{
    NSLog(@"Test Foreground Main Thread Log");
    NSLog(@"wait.. 5s");
    [self.maTester generateMainThreadLagLog];
}

- (void)testBlockAndBeKilled
{
    NSLog(@"Test Block And Be killed");
    [self.maTester generateMainThreadBlockToBeKilledLog];
}

- (void)costAlotCPU
{
    if (_bCostCPUNow) {
        _bCostCPUNow = NO;
        [_costCPUBtn setTitle:@"Cost CPU A Lot" forState:UIControlStateNormal];
    } else {
        _bCostCPUNow = YES;
        [_costCPUBtn setTitle:@"Stop Cost CPU A Lot" forState:UIControlStateNormal];
    }
    [self.maTester costCPUALot];
}

- (void)costDiskIOLag
{
    [self.maTester writeMassData];
}

- (void)startBlockMonitor
{
    [[[MatrixHandler sharedInstance] getCrashBlockPlugin] startBlockMonitor];
}

- (void)stopBlockMonitor
{
    [[[MatrixHandler sharedInstance] getCrashBlockPlugin] stopBlockMonitor];
}

- (void)testResponse
{
    UIAlertController *alertController = [UIAlertController alertControllerWithTitle:@"Hi" message:@"" preferredStyle:UIAlertControllerStyleAlert];
    [alertController addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];
    [self presentViewController:alertController animated:YES completion:nil];
}

@end
