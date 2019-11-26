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

#import "WCBlockMonitorMgr.h"
#import <vector>
#import <sys/time.h>
#import <sys/sysctl.h>
#import <mach/mach.h>
#import <mach/mach_types.h>
#import <pthread/pthread.h>

#import "MatrixLogDef.h"
#import "WCGetMainThreadUtil.h"
#import "WCCPUHandler.h"
#import "WCBlockMonitorConfigHandler.h"
#import "WCDumpInterface.h"
#import "WCCrashBlockFileHandler.h"
#import "WCGetCallStackReportHandler.h"
#import "WCCrashBlockMonitorPlugin.h"
#import "WCFilterStackHandler.h"
#import "KSSymbolicator.h"
#import "WCPowerConsumeStackCollector.h"
#import "MatrixAsyncHook.h"

#if !TARGET_OS_OSX
#import <UIKit/UIKit.h>
#endif

#if TARGET_OS_OSX
#import "NSApplicationEvent.h"
#endif

mach_port_t g_matrix_block_monitor_dumping_thread_id = 0;

static useconds_t g_RunLoopTimeOut = g_defaultRunLoopTimeOut;
static useconds_t g_CheckPeriodTime = g_defaultCheckPeriodTime;
static float g_CPUUsagePercent = 1000; // a big cpu percent for preventing from wrong setting.
static useconds_t g_PerStackInterval = g_defaultPerStackInterval;
static size_t g_StackMaxCount = 100;

static NSUInteger g_CurrentThreadCount = 0;
static BOOL g_MainThreadHandle = NO;
static int g_MainThreadCount = 0;
static KSStackCursor *g_PointMainThreadArray = NULL;
static int *g_PointMainThreadRepeatCountArray = NULL;
static KSStackCursor **g_PointCPUHighThreadArray = NULL;
static int g_PointCpuHighThreadCount = 0;
static float *g_PointCpuHighThreadValueArray = NULL;

static BOOL g_filterSameStack = NO;
static uint32_t g_triggerdFilterSameCnt = 0;

#define APP_SHOULD_SUSPEND 180 * BM_MicroFormat_Second

#define PRINT_MEMORY_USE_INTERVAL 5

#define __timercmp(tvp, uvp, cmp) \
    (((tvp)->tv_sec == (uvp)->tv_sec) ? ((tvp)->tv_usec cmp(uvp)->tv_usec) : ((tvp)->tv_sec cmp(uvp)->tv_sec))

#define BM_SAFE_CALL_SELECTOR_NO_RETURN(obj,sel,func) \
{\
if(obj&&[obj respondsToSelector:sel]) \
{\
[obj func];\
}\
}\

#if TARGET_OS_OSX
static struct timeval g_tvEvent;
static BOOL g_eventStart;
#endif

static struct timeval g_tvRun;
static BOOL g_bRun;
static struct timeval g_enterBackground;
static struct timeval g_tvSuspend;
static CFRunLoopActivity g_runLoopActivity;
static struct timeval g_lastCheckTime;

static BOOL g_bLaunchOver = NO;
#if !TARGET_OS_OSX
static BOOL g_bBackgroundLaunch = NO;
#endif
static BOOL g_bMonitor = NO;

typedef enum : NSUInteger {
    eRunloopInitMode,
    eRunloopDefaultMode,
} ERunloopMode;

static ERunloopMode g_runLoopMode;

void exitCallBack()
{
    [[WCBlockMonitorMgr shareInstance] stop];
}

KSStackCursor *kscrash_pointThreadCallback(void)
{
    return g_PointMainThreadArray;
}

int *kscrash_pointThreadRepeatNumberCallback(void)
{
    return g_PointMainThreadRepeatCountArray;
}

KSStackCursor **kscrash_pointCPUHighThreadCallback(void)
{
    return g_PointCPUHighThreadArray;
}

int kscrash_pointCpuHighThreadCountCallback(void)
{
    return g_PointCpuHighThreadCount;
}

float *kscrash_pointCpuHighThreadArrayCallBack(void)
{
    return g_PointCpuHighThreadValueArray;
}

@interface WCBlockMonitorMgr () <WCPowerConsumeStackCollectorDelegate> {
    NSThread *m_monitorThread;
    BOOL m_bStop;

#if !TARGET_OS_OSX
    UIApplicationState m_currentState;
#endif

    //  ================================================

    NSUInteger m_nIntervalTime;
    NSUInteger m_nLastTimeInterval;

    std::vector<NSUInteger> m_vecLastMainThreadCallStack;
    NSUInteger m_lastMainThreadStackCount;

    // ================================================

    uint64_t m_blockDiffTime;

    uint32_t m_firstSleepTime;

    NSString *m_potenHandledLagFile;

    WCMainThreadHandler *m_pointMainThreadHandler;

    BOOL m_bInSuspend;

    CFRunLoopObserverRef m_runLoopBeginObserver;
    CFRunLoopObserverRef m_runLoopEndObserver;
    CFRunLoopObserverRef m_initializationBeginRunloopObserver;
    CFRunLoopObserverRef m_initializationEndRunloopObserver;

    dispatch_queue_t m_asyncDumpQueue;

