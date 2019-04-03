//
//  KSCrashDoctor.m
//  KSCrash
//
//  Created by Karl Stenerud on 2012-11-10.
//  Copyright (c) 2012 Karl Stenerud. All rights reserved.
//

#import "KSCrashDoctor.h"
#import "KSCrashReportFields.h"
#import "KSCrashMonitor_System.h"


typedef enum
{
    CPUFamilyUnknown,
    CPUFamilyArm,
    CPUFamilyX86,
    CPUFamilyX86_64
} CPUFamily;

@interface  KSCrashDoctorParam: NSObject

@property(nonatomic, readwrite, retain) NSString* className;
@property(nonatomic, readwrite, retain) NSString* previousClassName;
@property(nonatomic, readwrite, retain) NSString* type;
@property(nonatomic, readwrite, assign) BOOL isInstance;
@property(nonatomic, readwrite, assign) uintptr_t address;
@property(nonatomic, readwrite, retain) NSString* value;

@end

@implementation KSCrashDoctorParam

@synthesize className = _className;
@synthesize previousClassName = _previousClassName;
@synthesize isInstance = _isInstance;
@synthesize address = _address;
@synthesize value = _value;
@synthesize type = _type;

@end

@interface KSCrashDoctorFunctionCall: NSObject

@property(nonatomic, readwrite, retain) NSString* name;
@property(nonatomic, readwrite, retain) NSArray* params;

@end

@implementation KSCrashDoctorFunctionCall

@synthesize name = _name;
@synthesize params = _params;

- (NSString*) descriptionForObjCCall
{
    if(![self.name isEqualToString:@"objc_msgSend"])
    {
        return nil;
    }
    KSCrashDoctorParam* receiverParam = [self.params objectAtIndex:0];
    NSString* receiver = receiverParam.previousClassName;
    if(receiver == nil)
    {
        receiver = receiverParam.className;
        if(receiver == nil)
        {
            receiver = @"id";
        }
    }

    KSCrashDoctorParam* selectorParam = [self.params objectAtIndex:1];
    if(![selectorParam.type isEqualToString:@KSCrashMemType_String])
    {
        return nil;
    }
    NSArray* splitSelector = [selectorParam.value componentsSeparatedByString:@":"];
    int paramCount = (int)splitSelector.count - 1;

    NSMutableString* string = [NSMutableString stringWithFormat:@"-[%@ %@", receiver, [splitSelector objectAtIndex:0]];
    for(int paramNum = 0; paramNum < paramCount; paramNum++)
    {
        [string appendString:@":"];
        if(paramNum < 2)
        {
            KSCrashDoctorParam* param = [self.params objectAtIndex:(NSUInteger)paramNum + 2];
            if(param.value != nil)
            {
                if([param.type isEqualToString:@KSCrashMemType_String])
                {
                    [string appendFormat:@"\"%@\"", param.value];
                }
                else
                {
                    [string appendString:param.value];
                }
            }
            else if(param.previousClassName != nil)
            {
                [string appendString:param.previousClassName];
            }
            else if(param.className != nil)
            {
                [string appendFormat:@"%@ (%@)", param.className, param.isInstance ? @"instance" : @"class"];
            }
            else
            {
                [string appendString:@"?"];
            }
        }
        else
        {
            [string appendString:@"?"];
        }
        if(paramNum < paramCount - 1)
        {
            [string appendString:@" "];
        }
    }

    [string appendString:@"]"];
    return string;
}

- (NSString*) descriptionWithParamCount:(int) paramCount
{
    NSString* objCCall = [self descriptionForObjCCall];
    if(objCCall != nil)
    {
        return objCCall;
    }

    if(paramCount > (int)self.params.count)
    {
        paramCount = (int)self.params.count;
    }
    NSMutableString* str = [NSMutableString string];
    [str appendFormat:@"Function: %@\n", self.name];
    for(int i = 0; i < paramCount; i++)
    {
        KSCrashDoctorParam* param = [self.params objectAtIndex:(NSUInteger)i];
        [str appendFormat:@"Param %d:  ", i + 1];
        if(param.className != nil)
        {
            [str appendFormat:@"%@ (%@) ", param.className, param.isInstance ? @"instance" : @"class"];
        }
        if(param.value != nil)
        {
            [str appendFormat:@"%@ ", param.value];
        }
        if(param.previousClassName != nil)
        {
            [str appendFormat:@"(was %@)", param.previousClassName];
        }
        if(i < paramCount - 1)
        {
            [str appendString:@"\n"];
        }
    }
    return str;
}

@end



@implementation KSCrashDoctor

+ (KSCrashDoctor*) doctor
{
    return [[self alloc] init];
}

