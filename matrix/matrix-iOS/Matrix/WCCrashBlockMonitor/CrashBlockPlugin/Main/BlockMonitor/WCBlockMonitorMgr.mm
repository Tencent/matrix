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
#import <mach/mach_time.h>
#import <vector>
#import <algorithm>

#import "MatrixLogDef.h"
#import "MatrixDeviceInfo.h"
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
#import "logger_internal.h"


// ============================================================================
#pragma mark - cpu frequency
// ============================================================================

#if defined(__arm64__)

// tries to estimate the frequency, returns 0 on failure
double measure_frequency() {
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    const size_t test_duration_in_cycles =
    65536;// 1048576;
    // travis feels strongly about the measure-twice-and-subtract trick.
    auto begin1 = mach_absolute_time();
    size_t cycles = 2 * test_duration_in_cycles;
    
    __asm volatile(
                   ".align 4\n Lcyclemeasure1:\nsubs %[counter],%[counter],#1\nbne Lcyclemeasure1\n "
                   : /* read/write reg */ [counter] "+r"(cycles));
    auto end1 = mach_absolute_time();
    double nanoseconds1 =
    (double) (end1 - begin1) * (double)info.numer / (double)info.denom;
    
    auto begin2 = mach_absolute_time();
    cycles = test_duration_in_cycles;
    // I think that this will have a 2-cycle latency on ARM?
    __asm volatile(
                   ".align 4\n Lcyclemeasure2:\nsubs %[counter],%[counter],#1\nbne Lcyclemeasure2\n "
                   : /* read/write reg */ [counter] "+r"(cycles));
    auto end2 = mach_absolute_time();
    double nanoseconds2 =
    (double) (end2 - begin2) * (double)info.numer / (double)info.denom;
    double nanoseconds = (nanoseconds1 - nanoseconds2);
    if ((fabs(nanoseconds - nanoseconds1 / 2) > 0.05 * nanoseconds) or
        (fabs(nanoseconds - nanoseconds2) > 0.05 * nanoseconds)) {
        return 0;
    }
    double frequency = double(test_duration_in_cycles) / nanoseconds;
    return frequency;
}

//return GHz
double cpu_frequency()
{
    double result = 0;
    size_t attempt = 1000;
    std::vector<double> freqs;
    for (int i = 0; i < attempt; i++) {
        double freq = measure_frequency();
        if(freq > 0) freqs.push_back(freq);
    }
    if(freqs.size() == 0) {
        return result;
    }
    std::sort(freqs.begin(),freqs.end());
    result = freqs[freqs.size() / 2];
    return result;
}

#endif

#if !TARGET_OS_OSX
#import <UIKit/UIKit.h>
#endif

#if TARGET_OS_OSX
#import "NSApplicationEvent.h"
#endif

static useconds_t g_RunLoopTimeOut = g_defaultRunLoopTimeOut;
static useconds_t g_CheckPeriodTime = g_defaultCheckPeriodTime;
static float g_CPUUsagePercent = 1000; // a big cpu percent for preventing from wrong setting.
static useconds_t g_PerStackInterval = g_defaultPerStackInterval;
static size_t g_StackMaxCount = 100;
static BOOL g_bSensitiveRunloopHangDetection = NO;

static NSUInteger g_CurrentThreadCount = 0;
static BOOL g_MainThreadHandle = NO;
static BOOL g_MainThreadProfile = NO;
static int g_MainThreadCount = 0;
static KSStackCursor *g_PointMainThreadArray = NULL;
static int *g_PointMainThreadRepeatCountArray = NULL;
static char *g_PointMainThreadProfile = NULL;
static KSStackCursor **g_PointCPUHighThreadArray = NULL;
static int g_PointCpuHighThreadCount = 0;
static float *g_PointCpuHighThreadValueArray = NULL;

static BOOL g_filterSameStack = NO;
static uint32_t g_triggerdFilterSameCnt = 0;
static BOOL g_runloopThresholdUpdated = NO;

API_AVAILABLE(ios(11.0))
static NSProcessInfoThermalState g_thermalState = NSProcessInfoThermalStateNominal;

#define APP_SHOULD_SUSPEND (180 * BM_MicroFormat_Second)

#define PRINT_MEMORY_USE_INTERVAL (5 * BM_MicroFormat_Second)
#define PRINT_CPU_FREQUENCY_INTERVAL (10 * BM_MicroFormat_Second)

#define __timercmp(tvp, uvp, cmp) (((tvp)->tv_sec == (uvp)->tv_sec) ? ((tvp)->tv_usec cmp(uvp)->tv_usec) : ((tvp)->tv_sec cmp(uvp)->tv_sec))