    WCCPUHandler *m_cpuHandler;
    BOOL m_bTrackCPU;
    
    WCFilterStackHandler *m_stackHandler;
    WCPowerConsumeStackCollector *m_powerConsumeStackCollector;
    
    uint32_t m_printMemoryTickTok;
}

@property (nonatomic, strong) WCBlockMonitorConfigHandler *monitorConfigHandler;

#if TARGET_OS_OSX
@property (nonatomic, strong) NSApplicationEvent *eventHandler;
#endif

@end

@implementation WCBlockMonitorMgr
    
+ (WCBlockMonitorMgr *)shareInstance
{
    static WCBlockMonitorMgr *g_monitorMgr = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        g_monitorMgr = [[WCBlockMonitorMgr alloc] init];
    });
    return g_monitorMgr;
}

- (void)resetConfiguration:(WCBlockMonitorConfiguration *)bmConfig
{
    _monitorConfigHandler = [[WCBlockMonitorConfigHandler alloc] init];
    [_monitorConfigHandler setConfiguration:bmConfig];
}

- (id)init
{
    if (self = [super init]) {
#if !TARGET_OS_OSX
        g_bLaunchOver = NO;
#else
        g_bLaunchOver = YES;
#endif
        m_potenHandledLagFile = nil;
        m_asyncDumpQueue = dispatch_queue_create("com.tencent.xin.asyncdump", DISPATCH_QUEUE_SERIAL);
        m_bStop = YES;
    }

    return self;
}

- (void)dealloc
{
    CFRelease(m_runLoopBeginObserver);
    CFRelease(m_runLoopEndObserver);
    CFRelease(m_initializationBeginRunloopObserver);
    CFRelease(m_initializationEndRunloopObserver);
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    if (g_PointMainThreadArray != NULL) {
        free(g_PointMainThreadArray);
    }
    if (g_PointMainThreadRepeatCountArray != NULL) {
        free(g_PointMainThreadRepeatCountArray);
    }
}

- (void)freeCpuHighThreadArray
{
    for (int i = 0; i < g_PointCpuHighThreadCount; i++) {
        if (g_PointCPUHighThreadArray[i] != NULL) {
            KSStackCursor_Backtrace_Context *context = (KSStackCursor_Backtrace_Context *) g_PointCPUHighThreadArray[i]->context;
            if (context->backtrace != NULL) {
                free((uintptr_t *) context->backtrace);
            }
            free(g_PointCPUHighThreadArray[i]);
            g_PointCPUHighThreadArray[i] = NULL;
        }
    }
    
    if (g_PointCPUHighThreadArray != NULL) {
        free(g_PointCPUHighThreadArray);
        g_PointCPUHighThreadArray = NULL;
    }
    
    if (g_PointCpuHighThreadValueArray != NULL) {
        free(g_PointCpuHighThreadValueArray);
        g_PointCpuHighThreadValueArray = NULL;
    }
    
    g_PointCpuHighThreadCount = 0;
}

// ============================================================================
#pragma mark - Public method
// ============================================================================

- (void)start
{
    if (!m_bStop) {
        return;
    }
    
    g_bMonitor = YES;
    
    g_MainThreadHandle = [_monitorConfigHandler getMainThreadHandle];
    [self setRunloopTimeOut:[_monitorConfigHandler getRunloopTimeOut]
         andCheckPeriodTime:[_monitorConfigHandler getCheckPeriodTime]];
    [self setCPUUsagePercent:[_monitorConfigHandler getCPUUsagePercent]];
    [self setPerStackInterval:[_monitorConfigHandler getPerStackInterval]];
    
    [[MatrixAsyncHook sharedInstance] beginHook];

    m_nIntervalTime = 1;
    m_nLastTimeInterval = 1;
    m_blockDiffTime = 0;
    m_firstSleepTime = 5;
    gettimeofday(&g_tvSuspend, NULL);
    g_enterBackground = {0, 0};
    gettimeofday(&g_lastCheckTime, NULL);
    
    if ([_monitorConfigHandler getShouldPrintMemoryUse]) {
        m_printMemoryTickTok = 0;
    } else {
        m_printMemoryTickTok = 6;
    }

    g_MainThreadCount = [_monitorConfigHandler getMainThreadCount];
    m_pointMainThreadHandler = [[WCMainThreadHandler alloc] initWithCycleArrayCount:g_MainThreadCount];
    g_StackMaxCount = [m_pointMainThreadHandler getStackMaxCount];
    
    m_bInSuspend = YES;
    
    m_bTrackCPU = YES;
    
    m_cpuHandler = [[WCCPUHandler alloc] initWithCPULimit:[_monitorConfigHandler getPowerConsumeCPULimit]];
    
    if ([_monitorConfigHandler getShouldGetPowerConsumeStack]) {
        m_powerConsumeStackCollector = [[WCPowerConsumeStackCollector alloc] initWithCPULimit:[_monitorConfigHandler getPowerConsumeCPULimit]];
        m_powerConsumeStackCollector.delegate = self;
    } else {
        m_powerConsumeStackCollector = nil;
    }

    g_filterSameStack = [_monitorConfigHandler getShouldFilterSameStack];
    g_triggerdFilterSameCnt = [_monitorConfigHandler getTriggerFilterCount];
    
#if !TARGET_OS_OSX
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(willTerminate)
                                                 name:UIApplicationWillTerminateNotification
                                               object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(didBecomeActive)
                                                 name:UIApplicationDidBecomeActiveNotification
                                               object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(didEnterBackground)
                                                 name:UIApplicationDidEnterBackgroundNotification
                                               object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(willResignActive)
                                                 name:UIApplicationWillResignActiveNotification
                                               object:nil];
