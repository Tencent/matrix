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

#import "WCCrashReportInterpreter.h"
#import "KSStackCursor_Backtrace.h"
#import "WCCrashReportInfoUtil.h"

@implementation WCCrashReportInterpreter

+ (NSData *)interpretReport:(NSData *)reportFileData
{
    NSString *fileString = [[NSString alloc] initWithData:reportFileData encoding:NSUTF8StringEncoding];
    NSArray *reportLineArray = [fileString componentsSeparatedByString:@"\n"];
    NSArray *addressArray = [WCCrashReportInterpreter p_getAddressFromArray:reportLineArray];
    NSArray *symbolicatedAdressArray = [WCCrashReportInterpreter p_getSymbolicatedStack:addressArray];
    NSMutableDictionary *reportUUIDDict = [WCCrashReportInterpreter p_getBinaryImageUUID:reportLineArray];
    NSString *symbolicatedReport = [WCCrashReportInterpreter p_getReportWithReportLineArray:reportLineArray withSymblicatedAddressArray:symbolicatedAdressArray withBinaryUUID:reportUUIDDict];
    return [symbolicatedReport dataUsingEncoding:NSUTF8StringEncoding];
}

+ (NSString *)interpretText:(NSString *)reportString
{
    NSArray *reportLineArray = [WCCrashReportInterpreter p_cleanEmptyString:[reportString componentsSeparatedByString:@"\n"]];
    NSArray *addressArray = [WCCrashReportInterpreter p_getTextStackArray:reportLineArray];
    NSArray *symbolicatedAddressArray = [WCCrashReportInterpreter p_getSymbolicatedStack:addressArray];
    return [WCCrashReportInterpreter p_getNewCrashTextString:symbolicatedAddressArray];
}

+ (NSArray *)p_getTextStackArray:(NSArray *)reportLineArray
{
    NSUInteger lineNum = 0;
    
    NSMutableArray *addressArray = [[NSMutableArray alloc] init];
    
    for (lineNum = 0; lineNum < [reportLineArray count]; lineNum ++) {
        NSString *lineString = [reportLineArray objectAtIndex:lineNum];
        if (lineString != nil && lineString.length > 0) {
            NSString *addrlineString = [lineString stringByTrimmingCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@"\t"]];
            NSArray *threadPartArray = [WCCrashReportInterpreter p_cleanEmptyString:[addrlineString componentsSeparatedByString:@" "]];
            if ([threadPartArray count] >= 3) {
                NSMutableDictionary *stackInfo = [[NSMutableDictionary alloc] init];
                [stackInfo setObject:threadPartArray[1] forKey:@"imageName"];
                BOOL bGetBiaAddress = NO;
                for (NSString *currentStringPart in threadPartArray) {
                    if (bGetBiaAddress) {
                        [stackInfo setObject:currentStringPart forKey:@"biaAddress"];
                        break;
                    }
                    if ([currentStringPart isEqualToString:@"+"]) {
                        bGetBiaAddress = YES;
                    }
                }
                [addressArray addObject:[stackInfo copy]];
            }
        }
    }
    return [addressArray copy];
}

