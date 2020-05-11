//
//  KSCrash.m
//
//  Created by Karl Stenerud on 2012-01-28.
//
//  Copyright (c) 2012 Karl Stenerud. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall remain in place
// in this source code.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//


#import "KSCrash.h"

#import "KSCrashC.h"
#import "KSCrashDoctor.h"
#import "KSCrashReportFields.h"
#import "KSCrashMonitor_AppState.h"
#import "KSJSONCodecObjC.h"
#import "NSError+SimpleConstructor.h"
#import "KSCrashMonitorContext.h"
#import "KSCrashMonitor_System.h"
#import "KSSystemCapabilities.h"
//#define KSLogger_LocalLevel TRACE
#import "KSLogger.h"
#import "MatrixPathUtil.h"

#include <inttypes.h>
#if KSCRASH_HAS_UIKIT
#import <UIKit/UIKit.h>
#endif

// ============================================================================
#pragma mark - Meta Data -
// ============================================================================

/**
 * Metadata class to hold name and creation date for a file, with
 * default comparison based on the creation date (ascending).
 */
@interface KSCrashReportInfo: NSObject

@property(nonatomic,readonly,retain) NSString* reportID;
@property(nonatomic,readonly,retain) NSDate* creationDate;

+ (KSCrashReportInfo*) reportInfoWithID:(NSString*) reportID
                           creationDate:(NSDate*) creationDate;

- (id) initWithID:(NSString*) reportID creationDate:(NSDate*) creationDate;

- (NSComparisonResult) compare:(KSCrashReportInfo*) other;

@end

@implementation KSCrashReportInfo

@synthesize reportID = _reportID;
@synthesize creationDate = _creationDate;

+ (KSCrashReportInfo*) reportInfoWithID:(NSString*) reportID
                           creationDate:(NSDate*) creationDate
{
    return [[self alloc] initWithID:reportID creationDate:creationDate];
}

- (id) initWithID:(NSString*) reportID creationDate:(NSDate*) creationDate
{
    if((self = [super init]))
    {
        _reportID = reportID;
        _creationDate = creationDate;
    }
    return self;
}

- (NSComparisonResult) compare:(KSCrashReportInfo*) other
{
    return [_creationDate compare:other->_creationDate];
}

@end

// ============================================================================
#pragma mark - Globals -
// ============================================================================

@interface KSCrash ()

@property(nonatomic,readwrite,retain) NSString* bundleName;
@property(nonatomic,readwrite,retain) NSString* basePath;

@end


static NSString* getBundleName()
{
    NSString* bundleName = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleName"];
    if(bundleName == nil)
    {
        bundleName = @"Unknown";
    }
    return bundleName;
}

static NSString* getBasePath()
{
    NSString *ksBasePath = [MatrixPathUtil crashBlockPluginCachePath];
    NSString *crashStorePath = [ksBasePath stringByAppendingPathComponent:getBundleName()];
    return crashStorePath;
}

@implementation KSCrash

// ============================================================================
#pragma mark - Properties -
// ============================================================================

@synthesize sink = _sink;
@synthesize userInfo = _userInfo;
@synthesize deleteBehaviorAfterSendAll = _deleteBehaviorAfterSendAll;
@synthesize monitoring = _monitoring;
@synthesize deadlockWatchdogInterval = _deadlockWatchdogInterval;
@synthesize searchQueueNames = _searchQueueNames;
@synthesize onCrash = _onCrash;
@synthesize onHandleSignalCallBack = _onHandleSignalCallBack;
@synthesize onWritePointThread = _onWritePointThread;
@synthesize onWritePointThreadRepeatNumber = _onWritePointThreadRepeatNumber;
@synthesize onWritePointCpuHighThread = _onWritePointCpuHighThread;
@synthesize onWritePointCpuHighThreadCount = _onWritePointCpuHighThreadCount;
@synthesize onWritePointCpuHighThreadValue =_onWritePointCpuHighThreadValue;
@synthesize bundleName = _bundleName;
@synthesize basePath = _basePath;
@synthesize introspectMemory = _introspectMemory;
@synthesize doNotIntrospectClasses = _doNotIntrospectClasses;
@synthesize demangleLanguages = _demangleLanguages;
@synthesize addConsoleLogToReport = _addConsoleLogToReport;
@synthesize printPreviousLog = _printPreviousLog;
@synthesize maxReportCount = _maxReportCount;
@synthesize uncaughtExceptionHandler = _uncaughtExceptionHandler;