#endif

    atexit(exitCallBack);

    [self addRunLoopObserver];
    [self addMonitorThread];
    
#if TARGET_OS_OSX
    self.eventHandler = [[NSApplicationEvent alloc] init];
    [self.eventHandler tryswizzle];
#endif

}

- (void)stop
{
    if (m_bStop) {
        return;
    }
    
    m_bStop = YES;

    [self removeRunLoopObserver];

    while ([m_monitorThread isExecuting]) {
        usleep(100 * BM_MicroFormat_MillSecond);
    }
}

#if !TARGET_OS_OSX

- (void)handleBackgroundLaunch
{
    if (m_bInSuspend) {
        g_bMonitor = NO;
        g_bBackgroundLaunch = YES;
    }
}

- (void)handleSuspend
{
    g_bMonitor = NO;
    gettimeofday(&g_tvSuspend, NULL);
    m_bInSuspend = YES;
}

#endif

- (NSDictionary *)getUserInfoForCurrentDumpForDumpType:(EDumpType)dumpType
{
    if (_delegate != nil && [_delegate respondsToSelector:@selector(onBlockMonitor:getCustomUserInfoForDumpType:)]) {
        return [_delegate onBlockMonitor:self getCustomUserInfoForDumpType:dumpType];
    }
    return nil;
}

// ============================================================================
#pragma mark - Application State (Notification Observer)
// ============================================================================

#if !TARGET_OS_OSX
- (void)willTerminate
{
    [self stop];
}

- (void)didBecomeActive
{
    MatrixDebug(@"did become active");
    
    g_enterBackground = {0, 0};

    m_bInSuspend = NO;
    m_currentState = [UIApplication sharedApplication].applicationState;

    g_bMonitor = YES;
    g_bLaunchOver = YES;
    
    if (g_bBackgroundLaunch) {
        [self clearDumpInBackgroundLaunch];
        g_bBackgroundLaunch = NO;
    }
    
    [self clearLaunchLagRecord];
}

- (void)didEnterBackground
{
    MatrixDebug(@"did enter background");
    gettimeofday(&g_enterBackground, NULL);
    m_currentState = [UIApplication sharedApplication].applicationState;
}

- (void)willResignActive
{
    MatrixDebug(@"will resign active");
    m_currentState = [UIApplication sharedApplication].applicationState;
    g_bLaunchOver = YES;
}
    
#endif
    
// ============================================================================
#pragma mark - Config
// ============================================================================

- (void)setRunloopTimeOut:(useconds_t)runloopTimeOut andCheckPeriodTime:(useconds_t)checkPeriodTime
{
    if (runloopTimeOut < checkPeriodTime || checkPeriodTime < BM_MicroFormat_FrameMillSecond || runloopTimeOut < BM_MicroFormat_FrameMillSecond) {
        MatrixWarning(@"runloopTimeOut[%u] < checkPeriodTime[%u]", runloopTimeOut, checkPeriodTime);
        return;
    }
    useconds_t tmpTimeOut = g_RunLoopTimeOut;
    useconds_t tmpPeriodTime = g_CheckPeriodTime;
    g_RunLoopTimeOut = runloopTimeOut;
    g_CheckPeriodTime = checkPeriodTime;
    MatrixInfo(@"set timeout: before[%u] after[%u], period: before[%u] after[%u]", tmpTimeOut, g_RunLoopTimeOut, tmpPeriodTime, g_CheckPeriodTime);
}

- (void)setCPUUsagePercent:(float)usagePercent
{
    float tmpUsagePercent = g_CPUUsagePercent;
    g_CPUUsagePercent = usagePercent;
    MatrixInfo(@"set cpuusage before[%lf] after[%lf]", tmpUsagePercent, g_CPUUsagePercent);
}

- (void)setPerStackInterval:(useconds_t)perStackInterval
{
    if (perStackInterval < BM_MicroFormat_FrameMillSecond || perStackInterval > BM_MicroFormat_Second) {
        MatrixWarning(@"perstackInterval invalid, current[%u] setTo[%u]", g_PerStackInterval, perStackInterval);
        return;
    }
    useconds_t tmpStackInterval = g_PerStackInterval;
    g_PerStackInterval = perStackInterval;
    MatrixInfo(@"set per stack interval before[%u] after[%u]", tmpStackInterval, g_PerStackInterval);
}

// ============================================================================
#pragma mark - Monitor Thread
// ============================================================================

- (void)addMonitorThread
{
    m_bStop = NO;
    m_monitorThread = [[NSThread alloc] initWithTarget:self selector:@selector(threadProc) object:nil];
    [m_monitorThread start];
}