+ (NSArray *)p_getSymbolicatedStack:(NSArray *)addressArray
{
    NSArray *cleanedAddressArray = [WCCrashReportInterpreter p_cleanAddressArray:addressArray];
    size_t addressCount = [cleanedAddressArray count];
    NSMutableArray *symbolicatedStack = [[NSMutableArray alloc] init];

    if (addressCount > 0) {
        uintptr_t *currentThreadStack = (uintptr_t *) malloc(sizeof(uintptr_t) * addressCount);
        if (currentThreadStack != NULL) {
            for (int i = 0; i < addressCount; i++) {
                NSDictionary *currentAddressInfo = cleanedAddressArray[i];
                NSString *imageName = currentAddressInfo[@"imageName"];
                uintptr_t imageAddr = [WCCrashReportInterpreter getImageStartAddr:imageName];
                NSString *addressString = currentAddressInfo[@"biaAddress"];
                if (addressString != nil && [addressString length] > 0) {
                    uintptr_t biaAddress = (uintptr_t)[addressString longLongValue];
                    currentThreadStack[i] = imageAddr + biaAddress;
                } else {
                    currentThreadStack[i] = 0;
                }
            }
            KSStackCursor *stackCursor = (KSStackCursor *) malloc(sizeof(KSStackCursor));
            kssc_initWithBacktrace(stackCursor, currentThreadStack, (int) addressCount, 0);

            NSUInteger index = 0;
            while (stackCursor->advanceCursor(stackCursor)) {
                NSMutableDictionary *addrInfoDict = [[NSMutableDictionary alloc] init];
                if (stackCursor->symbolicate(stackCursor)) {
                    if (stackCursor->stackEntry.imageName != NULL) {
                        [addrInfoDict setObject:[NSString stringWithUTF8String:wxg_lastPathEntry(stackCursor->stackEntry.imageName)] forKey:@"imageName"];
                    }
                    [addrInfoDict setValue:[NSNumber numberWithUnsignedInteger:stackCursor->stackEntry.imageAddress] forKey:@"imageAddress"];
                    if (stackCursor->stackEntry.symbolName != NULL) {
                        [addrInfoDict setObject:[NSString stringWithUTF8String:stackCursor->stackEntry.symbolName] forKey:@"symbolName"];
                    }
                    [addrInfoDict setValue:[NSNumber numberWithUnsignedInteger:stackCursor->stackEntry.symbolAddress] forKey:@"symbolAddress"];
                }
                NSNumber *stackAddress = [NSNumber numberWithUnsignedInteger:stackCursor->stackEntry.address];
                [addrInfoDict setValue:stackAddress forKey:@"instructionAddress"];

                if (index < addressCount) {
                    NSString *currentOriginLine = cleanedAddressArray[index][@"originLine"];
                    [addrInfoDict setValue:currentOriginLine forKey:@"originLine"];
                }

                index++;
                [symbolicatedStack addObject:[addrInfoDict copy]];
            }

            KSStackCursor_Backtrace_Context *context = (KSStackCursor_Backtrace_Context *) stackCursor->context;
            if (context->backtrace) {
                free((uintptr_t *) context->backtrace);
            }
            free(stackCursor);
        }
    }
    return symbolicatedStack;
}

+ (uintptr_t)getImageStartAddr:(NSString *)currentImageName
{
    NSArray *binaryImages = [[WCCrashReportInfoUtil sharedInstance] getBinaryImages];
    for (NSDictionary *imageInfoDict in binaryImages) {
        NSString *imageName = imageInfoDict[@"name"];
        NSNumber *imageAddr = imageInfoDict[@"image_addr"];
        if ([[imageName uppercaseString] isEqualToString:[currentImageName uppercaseString]]) {
            return [imageAddr unsignedIntegerValue];
        }
    }
    return 0;
}

+ (NSString *)getImageUUID:(NSString *)currentImageName
{
    NSArray *binaryImages = [[WCCrashReportInfoUtil sharedInstance] getBinaryImages];
    for (NSDictionary *imageInfoDict in binaryImages) {
        NSString *imageName = imageInfoDict[@"name"];
        NSString *imageUUID = imageInfoDict[@"uuid"];
        if ([[imageName uppercaseString] isEqualToString:[currentImageName uppercaseString]]) {
            return imageUUID;
        }
    }
    return @"";
}

+ (NSArray *)p_cleanEmptyString:(NSArray *)stringArray
{
    NSMutableArray *cleanedStringArray = [[NSMutableArray alloc] init];
    for (NSString *oneString in stringArray) {
        if (oneString && [oneString length] > 0) {
            [cleanedStringArray addObject:oneString];
        }
    }
    return [cleanedStringArray copy];
}

+ (NSArray *)p_cleanAddressArray:(NSArray *)addressArray
{
    NSMutableArray *cleanedAddressArray = [[NSMutableArray alloc] init];
    for (NSDictionary *currentAddressInfo in addressArray) {
        NSString *imgName = currentAddressInfo[@"imageName"];
        if ([imgName containsString:@"?"]) {
            continue;
        }
        [cleanedAddressArray addObject:currentAddressInfo];
    }
    return [cleanedAddressArray copy];
}

