/*
 * Tencent is pleased to support the open source community by making
 * wechat-matrix available. Copyright (C) 2019 THL A29 Limited, a Tencent
 * company. All rights reserved. Licensed under the BSD 3-Clause License (the
 * "License"); you may not use this file except in compliance with the License.
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

#import "bundle_name_helper.h"
#import <Foundation/Foundation.h>

void bundleHelperGetAppBundleName(char *outBuffer, unsigned int bufferSize) {
    if (outBuffer == NULL || bufferSize == 0) {
        return;
    }

    NSString *bundleName = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleExecutable"];
    if (bundleName) {
        const char *cStr = [[NSString stringWithFormat:@"/%@.app/", bundleName] cStringUsingEncoding:NSUTF8StringEncoding];
        strncpy(outBuffer, cStr, bufferSize);
        outBuffer[bufferSize - 1] = 0;
    } else {
        outBuffer[0] = 0;
    }
}

void bundleHelperGetAppName(char *outBuffer, unsigned int bufferSize) {
    if (outBuffer == NULL || bufferSize == 0) {
        return;
    }

    NSString *bundleName = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleExecutable"];
    if (bundleName) {
        const char *cStr = [[NSString stringWithFormat:@"/%@.app/%@", bundleName, bundleName] cStringUsingEncoding:NSUTF8StringEncoding];
        strncpy(outBuffer, cStr, bufferSize);
        outBuffer[bufferSize - 1] = 0;
    } else {
        outBuffer[0] = 0;
    }
}