- (NSDictionary*) recrashReport:(NSDictionary*) report
{
    return [report objectForKey:@KSCrashField_RecrashReport];
}

- (NSDictionary*) systemReport:(NSDictionary*) report
{
    return [report objectForKey:@KSCrashField_System];
}

- (NSDictionary*) crashReport:(NSDictionary*) report
{
    return [report objectForKey:@KSCrashField_Crash];
}

- (NSDictionary*) infoReport:(NSDictionary*) report
{
    return [report objectForKey:@KSCrashField_Report];
}

- (NSDictionary*) errorReport:(NSDictionary*) report
{
    return [[self crashReport:report] objectForKey:@KSCrashField_Error];
}

- (CPUFamily) cpuFamily:(NSDictionary*) report
{
    NSDictionary* system = [self systemReport:report];
    NSString* cpuArch = [system objectForKey:@KSCrashField_CPUArch];
    if([cpuArch rangeOfString:@"arm"].location == 0)
    {
        return CPUFamilyArm;
    }
    if([cpuArch rangeOfString:@"i"].location == 0 && [cpuArch rangeOfString:@"86"].location == 2)
    {
        return CPUFamilyX86;
    }
    if ([cpuArch rangeOfString:@"x86_64" options:NSCaseInsensitiveSearch].location != NSNotFound) {
        return CPUFamilyX86_64;
    }
    return CPUFamilyUnknown;
}

- (NSString*) registerNameForFamily:(CPUFamily) family paramIndex:(int) index
{
    switch (family)
    {
        case CPUFamilyArm:
        {
            switch(index)
            {
                case 0:
                    return @"r0";
                case 1:
                    return @"r1";
                case 2:
                    return @"r2";
                case 3:
                    return @"r3";
            }
        }
        case CPUFamilyX86:
        {
            switch(index)
            {
                case 0:
                    return @"edi";
                case 1:
                    return @"esi";
                case 2:
                    return @"edx";
                case 3:
                    return @"ecx";
            }
        }
        case CPUFamilyX86_64:
        {
            switch(index)
            {
                case 0:
                    return @"rdi";
                case 1:
                    return @"rsi";
                case 2:
                    return @"rdx";
                case 3:
                    return @"rcx";
            }
        }
        case CPUFamilyUnknown:
            return nil;
    }
    return nil;
}

- (NSString*) mainExecutableNameForReport:(NSDictionary*) report
{
    NSDictionary* info = [self infoReport:report];
    return [info objectForKey:@KSCrashField_ProcessName];
}

- (NSDictionary*) crashedThreadReport:(NSDictionary*) report
{
    NSDictionary* crashReport = [self crashReport:report];
    NSDictionary* crashedThread = [crashReport objectForKey:@KSCrashField_CrashedThread];
    if(crashedThread != nil)
    {
        return crashedThread;
    }

    for(NSDictionary* thread in [crashReport objectForKey:@KSCrashField_Threads])
    {
        if([[thread objectForKey:@KSCrashField_Crashed] boolValue])
        {
            return thread;
        }
    }
    return nil;
}

- (NSArray*) backtraceFromThreadReport:(NSDictionary*) threadReport
{
    NSDictionary* backtrace = [threadReport objectForKey:@KSCrashField_Backtrace];
    return [backtrace objectForKey:@KSCrashField_Contents];
}

- (NSDictionary*) basicRegistersFromThreadReport:(NSDictionary*) threadReport
{
    NSDictionary* registers = [threadReport objectForKey:@KSCrashField_Registers];
    NSDictionary* basic = [registers objectForKey:@KSCrashField_Basic];
    return basic;
}

- (NSDictionary*) lastInAppStackEntry:(NSDictionary*) report
{
    NSString* executableName = [self mainExecutableNameForReport:report];
    NSDictionary* crashedThread = [self crashedThreadReport:report];
    NSArray* backtrace = [self backtraceFromThreadReport:crashedThread];
    for(NSDictionary* entry in backtrace)
    {
        NSString* objectName = [entry objectForKey:@KSCrashField_ObjectName];
        if([objectName isEqualToString:executableName])
        {
            return entry;
        }
    }
    return nil;
}

- (NSDictionary*) lastStackEntry:(NSDictionary*) report
{
    NSDictionary* crashedThread = [self crashedThreadReport:report];
    NSArray* backtrace = [self backtraceFromThreadReport:crashedThread];
    if([backtrace count] > 0)
    {
        return [backtrace objectAtIndex:0];
    }
    return nil;
}