// ============================================================================
#pragma mark - Lifecycle -
// ============================================================================

+ (void)load
{
    [[self class] classDidBecomeLoaded];
}

+ (void)initialize
{
    if (self == [KSCrash class]) {
        [[self class] subscribeToNotifications];
    }
}

+ (instancetype) sharedInstance
{
    static KSCrash *sharedInstance = nil;
    static dispatch_once_t onceToken;
    
    dispatch_once(&onceToken, ^{
        sharedInstance = [[KSCrash alloc] init];
    });
    return sharedInstance;
}

- (id) init
{
    return [self initWithBasePath:getBasePath()];
}

- (id) initWithBasePath:(NSString *)basePath
{
    if((self = [super init]))
    {
        self.bundleName = getBundleName();
        self.basePath = basePath;
        if(self.basePath == nil)
        {
            KSLOG_ERROR(@"Failed to initialize crash handler. Crash reporting disabled.");
            return nil;
        }
        self.deleteBehaviorAfterSendAll = KSCDeleteAlways;
        self.introspectMemory = YES;
        self.catchZombies = NO;
        self.maxReportCount = 5;
        self.searchQueueNames = NO;
        self.monitoring = KSCrashMonitorTypeProductionSafeMinimal;
    }
    return self;
}


// ============================================================================
#pragma mark - API -
// ============================================================================

- (NSDictionary*) userInfo
{
   return _userInfo;
}

- (void) setUserInfo:(NSDictionary*) userInfo
{
    @synchronized (self)
    {
        NSError* error = nil;
        NSData* userInfoJSON = nil;
        if(userInfo != nil)
        {
            userInfoJSON = [self nullTerminated:[KSJSONCodec encode:userInfo
                                                            options:KSJSONEncodeOptionSorted
                                                              error:&error]];
            if(error != NULL)
            {
                KSLOG_ERROR(@"Could not serialize user info: %@", error);
                return;
            }
        }
        
        _userInfo = userInfo;
        kscrash_setUserInfoJSON([userInfoJSON bytes]);
    }
}

- (void) setMonitoring:(KSCrashMonitorType)monitoring
{
    _monitoring = kscrash_setMonitoring(monitoring);
}

- (void) setDeadlockWatchdogInterval:(double) deadlockWatchdogInterval
{
    _deadlockWatchdogInterval = deadlockWatchdogInterval;
    kscrash_setDeadlockWatchdogInterval(deadlockWatchdogInterval);
}

- (void) setSearchQueueNames:(BOOL) searchQueueNames
{
    _searchQueueNames = searchQueueNames;
    kscrash_setSearchQueueNames(searchQueueNames);
}

- (void) setOnCrash:(KSReportWriteCallback) onCrash
{
    _onCrash = onCrash;
    kscrash_setCrashNotifyCallback(onCrash);
}

- (void) setOnHandleSignalCallBack:(KSCrashSentryHandleSignal) onHandleSignalCallBack
{
    _onHandleSignalCallBack = onHandleSignalCallBack;
    kscrash_setHandleSignalCallback(onHandleSignalCallBack);
}

- (void) setOnInnerHandleSignalCallBack:(KSCrashSentryHandleSignal)onInnerHandleSignalCallBack
{
    _onInnerHandleSignalCallBack = onInnerHandleSignalCallBack;
    kscrash_setInnerHandleSignalCallback(onInnerHandleSignalCallBack);
}

- (void) setOnWritePointThread:(KSReportWritePointThreadCallback)onWritePointThread
{
    _onWritePointThread = onWritePointThread;
    kscrash_setPointThreadCallback(onWritePointThread);
}