- (void)threadProc
{
    g_matrix_block_monitor_dumping_thread_id = pthread_mach_thread_np(pthread_self());

    if (m_firstSleepTime) {
        sleep(m_firstSleepTime);
        m_firstSleepTime = 0;
    }
    
    if (g_filterSameStack) {
        m_stackHandler = [[WCFilterStackHandler alloc] init];
    }
    
    while (YES) {
        @autoreleasepool {
            if (g_bMonitor) {
                EDumpType dumpType = [self check];
                if (m_bStop) {
                    break;
                }
                BM_SAFE_CALL_SELECTOR_NO_RETURN(_delegate,
                                                @selector(onBlockMonitor:enterNextCheckWithDumpType:),
                                                onBlockMonitor:self enterNextCheckWithDumpType:dumpType);
                if (dumpType != EDumpType_Unlag) {
                    if (EDumpType_BackgroundMainThreadBlock == dumpType ||
                        EDumpType_MainThreadBlock == dumpType) {
                        if (g_CurrentThreadCount > 64) {
                            dumpType = EDumpType_BlockThreadTooMuch;
                            [self dumpFileWithType:dumpType];
                        } else {
                            EFilterType filterType = [self needFilter];
                            if (filterType == EFilterType_None) {
                                if (g_MainThreadHandle) {
                                    if (g_PointMainThreadArray != NULL) {
                                        free(g_PointMainThreadArray);
                                        g_PointMainThreadArray = NULL;
                                    }
                                    g_PointMainThreadArray = [m_pointMainThreadHandler getPointStackCursor];
                                    g_PointMainThreadRepeatCountArray = [m_pointMainThreadHandler getPointStackRepeatCount];
                                    m_potenHandledLagFile = [self dumpFileWithType:dumpType];
                                    BM_SAFE_CALL_SELECTOR_NO_RETURN(_delegate,
                                                                    @selector(onBlockMonitor:getDumpFile:withDumpType:),
                                                                    onBlockMonitor:self getDumpFile:m_potenHandledLagFile withDumpType:dumpType);
                                    if (g_PointMainThreadArray != NULL) {
                                        KSStackCursor_Backtrace_Context *context = (KSStackCursor_Backtrace_Context *) g_PointMainThreadArray->context;
                                        if (context->backtrace) {
                                            free((uintptr_t *) context->backtrace);
                                        }
                                        free(g_PointMainThreadArray);
                                        g_PointMainThreadArray = NULL;
                                    }
                                } else {
                                    m_potenHandledLagFile = [self dumpFileWithType:dumpType];
                                    BM_SAFE_CALL_SELECTOR_NO_RETURN(_delegate,
                                                                    @selector(onBlockMonitor:getDumpFile:withDumpType:),
                                                                    onBlockMonitor:self getDumpFile:m_potenHandledLagFile withDumpType:dumpType);
                                }
                            } else {
                                BM_SAFE_CALL_SELECTOR_NO_RETURN(_delegate,
                                                                @selector(onBlockMonitor:dumpType:filter:),
                                                                onBlockMonitor:self dumpType:dumpType filter:filterType);
                            }
                        }
                    } else if (EDumpType_CPUBlock == dumpType) {
                        //1. get cpu high stack cursor
                        if (m_powerConsumeStackCollector) {
                            if (g_PointCPUHighThreadArray != NULL) {
                                [self freeCpuHighThreadArray];
                            }
                            g_PointCPUHighThreadArray = [m_powerConsumeStackCollector getCPUStackCursor];
                            g_PointCpuHighThreadCount = [m_powerConsumeStackCollector getCurrentCpuHighStackNumber];
                            g_PointCpuHighThreadValueArray = [m_powerConsumeStackCollector getCpuHighThreadValueArray];
                        }
                        //2. dump file with type
                        m_potenHandledLagFile = [self dumpFileWithType:dumpType];
                        [self freeCpuHighThreadArray];
                    }
                    else {
                        m_potenHandledLagFile = [self dumpFileWithType:dumpType];
                    }
                } else {
                    [self resetStatus];
                }
            }

            for (int nCnt = 0; nCnt < m_nIntervalTime && !m_bStop; nCnt++) {
                if (g_MainThreadHandle && g_bMonitor) {
                    int intervalCount = g_CheckPeriodTime / g_PerStackInterval;
                    if (intervalCount <= 0) {
                        usleep(g_CheckPeriodTime);
                    } else {
                        for (int index = 0; index < intervalCount && !m_bStop; index++) {
                            usleep(g_PerStackInterval);
                            size_t stackBytes = sizeof(uintptr_t) * g_StackMaxCount;
                            uintptr_t *stackArray = (uintptr_t *) malloc(stackBytes);
                            if (stackArray == NULL) {
                                continue;
                            }
                            __block size_t nSum = 0;
                            memset(stackArray, 0, stackBytes);
                            [WCGetMainThreadUtil getCurrentMainThreadStack:^(NSUInteger pc) {
                                stackArray[nSum] = (uintptr_t) pc;
                                nSum++;
                            }
                                                            withMaxEntries:g_StackMaxCount
                                                           withThreadCount:g_CurrentThreadCount];
                            [m_pointMainThreadHandler addThreadStack:stackArray andStackCount:nSum];
                        }
                    }
                } else {
                    usleep(g_CheckPeriodTime);
                }
            }

            if (m_bStop) {
                break;
            }
        }
    }
}