- (BOOL) isInvalidAddress:(NSDictionary*) errorReport
{
    NSDictionary* machError = [errorReport objectForKey:@KSCrashField_Mach];
    if(machError != nil)
    {
        NSString* exceptionName = [machError objectForKey:@KSCrashField_ExceptionName];
        return [exceptionName isEqualToString:@"EXC_BAD_ACCESS"];
    }
    NSDictionary* signal = [errorReport objectForKey:@KSCrashField_Signal];
    NSString* sigName = [signal objectForKey:@KSCrashField_Name];
    return [sigName isEqualToString:@"SIGSEGV"];
}

- (BOOL) isMathError:(NSDictionary*) errorReport
{
    NSDictionary* machError = [errorReport objectForKey:@KSCrashField_Mach];
    if(machError != nil)
    {
        NSString* exceptionName = [machError objectForKey:@KSCrashField_ExceptionName];
        return [exceptionName isEqualToString:@"EXC_ARITHMETIC"];
    }
    NSDictionary* signal = [errorReport objectForKey:@KSCrashField_Signal];
    NSString* sigName = [signal objectForKey:@KSCrashField_Name];
    return [sigName isEqualToString:@"SIGFPE"];
}

- (BOOL) isMemoryCorruption:(NSDictionary*) report
{
    NSDictionary* crashedThread = [self crashedThreadReport:report];
    NSArray* notableAddresses = [crashedThread objectForKey:@KSCrashField_NotableAddresses];
    for(NSDictionary* address in [notableAddresses objectEnumerator])
    {
        NSString* type = [address objectForKey:@KSCrashField_Type];
        if([type isEqualToString:@"string"])
        {
            NSString* value = [address objectForKey:@KSCrashField_Value];
            if([value rangeOfString:@"autorelease pool page"].location != NSNotFound &&
               [value rangeOfString:@"corrupted"].location != NSNotFound)
            {
                return YES;
            }
            if([value rangeOfString:@"incorrect checksum for freed object"].location != NSNotFound)
            {
                return YES;
            }
        }
    }

    NSArray* backtrace = [self backtraceFromThreadReport:crashedThread];
    for(NSDictionary* entry in backtrace)
    {
        NSString* objectName = [entry objectForKey:@KSCrashField_ObjectName];
        NSString* symbolName = [entry objectForKey:@KSCrashField_SymbolName];
        if([symbolName isEqualToString:@"objc_autoreleasePoolPush"])
        {
            return YES;
        }
        if([symbolName isEqualToString:@"free_list_checksum_botch"])
        {
            return YES;
        }
        if([symbolName isEqualToString:@"szone_malloc_should_clear"])
        {
            return YES;
        }
        if([symbolName isEqualToString:@"lookUpMethod"] && [objectName isEqualToString:@"libobjc.A.dylib"])
        {
            return YES;
        }
    }

    return NO;
}

- (KSCrashDoctorFunctionCall*) lastFunctionCall:(NSDictionary*) report
{
    KSCrashDoctorFunctionCall* function = [[KSCrashDoctorFunctionCall alloc] init];
    NSDictionary* lastStackEntry = [self lastStackEntry:report];
    function.name = [lastStackEntry objectForKey:@KSCrashField_SymbolName];

    NSDictionary* crashedThread = [self crashedThreadReport:report];
    NSDictionary* notableAddresses = [crashedThread objectForKey:@KSCrashField_NotableAddresses];
    CPUFamily family = [self cpuFamily:report];
    NSDictionary* registers = [self basicRegistersFromThreadReport:crashedThread];
    NSArray* regNames = [NSArray arrayWithObjects:
                           [self registerNameForFamily:family paramIndex:0],
                           [self registerNameForFamily:family paramIndex:1],
                           [self registerNameForFamily:family paramIndex:2],
                           [self registerNameForFamily:family paramIndex:3],
                           nil];
    NSMutableArray* params = [NSMutableArray arrayWithCapacity:4];
    for(NSString* regName in regNames)
    {
        KSCrashDoctorParam* param = [[KSCrashDoctorParam alloc] init];
        param.address = (uintptr_t)[[registers objectForKey:regName] unsignedLongLongValue];
        NSDictionary* notableAddress = [notableAddresses objectForKey:regName];
        if(notableAddress == nil)
        {
            param.value = [NSString stringWithFormat:@"%p", (void*)param.address];
        }
        else
        {
            param.type = [notableAddress objectForKey:@KSCrashField_Type];
            NSString* className = [notableAddress objectForKey:@KSCrashField_Class];
            NSString* previousClass = [notableAddress objectForKey:@KSCrashField_LastDeallocObject];
            NSString* value = [notableAddress objectForKey:@KSCrashField_Value];

            if([param.type isEqualToString:@KSCrashMemType_String])
            {
                param.value = value;
            }
            else if([param.type isEqualToString:@KSCrashMemType_Object])
            {
                param.className = className;
                param.isInstance = YES;
            }
            else if([param.type isEqualToString:@KSCrashMemType_Class])
            {
                param.className = className;
                param.isInstance = NO;
            }
            param.previousClassName = previousClass;
        }

        [params addObject:param];
    }

    function.params = params;
    return function;
}