#define BM_SAFE_CALL_SELECTOR_NO_RETURN(obj, sel, func) \
    {                                                   \
        if (obj && [obj respondsToSelector:sel]) {      \
            [obj func];                                 \
        }                                               \
    }

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
static BOOL g_bInSuspend = NO;

typedef enum : NSUInteger {
    eRunloopInitMode,
    eRunloopDefaultMode,
} ERunloopMode;

static ERunloopMode g_runLoopMode;

void exitCallBack() {
    [[WCBlockMonitorMgr shareInstance] stop];
}

KSStackCursor *kscrash_pointThreadCallback(void) {
    return g_PointMainThreadArray;
}

int *kscrash_pointThreadRepeatNumberCallback(void) {
    return g_PointMainThreadRepeatCountArray;
}

char *kscrash_pointThreadProfileCallback(void) {
    return g_PointMainThreadProfile;
}

KSStackCursor **kscrash_pointCPUHighThreadCallback(void) {
    return g_PointCPUHighThreadArray;
}

int kscrash_pointCpuHighThreadCountCallback(void) {
    return g_PointCpuHighThreadCount;
}

float *kscrash_pointCpuHighThreadArrayCallBack(void) {
    return g_PointCpuHighThreadValueArray;
}

@interface WCBlockMonitorMgr () <WCPowerConsumeStackCollectorDelegate> {
    NSThread *m_monitorThread;
    BOOL m_bStop;

#if !TARGET_OS_OSX
    UIApplicationState m_currentState;
#endif

    //  ================================================

    // 统一修改成为 ms
    NSUInteger m_nIntervalTime; // 每一轮检查check的时间间隔。（在 m_nIntervalTime 的时间段内，会持续去获取耗时堆栈）
    NSUInteger m_nLastTimeInterval; // 上一轮检查check的时间间隔（用于退火算法）

    std::vector<NSUInteger> m_vecLastMainThreadCallStack;
    NSUInteger m_lastMainThreadStackCount;

    // ================================================

    uint64_t m_blockDiffTime;

    uint32_t m_firstSleepTime;

    NSString *m_potenHandledLagFile;

    WCMainThreadHandler *m_pointMainThreadHandler;

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
    uint32_t m_printCPUFrequencyTickTok;

    BOOL m_suspendAllThreads;
    BOOL m_enableSnapshot;
}

@property (nonatomic, strong) WCBlockMonitorConfigHandler *monitorConfigHandler;

#if TARGET_OS_OSX
@property (nonatomic, strong) NSApplicationEvent *eventHandler;
#endif

@end

@implementation WCBlockMonitorMgr

+ (WCBlockMonitorMgr *)shareInstance {
    static WCBlockMonitorMgr *g_monitorMgr = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        g_monitorMgr = [[WCBlockMonitorMgr alloc] init];
    });
    return g_monitorMgr;
}

- (void)resetConfiguration:(WCBlockMonitorConfiguration *)bmConfig {
    _monitorConfigHandler = [[WCBlockMonitorConfigHandler alloc] init];
    [_monitorConfigHandler setConfiguration:bmConfig];
}

