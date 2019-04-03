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

#import "TestOOMViewController.h"
#import "Utility.h"
#import "MatrixHandler.h"

@interface TestContact : NSObject

@property (nonatomic, copy) NSString *nickName;
@property (nonatomic, copy) NSString *sex;
@property (nonatomic, copy) NSString *country;
@property (nonatomic, copy) NSString *state;
@property (nonatomic, copy) NSString *city;
@property (nonatomic, copy) NSString *signature;

@end

@implementation TestContact

- (id)init
{
    self = [super init];
    if (self) {
        self.nickName = @"Don Shirley";
        self.sex = @"Man";
        self.country = @"U.S.A";
        self.state = @"New York";
        self.city = @"New York City";
        self.signature = @"You never win with violence. You only win when you maintain your dignity.- Don Shirley (Green Book) \
        It takes courage to change people’s hearts. - Oleg (Green Book) \
        The world's full of lonely people afraid to make the first move. - Tony Lip (Green Book)";
    }
    return self;
}

@end

@interface TestOOMViewController ()

@property (nonatomic, strong) UIButton *stopMemBtn;
@property (nonatomic, strong) UIButton *makeOOMBtn;

@end

@implementation TestOOMViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    self.title = @"OOM";
    self.view.backgroundColor = [UIColor whiteColor];
    
    [self setupView];
}

- (void)setupView
{
    CGFloat btnHeight = 50.;
    CGFloat btnWidth = 200.;
    CGFloat btnGap = 44.;
    CGFloat contentX = (self.view.frame.size.width - btnWidth) / 2;
    CGFloat contentY = (self.view.frame.size.height - (btnHeight * 2 + btnGap * 1)) / 2;

    _stopMemBtn = [Utility genBigGreenButtonWithFrame:CGRectMake(contentX, contentY, btnWidth, btnHeight)];
    [_stopMemBtn addTarget:self action:@selector(stopMemStat) forControlEvents:UIControlEventTouchUpInside];
    [_stopMemBtn setTitle:@"Stop MemStat" forState:UIControlStateNormal];
    [self.view addSubview:_stopMemBtn];

    contentY = contentY + btnHeight + btnGap;

    _makeOOMBtn = [Utility genBigRedButtonWithFrame:CGRectMake(contentX, contentY, btnWidth, btnHeight)];
    [_makeOOMBtn setBackgroundColor:[UIColor redColor]];
    [_makeOOMBtn addTarget:self action:@selector(testOOM) forControlEvents:UIControlEventTouchUpInside];
    [_makeOOMBtn setTitle:@"Make OOM" forState:UIControlStateNormal];
    [self.view addSubview:_makeOOMBtn];
}

// ============================================================================
#pragma mark - Action
// ============================================================================

- (void)startMemStat
{
    [[[MatrixHandler sharedInstance] getMemoryStatPlugin] start];
    
    UIAlertController *alertController = [UIAlertController alertControllerWithTitle:@"MemStatPlugin start" message:@"" preferredStyle:UIAlertControllerStyleAlert];
    [alertController addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];
    [self presentViewController:alertController animated:YES completion:nil];
}

- (void)stopMemStat
{
    [[[MatrixHandler sharedInstance] getMemoryStatPlugin] stop];
    
    UIAlertController *alertController = [UIAlertController alertControllerWithTitle:@"MemStatPlugin stop" message:@"" preferredStyle:UIAlertControllerStyleAlert];
    [alertController addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];
    [self presentViewController:alertController animated:YES completion:nil];
}

- (void)testOOM
{
    NSLog(@"Make OOM");
    
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        [[[MatrixHandler sharedInstance] getMemoryStatPlugin] addTagToCurrentThread:@"com.wechat.memdemo1"];
        NSMutableArray *array = [NSMutableArray array];
        while (1) {
            TestContact *contact = [[TestContact alloc] init];
            [array addObject:contact];
        }
    });
    
    UIAlertController *alertController = [UIAlertController alertControllerWithTitle:@"Warning" message:@"will out of memory" preferredStyle:UIAlertControllerStyleAlert];
    [self presentViewController:alertController animated:YES completion:nil];
}

@end