- (NSString*) zombieCall:(KSCrashDoctorFunctionCall*) functionCall
{
    if([functionCall.name isEqualToString:@"objc_msgSend"] && functionCall.params.count > 0 && [[functionCall.params objectAtIndex:0] previousClassName] != nil)
    {
        return [functionCall descriptionWithParamCount:4];
    }
    else if([functionCall.name isEqualToString:@"objc_retain"] && functionCall.params.count > 0 && [[functionCall.params objectAtIndex:0] previousClassName] != nil)
    {
        return [functionCall descriptionWithParamCount:1];
    }
    return nil;
}

- (BOOL) isStackOverflow:(NSDictionary*) crashedThreadReport
{
    NSDictionary* stack = [crashedThreadReport objectForKey:@KSCrashField_Stack];
    return [[stack objectForKey:@KSCrashField_Overflow] boolValue];
}

- (BOOL) isDeadlock:(NSDictionary*) report
{
    NSDictionary* errorReport = [self errorReport:report];
    NSString* crashType = [errorReport objectForKey:@KSCrashField_Type];
    return [@KSCrashExcType_Deadlock isEqualToString:crashType];
}

- (NSString*) appendOriginatingCall:(NSString*) string callName:(NSString*) callName
{
    if(callName != nil && ![callName isEqualToString:@"main"])
    {
        return [string stringByAppendingFormat:@"\nOriginated at or in a subcall of %@", callName];
    }
    return string;
}

- (NSString*) diagnoseCrash:(NSDictionary*) report
{
    @try
    {
#ifdef DEBUG
        NSString* lastFunctionName = [[self lastInAppStackEntry:report] objectForKey:@KSCrashField_SymbolName];
#else
        NSString* lastFunctionName = @"unknown, cannot find symbol";
#endif
        NSDictionary* crashedThreadReport = [self crashedThreadReport:report];
        NSDictionary* errorReport = [self errorReport:report];

        if([self isDeadlock:report])
        {
            return [NSString stringWithFormat:@"Main thread deadlocked in %@", lastFunctionName];
        }
        
        if([self isStackOverflow:crashedThreadReport])
        {
            return [NSString stringWithFormat:@"Stack overflow in %@", lastFunctionName];
        }

        NSString* crashType = [errorReport objectForKey:@KSCrashField_Type];
        if([crashType isEqualToString:@KSCrashExcType_NSException])
        {
            NSDictionary* exception = [errorReport objectForKey:@KSCrashField_NSException];
            NSString* name = [exception objectForKey:@KSCrashField_Name];
            NSString* reason = [exception objectForKey:@KSCrashField_Reason]? [exception objectForKey:@KSCrashField_Reason]:[errorReport objectForKey:@KSCrashField_Reason];
            return [self appendOriginatingCall:[NSString stringWithFormat:@"Application threw exception %@: %@",
                                                name, reason]
                                      callName:lastFunctionName];
        }

        if([self isMemoryCorruption:report])
        {
            return @"Rogue memory write has corrupted memory.";
        }

        if([self isMathError:errorReport])
        {
            return [self appendOriginatingCall:[NSString stringWithFormat:@"Math error (usually caused from division by 0)."]
                                      callName:lastFunctionName];
        }

        KSCrashDoctorFunctionCall* functionCall = [self lastFunctionCall:report];
        NSString* zombieCall = [self zombieCall:functionCall];
        if(zombieCall != nil)
        {
            return [self appendOriginatingCall:[NSString stringWithFormat:@"Possible zombie in call: %@", zombieCall]
                                      callName:lastFunctionName];
        }

        if([self isInvalidAddress:errorReport])
        {
            uintptr_t address = (uintptr_t)[[errorReport objectForKey:@KSCrashField_Address] unsignedLongLongValue];
            if(address == 0)
            {
                return [self appendOriginatingCall:@"Attempted to dereference null pointer."
                                          callName:lastFunctionName];
            }
            return [self appendOriginatingCall:[NSString stringWithFormat:@"Attempted to dereference garbage pointer %p.", (void*)address]
                                      callName:lastFunctionName];
        }
        
        return @"Can't not diagnosize the reason.";
    }
    @catch (NSException* e)
    {
        NSArray* symbols = [e callStackSymbols];
        if(symbols)
        {
            return [NSString stringWithFormat:@"No diagnosis due to exception %@:\n%@\nPlease file a bug report to the KSCrash project.", e, symbols];
        }
        return [NSString stringWithFormat:@"No diagnosis due to exception %@\nPlease file a bug report to the KSCrash project.", e];
    }
}

@end