- (id)init {
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

- (void)dealloc {
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

- (void)freeCpuHighThreadArray {
    for (int i = 0; i < g_PointCpuHighThreadCount; i++) {
        if (g_PointCPUHighThreadArray[i] != NULL) {
            KSStackCursor_Backtrace_Context *context = (KSStackCursor_Backtrace_Context *)g_PointCPUHighThreadArray[i]->context;
            if (context->backtrace != NULL) {
                free((uintptr_t *)context->backtrace);
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

- (void)start {
    if (!m_bStop) {
        return;
    }

    g_bMonitor = YES;

    g_MainThreadHandle = [_monitorConfigHandler getMainThreadHandle];
    g_MainThreadProfile = [_monitorConfigHandler getMainThreadProfile];
    [self setRunloopThreshold:[_monitorConfigHandler getRunloopTimeOut] isFirstTime:YES];
    [self setCPUUsagePercent:[_monitorConfigHandler getCPUUsagePercent]];
    [self setPerStackInterval:[_monitorConfigHandler getPerStackInterval]];

    m_nIntervalTime = g_CheckPeriodTime;
    m_nLastTimeInterval = m_nIntervalTime;
    m_blockDiffTime = 0;
    m_firstSleepTime = 5;
    gettimeofday(&g_tvSuspend, NULL);
    g_enterBackground = { 0, 0 };
    gettimeofday(&g_lastCheckTime, NULL);

    if ([_monitorConfigHandler getShouldPrintMemoryUse]) {
        m_printMemoryTickTok = 0;
    } else {
        m_printMemoryTickTok = 6 * BM_MicroFormat_Second;
    }
    if ([_monitorConfigHandler getShouldPrintCPUFrequency]) {
        m_printCPUFrequencyTickTok = 0;
    } else {
        m_printCPUFrequencyTickTok = 11 * BM_MicroFormat_Second;
    }

    // TODO: g_mainThreadCount 应该根据检测时间、检测时间差来决定
    //    g_MainThreadCount = [_monitorConfigHandler getMainThreadCount];
    g_MainThreadCount =
    g_CheckPeriodTime
    / g_PerStackInterval; // 检查一轮耗时堆栈的时间与单次堆栈检查时间的比值，作为此次一轮检查的耗时堆栈总数，也是循环队列需要保存的数量
    m_pointMainThreadHandler = [[WCMainThreadHandler alloc] initWithCycleArrayCount:g_MainThreadCount];
    g_StackMaxCount = [m_pointMainThreadHandler getStackMaxCount];

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

    g_bSensitiveRunloopHangDetection = [_monitorConfigHandler getSensitiveRunloopHangDetection];

    m_suspendAllThreads = [_monitorConfigHandler getShouldSuspendAllThreads];
    m_enableSnapshot = [_monitorConfigHandler getShouldEnableSnapshot];

    if ([_monitorConfigHandler getShouldGetDiskIOStack]) {
        // delete the IO Disk Stack Collector
    }

#if !TARGET_OS_OSX
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(willTerminate) name:UIApplicationWillTerminateNotification object:nil];
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

    if (@available(iOS 11.0, *)) {
        // Apple doc: To receive NSProcessInfoThermalStateDidChangeNotification, you must access the thermalState prior to registering for the notification.
        g_thermalState = [[NSProcessInfo processInfo] thermalState];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(thermalStateDidChange)
                                                     name:NSProcessInfoThermalStateDidChangeNotification
                                                   object:nil];
    }
#endif

    atexit(exitCallBack);

    [self addRunLoopObserver];
    [self addMonitorThread];

#if TARGET_OS_OSX
    self.eventHandler = [[NSApplicationEvent alloc] init];
    [self.eventHandler tryswizzle];
#endif
}

- (void)stop {
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

- (void)handleBackgroundLaunch {
    g_bMonitor = NO;
    g_bBackgroundLaunch = YES;
}

- (void)handleSuspend {
    g_bMonitor = NO;
    gettimeofday(&g_tvSuspend, NULL);
    g_bInSuspend = YES;
}

#endif

- (NSDictionary *)getUserInfoForCurrentDumpForDumpType:(EDumpType)dumpType {
    if (_delegate != nil && [_delegate respondsToSelector:@selector(onBlockMonitor:getCustomUserInfoForDumpType:)]) {
        return [_delegate onBlockMonitor:self getCustomUserInfoForDumpType:dumpType];
    }
    return nil;
}

// ============================================================================
#pragma mark - Application State (Notification Observer)
// ============================================================================

#if !TARGET_OS_OSX
- (void)willTerminate {
    [self stop];
}

- (void)didBecomeActive {
    MatrixInfo(@"did become active");

    g_enterBackground = { 0, 0 };

    g_bInSuspend = NO;
    m_currentState = [UIApplication sharedApplication].applicationState;

    g_bMonitor = YES;
    g_bLaunchOver = YES;

    if (g_bBackgroundLaunch) {
        [self clearDumpInBackgroundLaunch];
        g_bBackgroundLaunch = NO;
    }

    [self clearLaunchLagRecord];
}

- (void)didEnterBackground {
    MatrixInfo(@"did enter background");
    gettimeofday(&g_enterBackground, NULL);
    m_currentState = [UIApplication sharedApplication].applicationState;
}

- (void)willResignActive {
    MatrixInfo(@"will resign active");
    m_currentState = [UIApplication sharedApplication].applicationState;
    g_bLaunchOver = YES;
}

- (void)thermalStateDidChange {
    MatrixDebug(@"thermal state did change");

    if (@available(iOS 11.0, *)) {
        // On iOS 15.0.2, Foundation.framework might post ThermalStateDidChangeNotification from -[NSProcessInfo thermalState],
        // recursively calling -[NSProcessInfo thermalState] in the notification's observer could cause a crash.
        // Dispatch it as a workaround. FB9802727 to Apple. Already fixed on iOS 15.2.
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            NSProcessInfoThermalState currentThermalState = [[NSProcessInfo processInfo] thermalState];
            if (currentThermalState > g_thermalState) {
                BM_SAFE_CALL_SELECTOR_NO_RETURN(self.delegate,
                                                @selector(onBlockMonitorThermalStateElevated:),
                                                onBlockMonitorThermalStateElevated:self);
            }
            g_thermalState = currentThermalState;
        });
    }
}

#endif

// ============================================================================
#pragma mark - Config
// ============================================================================

- (void)setCPUUsagePercent:(float)usagePercent {
    float tmpUsagePercent = g_CPUUsagePercent;
    g_CPUUsagePercent = usagePercent;
    MatrixInfo(@"set cpuusage before[%lf] after[%lf]", tmpUsagePercent, g_CPUUsagePercent);
}

- (void)setPerStackInterval:(useconds_t)perStackInterval {
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

- (void)addMonitorThread {
    m_bStop = NO;
    m_monitorThread = [[NSThread alloc] initWithTarget:self selector:@selector(threadProc) object:nil];
    [m_monitorThread start];
}

- (void)threadProc {
    if (m_firstSleepTime) {
        sleep(m_firstSleepTime);
        m_firstSleepTime = 0;
    }

    if (g_filterSameStack) {
        m_stackHandler = [[WCFilterStackHandler alloc] init];
    }

    set_curr_thread_ignore_logging(true);

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
                    if (EDumpType_BackgroundMainThreadBlock == dumpType || EDumpType_MainThreadBlock == dumpType) {
                        EFilterType filterType = [self needFilter];
                        if (filterType == EFilterType_None) {
                            if (g_MainThreadHandle) {
                                if (g_PointMainThreadArray != NULL) {
                                    free(g_PointMainThreadArray);
                                    g_PointMainThreadArray = NULL;
                                }
                                if (g_MainThreadProfile) {
                                    g_PointMainThreadProfile = [m_pointMainThreadHandler getStackProfile];
                                }
                                g_PointMainThreadArray = [m_pointMainThreadHandler getPointStackCursor];
                                g_PointMainThreadRepeatCountArray = [m_pointMainThreadHandler getPointStackRepeatCount];
                                m_potenHandledLagFile = [self dumpFileWithType:dumpType];
                                BM_SAFE_CALL_SELECTOR_NO_RETURN(_delegate,
                                                                @selector(onBlockMonitor:getDumpFile:withDumpType:),
                                                                onBlockMonitor:self getDumpFile:m_potenHandledLagFile withDumpType:dumpType);
                                if (g_PointMainThreadArray != NULL) {
                                    KSStackCursor_Backtrace_Context *context = (KSStackCursor_Backtrace_Context *)g_PointMainThreadArray->context;
                                    if (context->backtrace) {
                                        free((uintptr_t *)context->backtrace);
                                    }
                                    free(g_PointMainThreadArray);
                                    g_PointMainThreadArray = NULL;
                                }
                                if (g_PointMainThreadProfile != NULL) {
                                    free(g_PointMainThreadProfile);
                                    g_PointMainThreadProfile = NULL;
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

                        BM_SAFE_CALL_SELECTOR_NO_RETURN(_delegate,
                                                        @selector(onBlockMonitorMainThreadBlock:),
                                                        onBlockMonitorMainThreadBlock:self);
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
                    } else {
                        m_potenHandledLagFile = [self dumpFileWithType:dumpType];
                    }
                } else {
                    [self resetStatus];
                }
            }

            [self recordCurrentStack];

            if (m_bStop) {
                break;
            }
        }
    }
}

- (void)recordCurrentStack {
    /**
     1、
     m_nIntervalTime: 获取耗时堆栈的总时间
     eg: 如果检查一轮耗时堆栈为1s, 当前时间间隔为 6s -> nCnt 6
         如果检查一轮耗时堆栈为500ms，当前时间间隔为 1.5s -> nCnt 3
     因此 nCnt 不应该是 < m_nIntervalTime，应该修正为 m_nIntervalTime / g_CheckPeriodTime
     
     2、
     g_CheckPeriodTime: 单次检查耗时堆栈的总时间
     eg: 这个是后台可动态配置的。也即为设置的卡顿阈值一半的时间；同样也是检查一轮耗时堆栈的时间。
     
     3、
     g_PerStackInterval: 获取一次耗时堆栈的时间。这个固定是 50ms
     */
    unsigned long nTotalCnt = m_nIntervalTime / g_CheckPeriodTime;
    for (int nCnt = 0; nCnt < nTotalCnt && !m_bStop; nCnt++) {
        if (g_MainThreadHandle && g_bMonitor) {
            int intervalCount = g_CheckPeriodTime / g_PerStackInterval; // 1s 每秒检查20次
            if (intervalCount <= 0) {
                usleep(g_CheckPeriodTime);
            } else {
                int mainThreadCheckTimes = 0;
                for (int index = 0; index < intervalCount && !m_bStop; index++) {
                    usleep(g_PerStackInterval);
                    size_t stackBytes = sizeof(uintptr_t) * g_StackMaxCount;
                    uintptr_t *stackArray = (uintptr_t *)malloc(stackBytes);
                    if (stackArray == NULL) {
                        continue;
                    }
                    __block size_t nSum = 0;
                    memset(stackArray, 0, stackBytes);
                    [WCGetMainThreadUtil
                    getCurrentMainThreadStack:^(NSUInteger pc) {
                        stackArray[nSum] = (uintptr_t)pc;
                        nSum++;
                    }
                               withMaxEntries:g_StackMaxCount
                              withThreadCount:g_CurrentThreadCount];
                    [m_pointMainThreadHandler addThreadStack:stackArray andStackCount:nSum];
                    mainThreadCheckTimes++;
                }
            }
        } else {
            usleep(g_CheckPeriodTime);
        }
    }
}

- (EDumpType)check {
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
        MatrixInfo(@"check run loop time out threshold %u, bRun %d, runloopActivity %lu, block diff time %llu", g_RunLoopTimeOut, g_bRun, g_runLoopActivity, diff);
#endif

#if !TARGET_OS_OSX
        MatrixInfo(@"check run loop time out threshold %u, application state %ld, bRun %d, runloopActivity %lu, block diff time %llu",
                   g_RunLoopTimeOut,
                   (long)m_currentState,
                   g_bRun,
                   g_runLoopActivity,
                   diff);

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
        MatrixInfo(@"check event time out %u bRun %d", g_RunLoopTimeOut, g_eventStart);
        return EDumpType_MainThreadBlock;
    }
#endif

    // 2. cpu usage

    float appCpuUsage = 0.;
    if (m_powerConsumeStackCollector == nil) {
        appCpuUsage = [MatrixDeviceInfo appCpuUsage];
    } else {
        appCpuUsage = [m_powerConsumeStackCollector getCPUUsageAndPowerConsumeStack];
    }

    float deviceCpuUsage = [MatrixDeviceInfo cpuUsage];

    if ([_monitorConfigHandler getShouldPrintCPUUsage] && (appCpuUsage > 40.0f || deviceCpuUsage > 40.0f)) {
        MatrixInfo(@"AppCpuUsage: %.2f, Device: %.2f", appCpuUsage, deviceCpuUsage * [MatrixDeviceInfo cpuCount]);
    }

    // 耗电堆栈检测 & CPU 消耗过高检测
    // 1. 耗电堆栈检测: 在耗电堆栈的内部会去记录时间是否60s，只有累积到60s并且CPU超过阈值才会上报
    // 2. CPU消耗过高: 这里根据检查的时间间隔就会判断
    if (m_bTrackCPU) {
        unsigned long long checkPeriod = [WCBlockMonitorMgr diffTime:&g_lastCheckTime endTime:&tvCur];
        gettimeofday(&g_lastCheckTime, NULL);
        if ([m_cpuHandler cultivateCpuUsage:appCpuUsage periodTime:(float)checkPeriod / 1000000]) {
            MatrixInfo(@"exceed cpu average usage");
            if (m_powerConsumeStackCollector) {
                [m_powerConsumeStackCollector makeConclusion];
            }
            BM_SAFE_CALL_SELECTOR_NO_RETURN(_delegate, @selector(onBlockMonitorIntervalCPUTooHigh:), onBlockMonitorIntervalCPUTooHigh:self)
        }
        if (appCpuUsage > g_CPUUsagePercent) {
            MatrixInfo(@"check cpu over usage dump %f", appCpuUsage);
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
            uint64_t footprint = matrix_footprintMemory();
            uint64_t footprintMB = footprint / 1024 / 1024;
            uint64_t available = matrix_availableMemory();
            uint64_t availableMB = available / 1024 / 1024;
            if (footprintMB > 400) {
                MatrixInfo(@"check memory footprint %llu MB, available: %llu MB", footprintMB, availableMB);
            } else {
                MatrixDebug(@"check memory footprint %llu MB, available: %llu MB", footprintMB, availableMB);
            }
            if (footprintMB > [_monitorConfigHandler getMemoryWarningThresholdInMB]) {
                BM_SAFE_CALL_SELECTOR_NO_RETURN(_delegate, @selector(onBlockMonitorMemoryExcessive:), onBlockMonitorMemoryExcessive:self);
            }
        }
        // 内存监控依赖于每次加上检查的时间间隔，这里应该修正为 g_CheckPeriodTime
        m_printMemoryTickTok += g_CheckPeriodTime; // 真正的这里应该修正为 m_nIntervalTime，但原有逻辑是+1，这里保持原样
        if (m_printMemoryTickTok >= PRINT_MEMORY_USE_INTERVAL) {
            m_printMemoryTickTok = 0;
        }
    }

    // 4. print cpu frequency
#if defined(__arm64__)
    if (m_printCPUFrequencyTickTok < PRINT_CPU_FREQUENCY_INTERVAL) {
        if ((m_printCPUFrequencyTickTok % PRINT_CPU_FREQUENCY_INTERVAL) == 0) {
            if (@available(iOS 11.0, *)) {
                NSProcessInfoThermalState state = [[NSProcessInfo processInfo] thermalState];
                if (state == NSProcessInfoThermalStateFair) {
                    MatrixInfo(@"check thermal state in 'level 1' high");
                } else if (state == NSProcessInfoThermalStateSerious) {
                    MatrixInfo(@"check thermal state in 'level 2' high");
                } else if (state == NSProcessInfoThermalStateCritical) {
                    MatrixInfo(@"check thermal state in 'level 3' high");
                } else {
                    MatrixInfo(@"check thermal state OK");
                }
            }
        }
        m_printCPUFrequencyTickTok += g_CheckPeriodTime;
        if (m_printCPUFrequencyTickTok >= PRINT_CPU_FREQUENCY_INTERVAL) {
            m_printCPUFrequencyTickTok = 0;
        }
    }
#endif
    // 5. no lag
    return EDumpType_Unlag;
}

- (EFilterType)needFilter {
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
        MatrixInfo(@"call stack same timeinterval = %lu", (unsigned long)m_nIntervalTime);
        return EFilterType_Annealing;
    } else {
        m_nIntervalTime = g_CheckPeriodTime;
        m_nLastTimeInterval = m_nIntervalTime;

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
                MatrixInfo(@"call stack appear too much today, repeat count:[%u]", (uint32_t)repeatCnt);
                return EFilterType_TrigerByTooMuch;
            }
        }
        MatrixInfo(@"call stack diff");
        return EFilterType_None;
    }
}

