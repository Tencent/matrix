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

#import "ViewController.h"
#import "MatrixHandler.h"
#import "SlowTestViewController.h"
#import <exception>
#import <Matrix/WCCrashReportInterpreter.h>
#import <Matrix/MatrixTester.h>
#import "TestCrashViewController.h"
#import "TestLagViewController.h"
#import "TestOOMViewController.h"
#import "Utility.h"

@interface ViewController ()

@property (nonatomic, strong) UIButton *testResponseBtn;

@property (nonatomic, strong) UIButton *getFMBtn;
@property (nonatomic, strong) UIButton *getFTKMBtn;
@property (nonatomic, strong) UIButton *enterBtn;
@property (nonatomic, strong) UIButton *costCPUBtn;
@property (nonatomic, strong) UIButton *startBlockMonitorBtn;
@property (nonatomic, strong) UIButton *stopBlockMonitorBtn;
@property (nonatomic, strong) UIButton *symReportBtn;
@property (nonatomic, strong) UIButton *memoryTestBtn;

@property (nonatomic, strong) UIButton *crashViewBtn;
@property (nonatomic, strong) UIButton *lagViewBtn;
@property (nonatomic, strong) UIButton *oomViewBtn;

@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    self.title = @"Matrix";
    self.view.backgroundColor = [UIColor whiteColor];

    [self setupView];
}

- (void)setupView
{
    CGFloat btnHeight = 50.;
    CGFloat btnWidth = 200.;
    CGFloat btnGap = 44.;
    CGFloat contentX = (self.view.frame.size.width - btnWidth) / 2;
    CGFloat contentY = (self.view.frame.size.height - (btnHeight * 3 + btnGap * 2)) / 2;
    
    _crashViewBtn = [Utility genBigGreenButtonWithFrame:CGRectMake(contentX, contentY, btnWidth, btnHeight)];
    [_crashViewBtn setTitle:@"Crash" forState:UIControlStateNormal];
    [_crashViewBtn setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    [_crashViewBtn addTarget:self action:@selector(enterCrashView) forControlEvents:UIControlEventTouchUpInside];

    [self.view addSubview:_crashViewBtn];

    contentY = contentY + btnHeight + btnGap;
    
    _lagViewBtn = [Utility genBigGreenButtonWithFrame:CGRectMake(contentX, contentY, btnWidth, btnHeight)];
    [_lagViewBtn setTitle:@"Lag" forState:UIControlStateNormal];
    [_lagViewBtn addTarget:self action:@selector(enterLagView) forControlEvents:UIControlEventTouchUpInside];
    [self.view addSubview:_lagViewBtn];

    contentY = contentY + btnHeight + btnGap;

    _lagViewBtn = [Utility genBigGreenButtonWithFrame:CGRectMake(contentX, contentY, btnWidth, btnHeight)];
    [_lagViewBtn setTitle:@"Out Of Memory" forState:UIControlStateNormal];
    [_lagViewBtn addTarget:self action:@selector(enterOOMView) forControlEvents:UIControlEventTouchUpInside];
    [self.view addSubview:_lagViewBtn];
}

- (void)enterCrashView
{
    TestCrashViewController *vc = [[TestCrashViewController alloc] init];
    [self.navigationController pushViewController:vc animated:YES];
}

- (void)enterLagView
{
    TestLagViewController *vc = [[TestLagViewController alloc] init];
    [self.navigationController pushViewController:vc animated:YES];
}

- (void)enterOOMView
{
    TestOOMViewController *vc = [[TestOOMViewController alloc] init];
    [self.navigationController pushViewController:vc animated:YES];
}

- (void)enterNextView
{
    NSLog(@"enter next view");
    SlowTestViewController *vc = [[SlowTestViewController alloc] init];
    [self presentViewController:vc animated:YES completion:nil];
}

- (void)memoryStatTest
{
    WCMemoryStatPlugin *plugin = [[MatrixHandler sharedInstance] getMemoryStatPlugin];
    [plugin uploadReport:[plugin recordOfLastRun] withCustomInfo:@{@"uin": @(2)}];
}

- (void)symblicateReportTest
{
    NSString *testFilePath = [[NSBundle mainBundle] pathForResource:@"basicFomat" ofType:@"ips"];
    if (testFilePath == nil || [testFilePath length] == 0) {
        dispatch_async(dispatch_get_main_queue(), ^{
            UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"cannot symbolicate" message:nil preferredStyle:UIAlertControllerStyleAlert];
            UIAlertAction *cancelAction = [UIAlertAction actionWithTitle:@":)" style:UIAlertActionStyleDefault handler:nil];
            [alert addAction:cancelAction];
            [self presentViewController:alert animated:YES completion:nil];
        });
        return;
    }
    NSData *testFileData = [NSData dataWithContentsOfFile:testFilePath];
    NSData *returnStringData = [WCCrashReportInterpreter interpretReport:testFileData];
    NSString *returnString = [[NSString alloc] initWithData:returnStringData encoding:NSUTF8StringEncoding];
    NSLog(@"%@", returnString);
}

@end