- (void) setOnWritePointThreadRepeatNumber:(KSReportWritePointThreadRepeatNumberCallback)onWritePointThreadRepeatNumber
{
    _onWritePointThreadRepeatNumber = onWritePointThreadRepeatNumber;
    kscrash_setPointThreadRepeatNumberCallback(onWritePointThreadRepeatNumber);
}

- (void) setOnWritePointCpuHighThread:(KSReportWritePointCpuHighThreadCallback)onWritePointCpuHighThread
{
    _onWritePointCpuHighThread = onWritePointCpuHighThread;
    kscrash_setPointCpuHighThreadCallback(onWritePointCpuHighThread);
    
}

- (void) setOnWritePointCpuHighThreadCount:(KSReportWritePointCpuHighThreadCountCallback)onWritePointCpuHighThreadCount
{
    _onWritePointCpuHighThreadCount = onWritePointCpuHighThreadCount;
    kscrash_setPointCpuHighThreadCountCallback(onWritePointCpuHighThreadCount);
}

- (void) setOnWritePointCpuHighThreadValue:(KSReportWritePointCpuHighThreadValueCallback)onWritePointCpuHighThreadValue
{
    _onWritePointCpuHighThreadValue = onWritePointCpuHighThreadValue;
    kscrash_setPointCpuHighThreadValueCallback(onWritePointCpuHighThreadValue);
}

- (void) setIntrospectMemory:(BOOL) introspectMemory
{
    _introspectMemory = introspectMemory;
    kscrash_setIntrospectMemory(introspectMemory);
}

- (BOOL) catchZombies
{
    return (self.monitoring & KSCrashMonitorTypeZombie) != 0;
}

- (void) setCatchZombies:(BOOL)catchZombies
{
    if(catchZombies)
    {
        self.monitoring |= KSCrashMonitorTypeZombie;
    }
    else
    {
        self.monitoring &= (KSCrashMonitorType)~KSCrashMonitorTypeZombie;
    }
}

- (void) setDoNotIntrospectClasses:(NSArray *)doNotIntrospectClasses
{
    _doNotIntrospectClasses = doNotIntrospectClasses;
    NSUInteger count = [doNotIntrospectClasses count];
    if(count == 0)
    {
        kscrash_setDoNotIntrospectClasses(nil, 0);
    }
    else
    {
        NSMutableData* data = [NSMutableData dataWithLength:count * sizeof(const char*)];
        const char** classes = data.mutableBytes;
        for(unsigned i = 0; i < count; i++)
        {
            classes[i] = [[doNotIntrospectClasses objectAtIndex:i] cStringUsingEncoding:NSUTF8StringEncoding];
        }
        kscrash_setDoNotIntrospectClasses(classes, (int)count);
    }
}

- (void) setMaxReportCount:(int)maxReportCount
{
    _maxReportCount = maxReportCount;
    kscrash_setMaxReportCount(maxReportCount);
}