+ (NSString *)p_getNewCrashTextString:(NSArray *)symbolicatedAddressArray
{
    NSString *newCrashReport = @"";
    
    int index = 0;
    for (NSDictionary *addressInfo in symbolicatedAddressArray) {
        newCrashReport = [newCrashReport stringByAppendingString:[NSString stringWithFormat:@"%d  ", index]];
        newCrashReport = [newCrashReport stringByAppendingString:[NSString stringWithFormat:@"%@ ", addressInfo[@"imageName"]]];
        NSNumber *imageAddress = (NSNumber *)addressInfo[@"imageAddress"];
        newCrashReport = [newCrashReport stringByAppendingString:[NSString stringWithFormat:@"0x%llx ", [imageAddress unsignedLongLongValue]]];
        newCrashReport = [newCrashReport stringByAppendingString:[NSString stringWithFormat:@"%@ ", addressInfo[@"symbolName"]]];
        NSNumber *instructionAddrsss = (NSNumber *)addressInfo[@"instructionAddress"];
        NSNumber *symbolAddress = (NSNumber *)addressInfo[@"symbolAddress"];
        newCrashReport = [newCrashReport stringByAppendingString:[NSString stringWithFormat:@"+ %lld \n", [instructionAddrsss unsignedLongLongValue] - [symbolAddress unsignedLongLongValue]]];
        newCrashReport = [newCrashReport stringByAppendingString:@"\n"];
        
        index ++;
    }
    
    return newCrashReport;
}

+ (NSString *)p_getReportWithReportLineArray:(NSArray *)reportLineArray withSymblicatedAddressArray:(NSArray *)symbolicatedAddressArray withBinaryUUID:(NSMutableDictionary *)reportUUIDDict
{
    NSMutableString *newReport = [@"" mutableCopy];
    for (NSString *lineString in reportLineArray) {
        if ([WCCrashReportInterpreter p_validStackFormat:lineString]) {
            NSString *symString = [WCCrashReportInterpreter p_getSymbolicatedLine:lineString withSymblicatedAddressArray:symbolicatedAddressArray withBinaryUUID:reportUUIDDict];
            [newReport appendFormat:@"%@\n", symString];
        } else {
            [newReport appendFormat:@"%@\n", lineString];
        }
    }
    return [newReport copy];
}

+ (BOOL)p_validStackFormat:(NSString *)reportLineString
{
    NSArray *linePartArray = [WCCrashReportInterpreter p_cleanEmptyString:[reportLineString componentsSeparatedByString:@" "]];
    if ([WCCrashReportInterpreter p_hasPlusPartInArray:linePartArray]) { // 有加号，说明有可能有堆栈
        if ([WCCrashReportInterpreter p_validFirstStackFormat:linePartArray]) {
            return YES;
        }
        if ([WCCrashReportInterpreter p_validSecondStackFormat:linePartArray]) {
            return YES;
        }
    }
    return NO;
}

