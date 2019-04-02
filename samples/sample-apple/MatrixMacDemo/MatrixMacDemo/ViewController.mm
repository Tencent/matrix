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
#import <matrix-apple/MatrixTester.h>
#import "MacMatrixManager.h"

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
        self.signature = @"You never win with violence. You only win when you maintain your dignity. - Don Shirley (Green Book) It takes courage to change people’s hearts. - Oleg (Green Book) The world's full of lonely people afraid to make the first move. - Tony Lip (Green Book)";
    }
    return self;
}

@end

@implementation ViewController

- (void)viewDidLoad
{
    [super viewDidLoad];
    
    self.title = @"Matrix";
}

- (void)setRepresentedObject:(id)representedObject
{
    [super setRepresentedObject:representedObject];

}

- (IBAction)makeALag:(id)sender
{
    NSLog(@"make a lag");
    [[[MatrixTester alloc] init] generateMainThreadLagLog];
}

- (IBAction)makeBlockAndKill:(id)sender
{
    NSLog(@"block and killed");
    [[[MatrixTester alloc] init] generateMainThreadBlockToBeKilledLog];
}

- (IBAction)makeCppException:(id)sender
{
    [[[MatrixTester alloc] init] cppexceptionCrash];
}

- (IBAction)makeOOM:(id)sender
{
    NSLog(@"Make OOM");
    
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        [[[MacMatrixManager sharedInstance] getMemoryStatPlugin] addTagToCurrentThread:@"com.wechat.memdemo1"];
        NSMutableArray *array = [NSMutableArray array];
        while (1) {
            TestContact *contact = [[TestContact alloc] init];
            [array addObject:contact];
        }
    });
}
@end