- (NSDictionary*) systemInfo
{
    KSCrash_MonitorContext fakeEvent = {0};
    kscm_system_getAPI()->addContextualInfoToEvent(&fakeEvent);
    NSMutableDictionary* dict = [NSMutableDictionary new];

#define COPY_STRING(A) if (fakeEvent.System.A) dict[@#A] = [NSString stringWithUTF8String:fakeEvent.System.A]
#define COPY_STRING_TO_KEY(A, key) if (fakeEvent.System.A) dict[@key] = [NSString stringWithUTF8String:fakeEvent.System.A]

#define COPY_PRIMITIVE(A) dict[@#A] = @(fakeEvent.System.A)
#define COPY_PRIMITIVE_TO_KEY(A, key) dict[@key] = @(fakeEvent.System.A)

    COPY_STRING_TO_KEY(systemName, KSCrashField_SystemName);
    COPY_STRING_TO_KEY(systemVersion, KSCrashField_SystemVersion);
    COPY_STRING_TO_KEY(machine, KSCrashField_Machine);
    COPY_STRING_TO_KEY(model, KSCrashField_Model);
    COPY_STRING_TO_KEY(kernelVersion, KSCrashField_KernelVersion);
    COPY_STRING_TO_KEY(osVersion, KSCrashField_OSVersion);
    COPY_PRIMITIVE_TO_KEY(isJailbroken, KSCrashField_Jailbroken);
    COPY_STRING_TO_KEY(bootTime, KSCrashField_BootTime);
    COPY_STRING_TO_KEY(appStartTime, KSCrashField_AppStartTime);
    COPY_STRING_TO_KEY(executablePath, KSCrashField_ExecutablePath);
    COPY_STRING_TO_KEY(executableName, KSCrashField_Executable);
    COPY_STRING_TO_KEY(bundleID, KSCrashField_BundleID);
    COPY_STRING_TO_KEY(bundleName, KSCrashField_BundleName);
    
    if (kscrash_getCustomShortVersion() != NULL &&
        kscrash_getCustomFullVersion() != NULL) {
        dict[@KSCrashField_BundleVersion] = [NSString stringWithUTF8String:kscrash_getCustomFullVersion()];
        dict[@KSCrashField_BundleShortVersion] = [NSString stringWithUTF8String:kscrash_getCustomShortVersion()];
    } else {
        COPY_STRING_TO_KEY(bundleVersion, KSCrashField_BundleVersion);
        COPY_STRING_TO_KEY(bundleShortVersion, KSCrashField_BundleShortVersion);
    }

    COPY_STRING_TO_KEY(appID, KSCrashField_AppUUID);
    COPY_STRING_TO_KEY(cpuArchitecture, KSCrashField_CPUArch);
    COPY_PRIMITIVE_TO_KEY(cpuType, KSCrashField_CPUType);
    COPY_PRIMITIVE_TO_KEY(cpuSubType, KSCrashField_CPUSubType);
    COPY_PRIMITIVE_TO_KEY(binaryCPUType, KSCrashField_BinaryCPUType);
    COPY_PRIMITIVE_TO_KEY(binaryCPUSubType, KSCrashField_BinaryCPUSubType);
    COPY_STRING_TO_KEY(timezone, KSCrashField_TimeZone);
    COPY_STRING_TO_KEY(processName, KSCrashField_ProcessName);
    COPY_PRIMITIVE_TO_KEY(processID, KSCrashField_ProcessID);
    COPY_PRIMITIVE_TO_KEY(parentProcessID, KSCrashField_ParentProcessID);
    COPY_STRING_TO_KEY(deviceAppHash, KSCrashField_DeviceAppHash);
    COPY_STRING_TO_KEY(buildType, KSCrashField_BuildType);
    COPY_PRIMITIVE_TO_KEY(storageSize, KSCrashField_Storage);

    return dict;
}

+ (void)setCustomFullVersion:(NSString *)fullVersion shortVersion:(NSString *)shortVersion
{
    kscrash_setCustomVersion([fullVersion UTF8String], [shortVersion UTF8String]);
}

- (BOOL) install
{
    _monitoring = kscrash_install(self.bundleName.UTF8String,
                                  self.basePath.UTF8String);
    if(self.monitoring == 0)
    {
        return false;
    }

    return true;
}

- (void) sendAllReportsWithCompletion:(KSCrashReportFilterCompletion) onCompletion
{
    NSArray* reports = [self allReports];
    
    KSLOG_INFO(@"Sending %d crash reports", [reports count]);
    
    [self sendReports:reports
         onCompletion:^(NSArray* filteredReports, BOOL completed, NSError* error)
     {
         KSLOG_DEBUG(@"Process finished with completion: %d", completed);
         if(error != nil)
         {
             KSLOG_ERROR(@"Failed to send reports: %@", error);
         }
         if((self.deleteBehaviorAfterSendAll == KSCDeleteOnSucess && completed) ||
            self.deleteBehaviorAfterSendAll == KSCDeleteAlways)
         {
             kscrash_deleteAllReports();
         }
         kscrash_callCompletion(onCompletion, filteredReports, completed, error);
     }];
}