- (void)resetStatus {
    m_nIntervalTime = g_CheckPeriodTime;
    m_nLastTimeInterval = m_nIntervalTime;
    m_blockDiffTime = 0;
    m_vecLastMainThreadCallStack.clear();
    m_lastMainThreadStackCount = 0;
    m_potenHandledLagFile = nil;

    // 此处对 m_pointMainThreadHandler 的更改，和其他地方对它的读写，都在 m_monitorThread 线程里执行，避免线程安全问题
    if (g_runloopThresholdUpdated) {
        g_runloopThresholdUpdated = NO;
        g_MainThreadCount = g_CheckPeriodTime / g_PerStackInterval;
        m_pointMainThreadHandler = [[WCMainThreadHandler alloc] initWithCycleArrayCount:g_MainThreadCount];
    }
}

// ============================================================================
#pragma mark - Runloop Observer & Call back
// ============================================================================

- (void)addRunLoopObserver {
    NSRunLoop *curRunLoop = [NSRunLoop currentRunLoop];

    // the first observer
    CFRunLoopObserverContext context = { 0, (__bridge void *)self, NULL, NULL, NULL };
    CFRunLoopObserverRef beginObserver =
    CFRunLoopObserverCreate(kCFAllocatorDefault, kCFRunLoopAllActivities, YES, LONG_MIN, &myRunLoopBeginCallback, &context);
    CFRetain(beginObserver);
    m_runLoopBeginObserver = beginObserver;

    // the last observer
    CFRunLoopObserverRef endObserver =
    CFRunLoopObserverCreate(kCFAllocatorDefault, kCFRunLoopAllActivities, YES, LONG_MAX, &myRunLoopEndCallback, &context);
    CFRetain(endObserver);
    m_runLoopEndObserver = endObserver;

    CFRunLoopRef runloop = [curRunLoop getCFRunLoop];
    CFRunLoopAddObserver(runloop, beginObserver, kCFRunLoopCommonModes);
    CFRunLoopAddObserver(runloop, endObserver, kCFRunLoopCommonModes);

    // for InitializationRunLoopMode
    CFRunLoopObserverContext initializationContext = { 0, (__bridge void *)self, NULL, NULL, NULL };
    m_initializationBeginRunloopObserver = CFRunLoopObserverCreate(kCFAllocatorDefault,
                                                                   kCFRunLoopAllActivities,
                                                                   YES,
                                                                   LONG_MIN,
                                                                   &myInitializetionRunLoopBeginCallback,
                                                                   &initializationContext);
    CFRetain(m_initializationBeginRunloopObserver);

    m_initializationEndRunloopObserver =
    CFRunLoopObserverCreate(kCFAllocatorDefault, kCFRunLoopAllActivities, YES, LONG_MAX, &myInitializetionRunLoopEndCallback, &initializationContext);
    CFRetain(m_initializationEndRunloopObserver);

    CFRunLoopAddObserver(runloop, m_initializationBeginRunloopObserver, (CFRunLoopMode) @"UIInitializationRunLoopMode");
    CFRunLoopAddObserver(runloop, m_initializationEndRunloopObserver, (CFRunLoopMode) @"UIInitializationRunLoopMode");
}

