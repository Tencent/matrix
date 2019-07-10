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

#import "MatrixBaseModel.h"

#import <objc/runtime.h>

#if __has_feature(objc_arc)
#error This file must be compiled with MRR. Use -fno-objc-arc flag.
#endif

@implementation MatrixBaseModel

- (void)encodeWithCoder:(NSCoder *)encoder {
	Class cls = [self class];
	while (cls != [NSObject class]) {
		unsigned int numberOfIvars = 0;
		Ivar *ivars = class_copyIvarList(cls, &numberOfIvars);
		for (int i = 0; i < numberOfIvars; ++i) {
			const char *type = ivar_getTypeEncoding(ivars[i]);
			NSString *key = [NSString stringWithUTF8String:ivar_getName(ivars[i])];
			if (key == nil || key.length == 0) {
				continue;
			}
			id value = [self valueForKey:key];
			if (value) {
				switch (type[0]) {
					case _C_STRUCT_B: {
						NSUInteger ivarSize = 0;
						NSUInteger ivarAlignment = 0;
						NSGetSizeAndAlignment(type, &ivarSize, &ivarAlignment);
						NSData *data = [NSData dataWithBytes:(const char *)self + ivar_getOffset(ivars[i]) length:ivarSize];
						[encoder encodeObject:data forKey:key];
					}
						break;
					default:
						[encoder encodeObject:value forKey:key];
						break;
				}
			}
		}
		if (ivars) {
			free(ivars);
		}
		
		cls = class_getSuperclass(cls);
	}
}

- (id)initWithCoder:(NSCoder *)decoder {
	self = [super init];
	if (self) {
		Class cls = [self class];
		while (cls != [NSObject class]) {
			unsigned int numberOfIvars = 0;
			Ivar *ivars = class_copyIvarList(cls, &numberOfIvars);
			for (int i = 0; i < numberOfIvars; ++i) {
				const char *type = ivar_getTypeEncoding(ivars[i]);
				NSString *key = [NSString stringWithUTF8String:ivar_getName(ivars[i])];
				if (key == nil || key.length == 0) {
					continue;
				}
				id value = [decoder decodeObjectForKey:key];
				if (value) {
					switch (type[0]) {
						case _C_STRUCT_B: {
							NSUInteger ivarSize = 0;
							NSUInteger ivarAlignment = 0;
							NSGetSizeAndAlignment(type, &ivarSize, &ivarAlignment);
							NSData *data = [decoder decodeObjectForKey:key];
							char *sourceIvarLocation = (char *)self+ ivar_getOffset(ivars[i]);
							[data getBytes:sourceIvarLocation length:ivarSize];
							memcpy((char *)self + ivar_getOffset(ivars[i]), sourceIvarLocation, ivarSize);
						}
							break;
						default:
							[self setValue:value forKey:key];
							break;
					}
				}
			}
			
			if (ivars) {
				free(ivars);
			}
			cls = class_getSuperclass(cls);
		}
	}
	
	return self;
}

@end