- (void) deleteAllReports
{
    kscrash_deleteAllReports();
}

- (void) reportUserException:(NSString*) name
                      reason:(NSString*) reason
                    language:(NSString*) language
                  lineOfCode:(NSString*) lineOfCode
                  stackTrace:(NSArray*) stackTrace
               logAllThreads:(BOOL) logAllThreads
            terminateProgram:(BOOL) terminateProgram
{
    const char* cName = [name cStringUsingEncoding:NSUTF8StringEncoding];
    const char* cReason = [reason cStringUsingEncoding:NSUTF8StringEncoding];
    const char* cLanguage = [language cStringUsingEncoding:NSUTF8StringEncoding];
    const char* cLineOfCode = [lineOfCode cStringUsingEncoding:NSUTF8StringEncoding];
    
    const char* cStackTrace = NULL;
    if (stackTrace != NULL) {
        NSError* error = nil;
        NSData* jsonData = [KSJSONCodec encode:stackTrace options:0 error:&error];
        if(jsonData == nil || error != nil)
        {
            KSLOG_ERROR(@"Error encoding stack trace to JSON: %@", error);
            // Don't return, since we can still record other useful information.
        }
        NSString* jsonString = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
        cStackTrace = [jsonString cStringUsingEncoding:NSUTF8StringEncoding];
    }
    
    kscrash_reportUserException(cName,
                                cReason,
                                cLanguage,
                                cLineOfCode,
                                cStackTrace,
                                logAllThreads,
                                terminateProgram);
}

- (void) reportUserException:(NSString*) name
                      reason:(NSString*) reason
                    language:(NSString*) language
                  lineOfCode:(NSString*) lineOfCode
                  stackTrace:(NSArray*) stackTrace
               logAllThreads:(BOOL) logAllThreads
            terminateProgram:(BOOL) terminateProgram
                dumpFilePath:(NSString*) dumpFilePath
                    dumpType:(int) dumpType
{
    const char* cName = [name cStringUsingEncoding:NSUTF8StringEncoding];
    const char* cReason = [reason cStringUsingEncoding:NSUTF8StringEncoding];
    const char* cLanguage = [language cStringUsingEncoding:NSUTF8StringEncoding];
    const char* cLineOfCode = [lineOfCode cStringUsingEncoding:NSUTF8StringEncoding];
    const char* cDumpFilePath = [dumpFilePath cStringUsingEncoding:NSUTF8StringEncoding];
    
    const char* cStackTrace = NULL;
    if (stackTrace != NULL) {
        NSError* error = nil;
        NSData* jsonData = [KSJSONCodec encode:stackTrace options:0 error:&error];
        if(jsonData == nil || error != nil)
        {
            KSLOG_ERROR(@"Error encoding stack trace to JSON: %@", error);
            // Don't return, since we can still record other useful information.
        }
        NSString* jsonString = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
        cStackTrace = [jsonString cStringUsingEncoding:NSUTF8StringEncoding];
    }
    
    kscrash_reportUserExceptionWithSelfDefinedPath(cName,
                                                   cReason,
                                                   cLanguage,
                                                   cLineOfCode,
                                                   cStackTrace,
                                                   logAllThreads,
                                                   terminateProgram,
                                                   cDumpFilePath,
                                                   dumpType);
}

// ============================================================================
#pragma mark - Advanced API -
// ============================================================================

#define SYNTHESIZE_CRASH_STATE_PROPERTY(TYPE, NAME) \
- (TYPE) NAME \
{ \
    return kscrashstate_currentState()->NAME; \
}