+ (NSString *)p_getSymbolicatedLine:(NSString *)reportLineString withSymblicatedAddressArray:(NSArray *)symbolicatedAddressArray withBinaryUUID:(NSMutableDictionary *)reportUUIDDict
{
    NSArray *linePartArray = [WCCrashReportInterpreter p_cleanEmptyString:[reportLineString componentsSeparatedByString:@" "]];
    if ([WCCrashReportInterpreter p_validFirstStackFormat:linePartArray]) {
        // 0   libsystem_kernel.dylib            0x0000000238bdb104 0x238bb8000 + 143620
        NSMutableDictionary *addressInfo = [WCCrashReportInterpreter p_getAddressInfoFromFirstStackFormat:linePartArray];
        NSMutableDictionary *symAddressInfo = [WCCrashReportInterpreter p_getSymblicatedAddressInfo:addressInfo fromSymblicatedAddressArray:symbolicatedAddressArray];
        if (symAddressInfo == nil) {
            return reportLineString;
        }
        NSString *symbolName = symAddressInfo[@"symbolName"];
        if (symbolName == nil || symbolName.length == 0 || [symbolName containsString:@"null"]) {
            return reportLineString;
        } else {
            NSString *imageName = addressInfo[@"imageName"];
            NSString *reportUUID = [reportUUIDDict objectForKey:imageName];
            NSString *deviceUUID = [[WCCrashReportInterpreter getImageUUID:imageName] lowercaseString];
            deviceUUID = [deviceUUID stringByReplacingOccurrencesOfString:@"-" withString:@""];
            if ([reportUUID isEqualToString:deviceUUID]) {
                return [reportLineString stringByAppendingFormat:@" %@", symbolName];
            } else {
                return [reportLineString stringByAppendingString:@" Images Not Matched"];
            }
        }
    }
    
    if ([WCCrashReportInterpreter p_validSecondStackFormat:linePartArray]) {
        //7  ??? (libsystem_pthread.dylib + 45580) [0x203f6b20c]
        NSMutableDictionary *addressInfo = [WCCrashReportInterpreter p_getAddressInfoFromSecondStackFormat:linePartArray];
        NSMutableDictionary *symAddressInfo = [WCCrashReportInterpreter p_getSymblicatedAddressInfo:addressInfo fromSymblicatedAddressArray:symbolicatedAddressArray];
        if (symAddressInfo == nil) {
            return reportLineString;
        }
        NSString *symbolName = symAddressInfo[@"symbolName"];
        if (symbolName == nil || symbolName.length == 0 || [symbolName containsString:@"null"]) {
            return reportLineString;
        } else {
            NSString *imageName = addressInfo[@"imageName"];
            NSString *reportUUID = [reportUUIDDict objectForKey:imageName];
            NSString *deviceUUID = [[WCCrashReportInterpreter getImageUUID:imageName] lowercaseString];
            deviceUUID = [deviceUUID stringByReplacingOccurrencesOfString:@"-" withString:@""];
            NSRange range = [reportLineString rangeOfString:linePartArray[0]];
            NSMutableString *newString = [@"" mutableCopy];
            for (NSUInteger i = 0; i < range.location; i++) {
                [newString appendString:@" "];
            }
            [newString appendString:linePartArray[0]];
            if ([reportUUID isEqualToString:deviceUUID]) {
                [newString appendFormat:@"  %@ %@ %@ %@ %@", symbolName, linePartArray[2], linePartArray[3], linePartArray[4], linePartArray[5]];
            } else {
                [newString appendFormat:@"  %@ %@ %@ %@ %@", @"Images Not Matched", linePartArray[2], linePartArray[3], linePartArray[4], linePartArray[5]];
            }
            return [newString copy];
        }
    }
    return @"";
}

+ (NSMutableDictionary *)p_getSymblicatedAddressInfo:(NSMutableDictionary *)addressInfo fromSymblicatedAddressArray:(NSArray *)symbolicatedAddressArray
{
    NSString *firstImage = addressInfo[@"imageName"];
    NSString *firstBias = addressInfo[@"biaAddress"];
    long long biaAddress = [firstBias longLongValue];
    for (NSMutableDictionary *symbAddressInfo in symbolicatedAddressArray) {
        NSString *curImage = symbAddressInfo[@"imageName"];
        if ([curImage isEqualToString:firstImage]) {
            NSNumber *imageAddress = (NSNumber *)symbAddressInfo[@"imageAddress"];
            NSNumber *instructionAddrsss = (NSNumber *) symbAddressInfo[@"instructionAddress"];
            long long curBias = [instructionAddrsss longLongValue] - [imageAddress longLongValue];
            if (curBias == biaAddress) {
                return symbAddressInfo;
            }
        }
    }
    return nil;
}

+ (NSArray *)p_getAddressFromArray:(NSArray *)reportLineArray
{
    NSMutableArray *addressArray = [[NSMutableArray alloc] init];
    
    for (NSString *reportLine in reportLineArray) {
        if (reportLine.length == 0 || reportLine == nil) {
            continue;
        }
        NSArray *linePartArray = [WCCrashReportInterpreter p_cleanEmptyString:[reportLine componentsSeparatedByString:@" "]];
        if ([WCCrashReportInterpreter p_hasPlusPartInArray:linePartArray]) { // 有加号，说明有可能有堆栈
            NSMutableDictionary *addressInfo = nil;
            if ([WCCrashReportInterpreter p_validSecondStackFormat:linePartArray]) {
                addressInfo = [WCCrashReportInterpreter p_getAddressInfoFromSecondStackFormat:linePartArray];
            }
            if ([WCCrashReportInterpreter p_validFirstStackFormat:linePartArray]) {
                addressInfo = [WCCrashReportInterpreter p_getAddressInfoFromFirstStackFormat:linePartArray];
            }
            if (addressInfo != nil) {
                [addressInfo setObject:reportLine forKey:@"originLine"];
                [addressArray addObject:addressInfo];
            }
        }
    }
    return addressArray;
}