- (EDumpType)check
{
    // 1. runloop time out

    BOOL tmp_g_bRun = g_bRun;
    struct timeval tmp_g_tvRun = g_tvRun;

    struct timeval tvCur;
    gettimeofday(&tvCur, NULL);
    unsigned long long diff = [WCBlockMonitorMgr diffTime:&tmp_g_tvRun endTime:&tvCur];

#if TARGET_OS_OSX
    BOOL tmp_g_bEventStart = g_eventStart;
    struct timeval tmp_g_tvEvent = g_tvEvent;
    unsigned long long eventDiff = [WCBlockMonitorMgr diffTime:&tmp_g_tvEvent endTime:&tvCur];
#endif

#if !TARGET_OS_OSX
    struct timeval tmp_g_tvSuspend = g_tvSuspend;
    if (__timercmp(&tmp_g_tvSuspend, &tmp_g_tvRun, >)) {
        MatrixInfo(@"suspend after run, filter");
        return EDumpType_Unlag;
    }
#endif
   
    m_blockDiffTime = 0;

    if (tmp_g_bRun && tmp_g_tvRun.tv_sec && tmp_g_tvRun.tv_usec && __timercmp(&tmp_g_tvRun, &tvCur, <) && diff > g_RunLoopTimeOut) {
        m_blockDiffTime = diff;
#if TARGET_OS_OSX
        MatrixInfo(@"check run loop time out %u bRun %d runloopActivity %lu block diff time %llu",
                   g_RunLoopTimeOut, g_bRun, g_runLoopActivity, diff);
#endif
        
#if !TARGET_OS_OSX
        MatrixInfo(@"check run loop time out %u %ld bRun %d runloopActivity %lu block diff time %llu",
                   g_RunLoopTimeOut, (long) m_currentState, g_bRun, g_runLoopActivity, diff);
        
        if (g_bBackgroundLaunch) {
            MatrixInfo(@"background launch, filter");
            return EDumpType_Unlag;
        }
        
        if (m_currentState == UIApplicationStateBackground) {
            if (g_enterBackground.tv_sec != 0 || g_enterBackground.tv_usec != 0) {
                unsigned long long enterBackgroundTime = [WCBlockMonitorMgr diffTime:&g_enterBackground endTime:&tvCur];
                if (__timercmp(&g_enterBackground, &tvCur, <) && (enterBackgroundTime > APP_SHOULD_SUSPEND)) {
                    MatrixInfo(@"may mistake block %lld", enterBackgroundTime);
                    return EDumpType_Unlag;
                }
            }

            return EDumpType_BackgroundMainThreadBlock;
        }
#endif
        return EDumpType_MainThreadBlock;
    }
    
#if TARGET_OS_OSX
    if (tmp_g_bEventStart && tmp_g_tvEvent.tv_sec && tmp_g_tvEvent.tv_usec && __timercmp(&tmp_g_tvEvent, &tvCur, <) && eventDiff > g_RunLoopTimeOut) {
        m_blockDiffTime = eventDiff;
        MatrixInfo(@"check event time out %u bRun %d",g_RunLoopTimeOut, g_eventStart);
        return EDumpType_MainThreadBlock;
    }
#endif
    
    // 2. cpu usage
    
    float cpuUsage = 0.;
    if (m_powerConsumeStackCollector == nil) {
        cpuUsage = [WCPowerConsumeStackCollector getCurrentCPUUsage];
    } else {
        cpuUsage = [m_powerConsumeStackCollector getCPUUsageAndPowerConsumeStack];
    }
    
    if ([_monitorConfigHandler getShouldPrintCPUUsage] && cpuUsage > 40.0f) {
        MatrixInfo(@"mb[%f]", cpuUsage);;
    }
    
    if (m_bTrackCPU) {
        unsigned long long checkPeriod = [WCBlockMonitorMgr diffTime:&g_lastCheckTime endTime:&tvCur];
        gettimeofday(&g_lastCheckTime, NULL);
        if ([m_cpuHandler cultivateCpuUsage:cpuUsage periodTime:(float)checkPeriod/1000000]) {
            MatrixInfo(@"exceed cpu average usage");
            if (m_powerConsumeStackCollector) {
                [m_powerConsumeStackCollector makeConclusion];
            }
            BM_SAFE_CALL_SELECTOR_NO_RETURN(_delegate, @selector(onBlockMonitorIntervalCPUTooHigh:), onBlockMonitorIntervalCPUTooHigh:self)
        }
        if (cpuUsage > g_CPUUsagePercent) {
            MatrixInfo(@"check cpu over usage dump %f", cpuUsage);
            BM_SAFE_CALL_SELECTOR_NO_RETURN(_delegate, @selector(onBlockMonitorCurrentCPUTooHigh:), onBlockMonitorCurrentCPUTooHigh:self)
            if ([_monitorConfigHandler getShouldGetCPUHighLog]) {
                if (m_powerConsumeStackCollector && [m_powerConsumeStackCollector isCPUHighBlock]) {
                    return EDumpType_CPUBlock;
                }
            }
        }
    }

    // 3. print memory
    
    if (m_printMemoryTickTok < PRINT_MEMORY_USE_INTERVAL) {
        if ((m_printMemoryTickTok % PRINT_MEMORY_USE_INTERVAL) == 0) {
            int64_t footprint = [WCBlockMonitorMgr getFootprintResidentMemory];
            int64_t footprintMB = footprint / 1024 / 1024;
            if (footprintMB > 200) {
                MatrixInfo(@"check memory footprint %llu MB", footprintMB);
            }
            MatrixDebug(@"check memory footprint %llu MB", footprintMB);
        }
        m_printMemoryTickTok += 1;
        if (m_printMemoryTickTok == PRINT_MEMORY_USE_INTERVAL) {
            m_printMemoryTickTok = 0;
        }
    }
    
    // 4. no lag
    return EDumpType_Unlag;
}