SYNTHESIZE_CRASH_STATE_PROPERTY(NSTimeInterval, activeDurationSinceLastCrash)
SYNTHESIZE_CRASH_STATE_PROPERTY(NSTimeInterval, backgroundDurationSinceLastCrash)
SYNTHESIZE_CRASH_STATE_PROPERTY(int, launchesSinceLastCrash)
SYNTHESIZE_CRASH_STATE_PROPERTY(int, sessionsSinceLastCrash)
SYNTHESIZE_CRASH_STATE_PROPERTY(NSTimeInterval, activeDurationSinceLaunch)
SYNTHESIZE_CRASH_STATE_PROPERTY(NSTimeInterval, backgroundDurationSinceLaunch)
SYNTHESIZE_CRASH_STATE_PROPERTY(int, sessionsSinceLaunch)
SYNTHESIZE_CRASH_STATE_PROPERTY(BOOL, crashedLastLaunch)

- (int) reportCount
{
    return kscrash_getReportCount();
}

- (void) sendReports:(NSArray*) reports onCompletion:(KSCrashReportFilterCompletion) onCompletion
{
    if([reports count] == 0)
    {
        kscrash_callCompletion(onCompletion, reports, YES, nil);
        return;
    }
    
    if(self.sink == nil)
    {
        kscrash_callCompletion(onCompletion, reports, NO,
                                 [KSError errorWithDomain:[[self class] description]
                                                     code:0
                                              description:@"No sink set. Crash reports not sent."]);
        return;
    }
    
    [self.sink filterReports:reports
                onCompletion:^(NSArray* filteredReports, BOOL completed, NSError* error)
     {
         kscrash_callCompletion(onCompletion, filteredReports, completed, error);
     }];
}

- (NSData*) loadCrashReportJSONWithID:(int64_t) reportID
{
    char* report = kscrash_readReport(reportID);
    if(report != NULL)
    {
        return [NSData dataWithBytesNoCopy:report length:strlen(report) freeWhenDone:YES];
    }
    return nil;
}

- (void) doctorReport:(NSMutableDictionary*) report
{
    NSMutableDictionary* crashReport = report[@KSCrashField_Crash];
    if(crashReport != nil)
    {
        crashReport[@KSCrashField_Diagnosis] = [[KSCrashDoctor doctor] diagnoseCrash:report];
    }
    crashReport = report[@KSCrashField_RecrashReport][@KSCrashField_Crash];
    if(crashReport != nil)
    {
        crashReport[@KSCrashField_Diagnosis] = [[KSCrashDoctor doctor] diagnoseCrash:report];
    }
}

- (NSDictionary*) reportWithID:(int64_t) reportID
{
    NSData* jsonData = [self loadCrashReportJSONWithID:reportID];
    if(jsonData == nil)
    {
        return nil;
    }

    NSError* error = nil;
    NSMutableDictionary* crashReport = [KSJSONCodec decode:jsonData
                                                   options:KSJSONDecodeOptionIgnoreNullInArray |
                                                           KSJSONDecodeOptionIgnoreNullInObject |
                                                           KSJSONDecodeOptionKeepPartialObject
                                                     error:&error];
    if(error != nil)
    {
        KSLOG_ERROR(@"Encountered error loading crash report %" PRIx64 ": %@", reportID, error);
    }
    if(crashReport == nil)
    {
        KSLOG_ERROR(@"Could not load crash report");
        return nil;
    }
    [self doctorReport:crashReport];

    return crashReport;
}

- (NSArray*) allReports
{
    int reportCount = kscrash_getReportCount();
    int64_t reportIDs[reportCount];
    reportCount = kscrash_getReportIDs(reportIDs, reportCount);
    NSMutableArray* reports = [NSMutableArray arrayWithCapacity:(NSUInteger)reportCount];
    for(int i = 0; i < reportCount; i++)
    {
        NSDictionary* report = [self reportWithID:reportIDs[i]];
        if(report != nil)
        {
            [reports addObject:report];
        }
    }
    
    return reports;
}

- (NSString*) crashReportFileNameWithID:(NSString*) reportID
{
    return [NSString stringWithFormat:@"%@" @"-report-" "%@.json", self.bundleName, reportID];
}

- (NSString*) pathToCrashReportWithID:(NSString *) reportID
{
    NSString* fileName = [self crashReportFileNameWithID:reportID];
    return [[self.basePath stringByAppendingPathComponent:@"Reports"] stringByAppendingPathComponent:fileName];
}