- (void)removeRunLoopObserver {
    NSRunLoop *curRunLoop = [NSRunLoop currentRunLoop];

    CFRunLoopRef runloop = [curRunLoop getCFRunLoop];
    CFRunLoopRemoveObserver(runloop, m_runLoopBeginObserver, kCFRunLoopCommonModes);
    CFRunLoopRemoveObserver(runloop, m_runLoopBeginObserver, (CFRunLoopMode) @"UIInitializationRunLoopMode");

    CFRunLoopRemoveObserver(runloop, m_runLoopEndObserver, kCFRunLoopCommonModes);
    CFRunLoopRemoveObserver(runloop, m_runLoopEndObserver, (CFRunLoopMode) @"UIInitializationRunLoopMode");
}

void myRunLoopBeginCallback(CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *info) {
    g_runLoopActivity = activity;
    g_runLoopMode = eRunloopDefaultMode;
    switch (activity) {
        case kCFRunLoopEntry:
            g_bRun = YES;
            break;

        case kCFRunLoopBeforeTimers:
            if (g_bRun == NO && g_bInSuspend == NO) {
                gettimeofday(&g_tvRun, NULL);
            }
            g_bRun = YES;
            break;

        case kCFRunLoopBeforeSources:
            if (g_bRun == NO && g_bInSuspend == NO) {
                gettimeofday(&g_tvRun, NULL);
            }
            g_bRun = YES;
            break;

        case kCFRunLoopAfterWaiting:
            if (g_bRun == NO && g_bInSuspend == NO) {
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

void myRunLoopEndCallback(CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *info) {
    g_runLoopActivity = activity;
    g_runLoopMode = eRunloopDefaultMode;
    switch (activity) {
        case kCFRunLoopBeforeWaiting:
            if (g_bSensitiveRunloopHangDetection && g_bRun) {
                [WCBlockMonitorMgr checkRunloopDuration];
            }
            if (g_bInSuspend == NO) {
                gettimeofday(&g_tvRun, NULL);
            }
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

void myInitializetionRunLoopBeginCallback(CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *info) {
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

void myInitializetionRunLoopEndCallback(CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *info) {
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

+ (void)signalEventStart {
    gettimeofday(&g_tvEvent, NULL);
    g_eventStart = YES;
}

+ (void)signalEventEnd {
    g_eventStart = NO;
}

#endif

// ============================================================================
#pragma mark - Lag File
// ============================================================================

- (NSString *)dumpFileWithType:(EDumpType)dumpType {
    NSString *dumpFileName = @"";
    if (g_bLaunchOver) {
        BM_SAFE_CALL_SELECTOR_NO_RETURN(_delegate,
                                        @selector(onBlockMonitor:beginDump:blockTime:runloopThreshold:),
                                        onBlockMonitor:self beginDump:dumpType blockTime:m_blockDiffTime runloopThreshold:g_RunLoopTimeOut);
        dumpFileName = [WCDumpInterface dumpReportWithReportType:dumpType suspendAllThreads:m_suspendAllThreads enableSnapshot:m_enableSnapshot];
    } else {
        BM_SAFE_CALL_SELECTOR_NO_RETURN(_delegate,
                                        @selector(onBlockMonitor:beginDump:blockTime:runloopThreshold:),
                                        onBlockMonitor:self beginDump:EDumpType_LaunchBlock blockTime:m_blockDiffTime runloopThreshold:g_RunLoopTimeOut);
        dumpFileName = [WCDumpInterface dumpReportWithReportType:EDumpType_LaunchBlock suspendAllThreads:m_suspendAllThreads enableSnapshot:m_enableSnapshot];
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

- (void)clearLaunchLagRecord {
    NSString *filePath = [WCCrashBlockFileHandler getLaunchBlockRecordFilePath];
    if ([[NSFileManager defaultManager] fileExistsAtPath:filePath]) {
        [[NSFileManager defaultManager] removeItemAtPath:filePath error:nil];
        MatrixInfo(@"launch over, clear dump file record");
    }
}

- (void)clearDumpInBackgroundLaunch {
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

- (void)startTrackCPU {
    m_bTrackCPU = YES;
}

- (void)stopTrackCPU {
    m_bTrackCPU = NO;
}

- (BOOL)isBackgroundCPUTooSmall {
    return [m_cpuHandler isBackgroundCPUTooSmall];
}

// ============================================================================
#pragma mark - Hangs Detection
// ============================================================================

- (BOOL)lowerRunloopThreshold {
    useconds_t lowThreshold = [_monitorConfigHandler getRunloopLowThreshold];
    useconds_t defaultThreshold = [_monitorConfigHandler getRunloopTimeOut];

    if (lowThreshold > defaultThreshold) {
        MatrixWarning(@"Failed to lower runloop threshold: lowThreshold [%u] > defaultThreshold [%u].", lowThreshold, defaultThreshold);
        return NO;
    }

    return [self setRunloopThreshold:lowThreshold];
}

- (BOOL)recoverRunloopThreshold {
    useconds_t defaultThreshold = [_monitorConfigHandler getRunloopTimeOut];
    return [self setRunloopThreshold:defaultThreshold];
}

- (BOOL)setRunloopThreshold:(useconds_t)threshold {
    return [self setRunloopThreshold:threshold isFirstTime:NO];
}

- (BOOL)setRunloopThreshold:(useconds_t)threshold isFirstTime:(BOOL)isFirstTime {
    if (!isFirstTime && ![_monitorConfigHandler getRunloopDynamicThresholdEnabled]) {
        MatrixInfo(@"Failed to set runloop threshold: dynamic threshold isn't supported on this device.");
        return NO;
    }

    if (threshold < (400 * BM_MicroFormat_MillSecond) || threshold > (2 * BM_MicroFormat_Second)) {
        MatrixWarning(@"Failed to set runloop threshold: %u isn't in the range of [400ms, 2s].", threshold);
        return NO;
    }

    if (threshold % (100 * BM_MicroFormat_MillSecond) != 0) {
        MatrixWarning(@"Failed to set runloop threshold: %u isn't a multiple of 100ms.", threshold);
        return NO;
    }

    if (threshold == g_RunLoopTimeOut) {
        MatrixInfo(@"Set runloop threshold: same as current value.");
        return YES;
    }

    useconds_t checkPeriodTime = threshold / 2;
    assert(checkPeriodTime % g_PerStackInterval == 0); // 50ms 的整数倍

    useconds_t previousRunLoopTimeOut = g_RunLoopTimeOut;
    useconds_t previousCheckPeriodTime = g_CheckPeriodTime;
    g_RunLoopTimeOut = threshold;
    g_CheckPeriodTime = checkPeriodTime;

    if (!isFirstTime) {
        // 下一次跑到 resetStatus 时，更新 g_MainThreadCount 和 m_pointMainThreadHandler
        g_runloopThresholdUpdated = YES;
    }

    MatrixInfo(@"Set runloop threshold: before[%u] after[%u], check period: before[%u] after[%u]", previousRunLoopTimeOut, g_RunLoopTimeOut, previousCheckPeriodTime, g_CheckPeriodTime);

    return YES;
}

+ (void)checkRunloopDuration {
    assert(g_bSensitiveRunloopHangDetection);
    assert(g_bRun);

    // be careful: these code run frequently in main thread
    struct timeval tvCur;
    gettimeofday(&tvCur, NULL);
    unsigned long long duration = [WCBlockMonitorMgr diffTime:&g_tvRun endTime:&tvCur];

    // Apple HangTracer's threshold: 250ms
    if ((duration > 250 * BM_MicroFormat_MillSecond) && (duration < 60 * BM_MicroFormat_Second)) {
        // leave main thread as soon as possible
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            if ([MatrixDeviceInfo isBeingDebugged]) {
                MatrixInfo(@"Runloop hang detected: %llu ms (debugger attached, not reporting)", duration / 1000);
            } else {
                MatrixInfo(@"Runloop hang detected: %llu ms", duration / 1000);
                WCBlockMonitorMgr *blockMonitorMgr = [WCBlockMonitorMgr shareInstance];
                BM_SAFE_CALL_SELECTOR_NO_RETURN(blockMonitorMgr.delegate,
                                                @selector(onBlockMonitor:runloopHangDetected:),
                                                onBlockMonitor:blockMonitorMgr runloopHangDetected:duration);
            }
        });
    }
}

- (void)setShouldSuspendAllThreads:(BOOL)dynamicConfig {
    BOOL staticConfig = [_monitorConfigHandler getShouldSuspendAllThreads];
    m_suspendAllThreads = staticConfig && dynamicConfig;
    MatrixInfo(@"setShouldSuspendAllThreads: dynamicConfig = %d, staticConfig = %d", dynamicConfig, staticConfig);
}

// ============================================================================
#pragma mark - Custom Dump
// ============================================================================

- (void)generateLiveReportWithDumpType:(EDumpType)dumpType withReason:(NSString *)reason selfDefinedPath:(BOOL)bSelfDefined {
    [WCDumpInterface dumpReportWithReportType:dumpType
                          withExceptionReason:reason
                            suspendAllThreads:m_suspendAllThreads
                               enableSnapshot:m_enableSnapshot
                                writeCpuUsage:NO
                              selfDefinedPath:bSelfDefined];
}

// ============================================================================
#pragma mark - WCPowerConsumeStackCollectorDelegate
// ============================================================================

- (void)powerConsumeStackCollectorConclude:(NSArray<NSDictionary *> *)stackTree {
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

+ (unsigned long long)diffTime:(struct timeval *)tvStart endTime:(struct timeval *)tvEnd {
    return 1000000 * (tvEnd->tv_sec - tvStart->tv_sec) + tvEnd->tv_usec - tvStart->tv_usec;
}

@end