- (EFilterType)needFilter
{
    BOOL bIsSame = NO;
    static std::vector<NSUInteger> vecCallStack(300);
    __block NSUInteger nSum = 0;
    __block NSUInteger stackFeat = 0; // use the top stack address;
    
    if (g_MainThreadHandle) {
        nSum = [m_pointMainThreadHandler getLastMainThreadStackCount];
        uintptr_t *stack = [m_pointMainThreadHandler getLastMainThreadStack];
        if (stack) {
            for (size_t i = 0; i < nSum; i++) {
                vecCallStack[i] = stack[i];
            }
            stackFeat = kssymbolicate_symboladdress(stack[0]);
        } else {
            nSum = 0;
        }
    } else {
        [WCGetMainThreadUtil getCurrentMainThreadStack:^(NSUInteger pc) {
            if (nSum < WXGBackTraceMaxEntries) {
                vecCallStack[nSum] = pc;
            }
            if (nSum == 0) {
                stackFeat = kssymbolicate_symboladdress(pc);
            }
            nSum++;
        }];
    }

    if (nSum <= 1) {
        MatrixInfo(@"filter meaningless stack");
        return EFilterType_Meaningless;
    }

    if (nSum == m_lastMainThreadStackCount) {
        NSUInteger index = 0;
        for (index = 0; index < nSum; index++) {
            if (vecCallStack[index] != m_vecLastMainThreadCallStack[index]) {
                break;
            }
        }
        if (index == nSum) {
            bIsSame = YES;
        }
    }

    if (bIsSame) {
        NSUInteger lastTimeInterval = m_nIntervalTime;
        m_nIntervalTime = m_nLastTimeInterval + m_nIntervalTime;
        m_nLastTimeInterval = lastTimeInterval;
        MatrixInfo(@"call stack same timeinterval = %lu", (unsigned long) m_nIntervalTime);
        return EFilterType_Annealing;
    } else {
        m_nIntervalTime = 1;
        m_nLastTimeInterval = 1;

        //update last call stack
        m_vecLastMainThreadCallStack.clear();
        m_lastMainThreadStackCount = 0;
        for (NSUInteger index = 0; index < nSum; index++) {
            m_vecLastMainThreadCallStack.push_back(vecCallStack[index]);
            m_lastMainThreadStackCount++;
        }
        
        if (g_filterSameStack) {
            NSUInteger repeatCnt = [m_stackHandler addStackFeat:stackFeat];
            if (repeatCnt > g_triggerdFilterSameCnt) {
                MatrixInfo(@"call stack appear too much today, repeat conut:[%u]",(uint32_t) repeatCnt);
                return EFilterType_TrigerByTooMuch;
            }
        }
        MatrixInfo(@"call stack diff");
        return EFilterType_None;
    }
}

- (void)resetStatus
{
    m_nIntervalTime = 1;
    m_nLastTimeInterval = 1;
    m_blockDiffTime = 0;
    m_vecLastMainThreadCallStack.clear();
    m_lastMainThreadStackCount = 0;
    m_potenHandledLagFile = nil;
}

// ============================================================================
#pragma mark - Runloop Observer & Call back
// ============================================================================