- (NSString*) pathToCrashReportWithID:(NSString *)reportID withStorePath:(NSString *)storePath
{
    NSString* fileName = [self crashReportFileNameWithID:reportID];
    return [storePath stringByAppendingPathComponent:fileName];
}

- (NSArray*) allReportID
{
    return [self allReportIDWithPath:[self.basePath stringByAppendingPathComponent:@"Reports"]];
}

- (NSArray*) allReportIDWithPath:(NSString *)storePath
{
    NSError* error = nil;
    NSFileManager* fm = [NSFileManager defaultManager];
    NSArray* filenames = [fm contentsOfDirectoryAtPath:storePath error:&error];
    if(filenames == nil)
    {
        KSLOG_ERROR(@"Could not get contents of directory %@: %@", storePath, error);
        return nil;
    }
    
    NSMutableArray* reports = [NSMutableArray arrayWithCapacity:[filenames count]];
    for(NSString* filename in filenames)
    {
        NSString* reportId = [self reportIDFromFilename:filename];
        if(reportId != nil)
        {
            NSString* fullPath = [storePath stringByAppendingPathComponent:filename];
            NSDictionary* fileAttribs = [fm attributesOfItemAtPath:fullPath error:&error];
            if(fileAttribs == nil)
            {
                KSLOG_ERROR(@"Could not read file attributes for %@: %@", fullPath, error);
            }
            else
            {
                [reports addObject:[KSCrashReportInfo reportInfoWithID:reportId
                                                          creationDate:[fileAttribs valueForKey:NSFileCreationDate]]];
            }
        }
    }
    [reports sortUsingSelector:@selector(compare:)];
    
    NSMutableArray* sortedIDs = [NSMutableArray arrayWithCapacity:[reports count]];
    for(KSCrashReportInfo* info in reports)
    {
        [sortedIDs addObject:info.reportID];
    }
    return sortedIDs;
}

- (NSString*) reportIDFromFilename:(NSString*) filename
{
    if([filename length] == 0 || [[filename pathExtension] isEqualToString:@"json"] == NO)
    {
        return nil;
    }
    
    NSString* prefix = [NSString stringWithFormat:@"%@%@", self.bundleName, @"-report-"];
    NSString* suffix = @".json";
    
    NSRange prefixRange = [filename rangeOfString:prefix];
    NSRange suffixRange = [filename rangeOfString:suffix options:NSBackwardsSearch];
    if(prefixRange.location == 0 && suffixRange.location != NSNotFound)
    {
        NSUInteger prefixEnd = NSMaxRange(prefixRange);
        NSRange range = NSMakeRange(prefixEnd, suffixRange.location - prefixEnd);
        return [filename substringWithRange:range];
    }

    return nil;
}

- (NSData*) loadCrashReportJSONWithStringID:(NSString *) reportID
{
    char* report = kscrash_readReportStringID([reportID UTF8String]);
    if(report != NULL)
    {
        return [NSData dataWithBytesNoCopy:report length:strlen(report) freeWhenDone:YES];
    }
    return nil;
}

- (NSDictionary*) reportWithStringID:(NSString *) reportID
{
    NSData* jsonData = [self loadCrashReportJSONWithStringID:reportID];
    if(jsonData == nil)
    {
        return nil;
    }
    
    NSError* error = nil;
    NSMutableDictionary* crashReport = [KSJSONCodec decode:jsonData
                                                   options:KSJSONDecodeOptionIgnoreNullInArray |
                                        KSJSONDecodeOptionIgnoreNullInObject |
                                        KSJSONDecodeOptionKeepPartialObject
                                                     error:&error];
    if(error != nil)
    {
        KSLOG_ERROR(@"Encountered error loading crash report %" PRIx64 ": %@", reportID, error);
    }
    if(crashReport == nil)
    {
        KSLOG_ERROR(@"Could not load crash report");
        return nil;
    }
    [self doctorReport:crashReport];
    
    return crashReport;
}

