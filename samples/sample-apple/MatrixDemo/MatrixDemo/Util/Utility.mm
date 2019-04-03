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

#import "Utility.h"

#define UIColorFromRGB(rgbValue) \
        [UIColor colorWithRed:((float)((rgbValue & 0xFF0000) >> 16))/255.0 \
                        green:((float)((rgbValue & 0x00FF00) >>  8))/255.0 \
                         blue:((float)((rgbValue & 0x0000FF) >>  0))/255.0 \
                        alpha:1.0]

@implementation Utility

+ (UIButton *)genBigGreenButtonWithFrame:(CGRect)btnFrame
{
    UIButton *btn = [[UIButton alloc] initWithFrame:btnFrame];
    [btn setBackgroundColor:UIColorFromRGB(0x07C160)];
    [btn setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    btn.layer.cornerRadius = 4;
    btn.clipsToBounds = YES;
    return btn;
}

+ (UIButton *)genBigRedButtonWithFrame:(CGRect)btnFrame
{
    UIButton *btn = [[UIButton alloc] initWithFrame:btnFrame];
    [btn setBackgroundColor:UIColorFromRGB(0xE64340)];
    [btn setTitleColor:UIColorFromRGB(0xFFFFFF) forState:UIControlStateNormal];
    btn.layer.cornerRadius = 4;
    btn.clipsToBounds = YES;
    return btn;
}

@end