- (void)addRunLoopObserver
{
    NSRunLoop *curRunLoop = [NSRunLoop currentRunLoop];

    // the first observer
    CFRunLoopObserverContext context = {0, (__bridge void *) self, NULL, NULL, NULL};
    CFRunLoopObserverRef beginObserver = CFRunLoopObserverCreate(kCFAllocatorDefault, kCFRunLoopAllActivities, YES, LONG_MIN, &myRunLoopBeginCallback, &context);
    CFRetain(beginObserver);
    m_runLoopBeginObserver = beginObserver;

    // the last observer
    CFRunLoopObserverRef endObserver = CFRunLoopObserverCreate(kCFAllocatorDefault, kCFRunLoopAllActivities, YES, LONG_MAX, &myRunLoopEndCallback, &context);
    CFRetain(endObserver);
    m_runLoopEndObserver = endObserver;

    CFRunLoopRef runloop = [curRunLoop getCFRunLoop];
    CFRunLoopAddObserver(runloop, beginObserver, kCFRunLoopCommonModes);
    CFRunLoopAddObserver(runloop, endObserver, kCFRunLoopCommonModes);

    // for InitializationRunLoopMode
    CFRunLoopObserverContext initializationContext = {0, (__bridge void *) self, NULL, NULL, NULL};
    m_initializationBeginRunloopObserver = CFRunLoopObserverCreate(kCFAllocatorDefault, kCFRunLoopAllActivities, YES, LONG_MIN, &myInitializetionRunLoopBeginCallback, &initializationContext);
    CFRetain(m_initializationBeginRunloopObserver);

    m_initializationEndRunloopObserver = CFRunLoopObserverCreate(kCFAllocatorDefault, kCFRunLoopAllActivities, YES, LONG_MAX, &myInitializetionRunLoopEndCallback, &initializationContext);
    CFRetain(m_initializationEndRunloopObserver);

    CFRunLoopAddObserver(runloop, m_initializationBeginRunloopObserver, (CFRunLoopMode) @"UIInitializationRunLoopMode");
    CFRunLoopAddObserver(runloop, m_initializationEndRunloopObserver, (CFRunLoopMode) @"UIInitializationRunLoopMode");
}

- (void)removeRunLoopObserver
{
    NSRunLoop *curRunLoop = [NSRunLoop currentRunLoop];

    CFRunLoopRef runloop = [curRunLoop getCFRunLoop];
    CFRunLoopRemoveObserver(runloop, m_runLoopBeginObserver, kCFRunLoopCommonModes);
    CFRunLoopRemoveObserver(runloop, m_runLoopBeginObserver, (CFRunLoopMode) @"UIInitializationRunLoopMode");

    CFRunLoopRemoveObserver(runloop, m_runLoopEndObserver, kCFRunLoopCommonModes);
    CFRunLoopRemoveObserver(runloop, m_runLoopEndObserver, (CFRunLoopMode) @"UIInitializationRunLoopMode");
}

void myRunLoopBeginCallback(CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *info)
{
    g_runLoopActivity = activity;
    g_runLoopMode = eRunloopDefaultMode;
    switch (activity) {
        case kCFRunLoopEntry:
            g_bRun = YES;
            break;

        case kCFRunLoopBeforeTimers:
            if (g_bRun == NO) {
                gettimeofday(&g_tvRun, NULL);
            }
            g_bRun = YES;
            break;

        case kCFRunLoopBeforeSources:
            if (g_bRun == NO) {
                gettimeofday(&g_tvRun, NULL);
            }
            g_bRun = YES;
            break;

        case kCFRunLoopAfterWaiting:
            if (g_bRun == NO) {
                gettimeofday(&g_tvRun, NULL);
            }
            g_bRun = YES;
            break;

        case kCFRunLoopAllActivities:
            break;

        default:
            break;
    }
}

void myRunLoopEndCallback(CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *info)
{
    g_runLoopActivity = activity;
    g_runLoopMode = eRunloopDefaultMode;
    switch (activity) {
        case kCFRunLoopBeforeWaiting:
            gettimeofday(&g_tvRun, NULL);
            g_bRun = NO;
            break;

        case kCFRunLoopExit:
            g_bRun = NO;
            break;

        case kCFRunLoopAllActivities:
            break;

        default:
            break;
    }
}

void myInitializetionRunLoopBeginCallback(CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *info)
{
    g_runLoopActivity = activity;
    g_runLoopMode = eRunloopInitMode;
    switch (activity) {
        case kCFRunLoopEntry:
            g_bRun = YES;
            g_bLaunchOver = NO;
            break;

        case kCFRunLoopBeforeTimers:
            gettimeofday(&g_tvRun, NULL);
            g_bRun = YES;
            g_bLaunchOver = NO;
            break;

        case kCFRunLoopBeforeSources:
            gettimeofday(&g_tvRun, NULL);
            g_bRun = YES;
            g_bLaunchOver = NO;
            break;

        case kCFRunLoopAfterWaiting:
            gettimeofday(&g_tvRun, NULL);
            g_bRun = YES;
            g_bLaunchOver = NO;
            break;

        case kCFRunLoopAllActivities:
            break;
        default:
            break;
    }
}

void myInitializetionRunLoopEndCallback(CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *info)
{
    g_runLoopActivity = activity;
    g_runLoopMode = eRunloopInitMode;
    switch (activity) {
        case kCFRunLoopBeforeWaiting:
            gettimeofday(&g_tvRun, NULL);
            g_bRun = NO;
            g_bLaunchOver = YES;
            break;

        case kCFRunLoopExit:
            g_bRun = NO;
            g_bLaunchOver = YES;
            break;

        case kCFRunLoopAllActivities:
            break;

        default:
            break;
    }
}

// ============================================================================
#pragma mark - NSApplicationEvent
// ============================================================================

#if TARGET_OS_OSX
    
+ (void)signalEventStart
{
    gettimeofday(&g_tvEvent, NULL);
    g_eventStart = YES;
}
    
+ (void)signalEventEnd
{
    g_eventStart = NO;
}

#endif
    
// ============================================================================
#pragma mark - Lag File
// ============================================================================

