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

#import "MatrixTester.h"
#import <exception>

#import <TargetConditionals.h>

#if !TARGET_OS_OSX
#import <UIKit/UIKit.h>
#else
#import <AppKit/AppKit.h>
#endif

class MyException: public std::exception
{
public:
    virtual const char* what() const noexcept;
};

const char* MyException::what() const noexcept
{
    return "Something bad happened...";
}

class MyCPPClass
{
public:
    void throwAnException()
    {
        throw MyException();
    }
};

// ============================================================================
#pragma mark - ForceCrashException
// ============================================================================

@interface ForceCrashException : NSException @end
@implementation ForceCrashException @end

// ============================================================================
#pragma mark - MatrixTester
// ============================================================================

@interface MatrixTester ()

@property (nonatomic, strong) NSTimer *runloopTimer;
@property (nonatomic, strong) NSTimer *runloop2Timer;

@end

@implementation MatrixTester

// ============================================================================
#pragma mark - Crash
// ============================================================================

- (void)notFoundSelectorCrash
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundeclared-selector"
    [self performSelector:@selector(forceToCloseXxxx)];
#pragma clang diagnostic pop
}

- (void)wrongFormatCrash
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat"
    NSLog(@"%@", 1);
#pragma clang diagnostic pop
}

- (void)deadSignalCrash
{
    raise(SIGBUS);
}

- (void)nsexceptionCrash
{
    [NSException raise:@"testcrash" format:@""];
}

- (void)cppexceptionCrash
{
    MyCPPClass instance;
    instance.throwAnException();
}

- (void)cppToNsExceptionCrash
{
    try {
        [self cppexceptionCrash];
    } catch (const std::exception &e) {
        [self nsexceptionCrash];
    }
}

- (void)childNsexceptionCrash
{
    throw [ForceCrashException exceptionWithName:@"child nsexception" reason:nil userInfo:nil];
}

- (void)overflowCrash
{
    [self foo];
}

-(void)foo
{
    char tmpChar[] = "onTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflow\
    onTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflow\
    onTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflow\
    onTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflow\
    onTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflow\
    onTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflow\
    onTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflow\
    onTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflow\
    onTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflow\
    onTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflow\
    onTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflow\
    onTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflow\
    onTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflow\
    onTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflow\
    onTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflow\
    onTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflow\
    onTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflow\
    onTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflow\
    onTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflow\
    onTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflow\
    onTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflow\
    onTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflow\
    onTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflow\
    onTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflowonTest stackoverflow";
    [self fooo:tmpChar];
}

-(void)fooo:(char *)tmpChar
{
    tmpChar[0] = '1';
    [self foo];
}

// ============================================================================
#pragma mark - Lag
// ============================================================================

- (void)generateMainThreadLagLog
{
    dispatch_async(dispatch_get_main_queue(), ^{
        NSDate *lastDate = [NSDate date];
        int i = 1;
        while (1) {
            i++;
            NSDate *currentDate = [NSDate date];
            if (([currentDate timeIntervalSince1970] - [lastDate timeIntervalSince1970]) > 5.) {
                break;
            }
        }
    });
}

- (void)generateMainThreadBlockToBeKilledLog
{
    dispatch_async(dispatch_get_main_queue(), ^{
        int i = 1;
        while (1) {
            i++;
            dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
                int i = 1;
                while (1) {
                    i++;
                }
            });
        }
    });
}

- (void)testSpecialSceneOfLag
{
    // a magical test code
    _runloopTimer = [NSTimer scheduledTimerWithTimeInterval:2 target:self selector:@selector(runloop1Selector) userInfo:nil repeats:YES];
    _runloop2Timer = [NSTimer scheduledTimerWithTimeInterval:2 target:self selector:@selector(runloop2Selector) userInfo:nil repeats:YES];
}

- (void)runloop1Selector
{
    NSLog(@"I'm timer");
    dispatch_async(dispatch_get_main_queue(), ^{
        NSLog(@"I want to sleep");
        sleep(1);
    });

}

- (void)runloop2Selector
{
    NSLog(@"I'm timer2, sleep1");
    sleep(1);
}

static bool g_testCPUOrNot = false;

- (void)costCPUALot
{
    if (g_testCPUOrNot) {
        g_testCPUOrNot = false;
        return;
    }
    g_testCPUOrNot = true;
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        while (g_testCPUOrNot) {
#if !TARGET_OS_OSX
            UIColor *blueColor = [UIColor blueColor];
            blueColor = nil;
#else
            NSColor *blueColor = [NSColor blueColor];
            blueColor = nil;
#endif
            sleep(0.05);
        }
    });
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        while (g_testCPUOrNot) {
#if !TARGET_OS_OSX
            UIColor *redColor = [UIColor redColor];
            redColor = nil;
#else
            NSColor *redColor = [NSColor redColor];
            redColor = nil;
#endif
            sleep(0.05);
        }
    });
}

@end