- (void) deleteReportWithID:(NSString*) reportID
{
    [self deleteReportWithID:reportID withStorePath:[self.basePath stringByAppendingPathComponent:@"Reports"]];
}

- (void) deleteReportWithID:(NSString *)reportID withStorePath:(NSString *)storePath
{
    NSError* error = nil;
    NSString* filename = [self pathToCrashReportWithID:reportID withStorePath:storePath];
    
    [[NSFileManager defaultManager] removeItemAtPath:filename error:&error];
    if(error != nil)
    {
        KSLOG_ERROR(@"Could not delete file %@: %@", filename, error);
    }
}

- (void) setAddConsoleLogToReport:(BOOL) shouldAddConsoleLogToReport
{
    _addConsoleLogToReport = shouldAddConsoleLogToReport;
    kscrash_setAddConsoleLogToReport(shouldAddConsoleLogToReport);
}

- (void) setPrintPreviousLog:(BOOL) shouldPrintPreviousLog
{
    _printPreviousLog = shouldPrintPreviousLog;
    kscrash_setPrintPreviousLog(shouldPrintPreviousLog);
}


// ============================================================================
#pragma mark - Utility -
// ============================================================================

- (NSMutableData*) nullTerminated:(NSData*) data
{
    if(data == nil)
    {
        return NULL;
    }
    NSMutableData* mutable = [NSMutableData dataWithData:data];
    [mutable appendBytes:"\0" length:1];
    return mutable;
}


// ============================================================================
#pragma mark - Notifications -
// ============================================================================

+ (void) subscribeToNotifications
{
#if KSCRASH_HAS_UIAPPLICATION
    NSNotificationCenter* nCenter = [NSNotificationCenter defaultCenter];
    [nCenter addObserver:self
                selector:@selector(applicationDidBecomeActive)
                    name:UIApplicationDidBecomeActiveNotification
                  object:nil];
    [nCenter addObserver:self
                selector:@selector(applicationWillResignActive)
                    name:UIApplicationWillResignActiveNotification
                  object:nil];
    [nCenter addObserver:self
                selector:@selector(applicationDidEnterBackground)
                    name:UIApplicationDidEnterBackgroundNotification
                  object:nil];
    [nCenter addObserver:self
                selector:@selector(applicationWillEnterForeground)
                    name:UIApplicationWillEnterForegroundNotification
                  object:nil];
    [nCenter addObserver:self
                selector:@selector(applicationWillTerminate)
                    name:UIApplicationWillTerminateNotification
                  object:nil];
#endif
#if KSCRASH_HAS_NSEXTENSION
    NSNotificationCenter* nCenter = [NSNotificationCenter defaultCenter];
    [nCenter addObserver:self
                selector:@selector(applicationDidBecomeActive)
                    name:NSExtensionHostDidBecomeActiveNotification
                  object:nil];
    [nCenter addObserver:self
                selector:@selector(applicationWillResignActive)
                    name:NSExtensionHostWillResignActiveNotification
                  object:nil];
    [nCenter addObserver:self
                selector:@selector(applicationDidEnterBackground)
                    name:NSExtensionHostDidEnterBackgroundNotification
                  object:nil];
    [nCenter addObserver:self
                selector:@selector(applicationWillEnterForeground)
                    name:NSExtensionHostWillEnterForegroundNotification
                  object:nil];
#endif
}

+ (void) classDidBecomeLoaded
{
    kscrash_notifyObjCLoad();
}

+ (void) applicationDidBecomeActive
{
    kscrash_notifyAppActive(true);
}

+ (void) applicationWillResignActive
{
    kscrash_notifyAppActive(false);
}

+ (void) applicationDidEnterBackground
{
    kscrash_notifyAppInForeground(false);
}

+ (void) applicationWillEnterForeground
{
    kscrash_notifyAppInForeground(true);
}

+ (void) applicationWillTerminate
{
    kscrash_notifyAppTerminate();
}

@end


const double KSCrashFrameworkVersionNumber = 1.1519;

//! Project version string for KSCrashFramework.
const unsigned char KSCrashFrameworkVersionString[] = "1.15.19";