- (NSString *)dumpFileWithType:(EDumpType)dumpType
{
    NSString *dumpFileName = @"";
    if (g_bLaunchOver) {
        BM_SAFE_CALL_SELECTOR_NO_RETURN(_delegate,
                                        @selector(onBlockMonitor:beginDump:blockTime:),
                                        onBlockMonitor:self beginDump:dumpType blockTime:m_blockDiffTime);
        dumpFileName = [WCDumpInterface dumpReportWithReportType:dumpType withBlockTime:m_blockDiffTime];
    } else {
        BM_SAFE_CALL_SELECTOR_NO_RETURN(_delegate,
                                        @selector(onBlockMonitor:beginDump:blockTime:),
                                        onBlockMonitor:self beginDump:EDumpType_LaunchBlock blockTime:m_blockDiffTime);
        dumpFileName = [WCDumpInterface dumpReportWithReportType:EDumpType_LaunchBlock withBlockTime:m_blockDiffTime];
        NSString *filePath = [WCCrashBlockFileHandler getLaunchBlockRecordFilePath];
        NSData *infoData = [NSData dataWithContentsOfFile:filePath];
        if (infoData.length > 0) {
            NSString *alreadyHaveString = [[NSString alloc] initWithData:infoData encoding:NSUTF8StringEncoding];
            alreadyHaveString = [NSString stringWithFormat:@"%@,%@", alreadyHaveString, dumpFileName];
            MatrixInfo(@"current launch lag file: %@", alreadyHaveString);
            [alreadyHaveString writeToFile:filePath atomically:NO encoding:NSUTF8StringEncoding error:nil];
        } else {
            MatrixInfo(@"current launch lag file: %@", dumpFileName);
            [dumpFileName writeToFile:filePath atomically:NO encoding:NSUTF8StringEncoding error:nil];
        }
    }
    return dumpFileName;
}

// ============================================================================
#pragma mark - Launch Lag
// ============================================================================

- (void)clearLaunchLagRecord
{
    NSString *filePath = [WCCrashBlockFileHandler getLaunchBlockRecordFilePath];
    if ([[NSFileManager defaultManager] fileExistsAtPath:filePath]) {
        [[NSFileManager defaultManager] removeItemAtPath:filePath error:nil];
        MatrixInfo(@"launch over, clear dump file record");
    }
}

- (void)clearDumpInBackgroundLaunch
{
    NSString *filePath = [WCCrashBlockFileHandler getLaunchBlockRecordFilePath];
    NSString *infoString = [NSString stringWithContentsOfFile:filePath encoding:NSUTF8StringEncoding error:nil];
    NSFileManager *fileMgr = [NSFileManager defaultManager];
    if (infoString && infoString.length > 0) {
        NSArray *dumpFileArray = [infoString componentsSeparatedByString:@","];
        for (NSString *dumpFile in dumpFileArray) {
            [fileMgr removeItemAtPath:dumpFile error:nil];
            MatrixWarning(@"remove wrong launch lag: %@", dumpFile);
        }
    }
    [fileMgr removeItemAtPath:filePath error:nil];
}

// ============================================================================
#pragma mark - CPU
// ============================================================================
    
- (void)startTrackCPU
{
    m_bTrackCPU = YES;
}
    
- (void)stopTrackCPU
{
    m_bTrackCPU = NO;
}
    
- (BOOL)isBackgroundCPUTooSmall
{
    return [m_cpuHandler isBackgroundCPUTooSmall];
}

// ============================================================================
#pragma mark - WCPowerConsumeStackCollectorDelegate
// ============================================================================

- (void)powerConsumeStackCollectorConclude:(NSArray <NSDictionary *> *)stackTree
{
    dispatch_async(m_asyncDumpQueue, ^{
        if (stackTree == nil) {
            MatrixInfo(@"save battery cost stack log, but stack tree is empty");
            return;
        }
        MatrixInfo(@"save battery cost stack log");
        NSString *reportID = [[NSUUID UUID] UUIDString];
        NSData *reportData = [WCGetCallStackReportHandler getReportJsonDataWithPowerConsumeStack:stackTree
                                                                                    withReportID:reportID
                                                                                    withDumpType:EDumpType_PowerConsume];
        [WCDumpInterface saveDump:reportData withReportType:EDumpType_PowerConsume withReportID:reportID];
    });
}

// ============================================================================
#pragma mark - Utility
// ============================================================================

+ (unsigned long long)diffTime:(struct timeval *)tvStart endTime:(struct timeval *)tvEnd
{
    return 1000000 * (tvEnd->tv_sec - tvStart->tv_sec) + tvEnd->tv_usec - tvStart->tv_usec;
}

+ (int64_t)getFootprintResidentMemory
{
    int64_t memoryUsageInByte = 0;
    task_vm_info_data_t vmInfo;
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    kern_return_t kernelReturn = task_info(mach_task_self(), TASK_VM_INFO, (task_info_t) &vmInfo, &count);
    if(kernelReturn == KERN_SUCCESS) {
        memoryUsageInByte = (int64_t) vmInfo.phys_footprint;
    } else {
        MatrixError(@"Error with task_info(): %s", mach_error_string(kernelReturn));
    }
    return memoryUsageInByte;
}

@end
