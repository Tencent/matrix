//
//  KSCrashReportFilterJSON.m
//
//  Created by Karl Stenerud on 2012-05-09.
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


#import "KSCrashReportFilterJSON.h"

//#define KSLogger_LocalLevel TRACE
#import "KSLogger.h"


@interface KSCrashReportFilterJSONEncode ()

@property(nonatomic,readwrite,assign) KSJSONEncodeOption encodeOptions;

@end


@implementation KSCrashReportFilterJSONEncode

@synthesize encodeOptions = _encodeOptions;

+ (KSCrashReportFilterJSONEncode*) filterWithOptions:(KSJSONEncodeOption) options
{
    return [(KSCrashReportFilterJSONEncode*)[self alloc] initWithOptions:options];
}

- (id) initWithOptions:(KSJSONEncodeOption) options
{
    if((self = [super init]))
    {
        self.encodeOptions = options;
    }
    return self;
}

- (void) filterReports:(NSArray*) reports
          onCompletion:(KSCrashReportFilterCompletion) onCompletion
{
    NSMutableArray* filteredReports = [NSMutableArray arrayWithCapacity:[reports count]];
    for(NSDictionary* report in reports)
    {
        NSError* error = nil;
        NSData* jsonData = [KSJSONCodec encode:report
                                       options:self.encodeOptions
                                         error:&error];
        if(jsonData == nil)
        {
            kscrash_callCompletion(onCompletion, filteredReports, NO, error);
            return;
        }
        else
        {
            [filteredReports addObject:jsonData];
        }
    }

    kscrash_callCompletion(onCompletion, filteredReports, YES, nil);
}

@end


@interface KSCrashReportFilterJSONDecode ()

@property(nonatomic,readwrite,assign) KSJSONDecodeOption decodeOptions;

@end


@implementation KSCrashReportFilterJSONDecode

@synthesize decodeOptions = _encodeOptions;

+ (KSCrashReportFilterJSONDecode*) filterWithOptions:(KSJSONDecodeOption) options
{
    return [(KSCrashReportFilterJSONDecode*)[self alloc] initWithOptions:options];
}

- (id) initWithOptions:(KSJSONDecodeOption) options
{
    if((self = [super init]))
    {
        self.decodeOptions = options;
    }
    return self;
}

- (void) filterReports:(NSArray*) reports
          onCompletion:(KSCrashReportFilterCompletion) onCompletion
{
    NSMutableArray* filteredReports = [NSMutableArray arrayWithCapacity:[reports count]];
    for(NSData* data in reports)
    {
        NSError* error = nil;
        NSDictionary* report = [KSJSONCodec decode:data
                                           options:self.decodeOptions
                                             error:&error];
        if(report == nil)
        {
            kscrash_callCompletion(onCompletion, filteredReports, NO, error);
            return;
        }
        else
        {
            [filteredReports addObject:report];
        }
    }

    kscrash_callCompletion(onCompletion, filteredReports, YES, nil);
}

@end