+ (BOOL)p_validFirstStackFormat:(NSArray *)linePartArray //0   libsystem_kernel.dylib            0x0000000238bdb104 0x238bb8000 + 143620
{
    if ([linePartArray count] != 6) {
        return NO;
    }
    NSString *fourpart = linePartArray[4];
    if ([fourpart isEqualToString:@"+"]) {
        return YES;
    }
    return NO;
}

+ (NSMutableDictionary *)p_getAddressInfoFromFirstStackFormat:(NSArray *)linePartArray
{
    NSMutableDictionary *stackInfo = [[NSMutableDictionary alloc] init];
    NSString *binaryPart = linePartArray[1];
    NSString *biasPart = linePartArray[5];
    if (binaryPart && [binaryPart length] > 0 && biasPart && [biasPart length] > 0) {
        [stackInfo setObject:binaryPart forKey:@"imageName"];
        [stackInfo setObject:biasPart forKey:@"biaAddress"];
        return stackInfo;
    }
    return nil;
}

+ (BOOL)p_validSecondStackFormat:(NSArray *)linePartArray //7  ??? (libsystem_pthread.dylib + 45580) [0x203f6b20c]
{
    if ([linePartArray count] != 6) {
        return NO;
    }
    NSString *secPart = linePartArray[2];
    NSString *threePart = linePartArray[3];
    if ([threePart isEqualToString:@"+"] == NO) {
        return NO;
    }
    NSString *fourPart = linePartArray[4];
    NSString *fivePart = linePartArray[5];
    if ([secPart containsString:@"("] && [fourPart containsString:@")"] &&
        [fivePart containsString:@"["] && [fivePart containsString:@"]"]) {
        return YES;
    }
    return NO;;
}

+ (NSMutableDictionary *)p_getAddressInfoFromSecondStackFormat:(NSArray *)linePartArray
{
    NSMutableDictionary *stackInfo = [[NSMutableDictionary alloc] init];
    NSString *binaryPart = [linePartArray[2] stringByReplacingOccurrencesOfString:@"(" withString:@""];
    NSString *biasPart = [linePartArray[4] stringByReplacingOccurrencesOfString:@")" withString:@""];
    if (binaryPart && [binaryPart length] > 0 && biasPart && [biasPart length] > 0) {
        [stackInfo setObject:binaryPart forKey:@"imageName"];
        [stackInfo setObject:biasPart forKey:@"biaAddress"];
        return stackInfo;
    }
    return nil;
}

+ (BOOL)p_hasPlusPartInArray:(NSArray *)linePartArray
{
    for (NSString *linePart in linePartArray) {
        if ([linePart isEqualToString:@"+"]) {
            return YES;
        }
    }
    return NO;
}

+ (NSMutableDictionary *)p_getBinaryImageUUID:(NSArray *)reportLineArray
{
    BOOL startGetBinaryImages = NO;
    NSMutableDictionary *binaryImageDict = [[NSMutableDictionary alloc] init];
    for (NSString *reportLine in reportLineArray) {
        if ([reportLine containsString:@"Binary Images"]) {
            startGetBinaryImages = YES;
            continue;
        }
        if (startGetBinaryImages) {
            NSArray *newLinePart = [reportLine componentsSeparatedByString:@"<"];
            if ([newLinePart count] == 2) {
                NSString *lastPart = newLinePart[1];
                NSArray *newnewLinePart = [lastPart componentsSeparatedByString:@">"];
                if ([newnewLinePart count] == 2) {
                    NSString *uuidStr = [newnewLinePart[0] lowercaseString];
                    uuidStr = [uuidStr stringByReplacingOccurrencesOfString:@"-" withString:@""];
                    NSArray *linePartArray = [WCCrashReportInterpreter p_cleanEmptyString:[reportLine componentsSeparatedByString:@" "]];
                    NSString *binaryImages = linePartArray[3];
                    [binaryImageDict setObject:uuidStr forKey:binaryImages];
                }
            }
        }
    }
    return binaryImageDict;
}

@end
