/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
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

package com.tencent.matrix.resource.common.utils;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

/**
 * Created by tangyinsheng on 2017/7/7.
 */

public final class DigestUtil {
    private static final char[] HEX_DIGITS
            = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

    public static String getMD5String(byte[] buffer) {
        try {
            final MessageDigest md = MessageDigest.getInstance("MD5");
            md.update(buffer);
            final byte[] resBytes = md.digest();
            return bytesToHexString(resBytes);
        } catch (NoSuchAlgorithmException e) {
            // Should not happen.
            throw new IllegalStateException(e);
        }
    }

    private static String bytesToHexString(byte[] bytes) {
        final StringBuilder sb = new StringBuilder();
        for (byte b : bytes) {
            if (b >= 0 && b <= 15) {
                sb.append('0').append(HEX_DIGITS[b]);
            } else {
                sb.append(HEX_DIGITS[(b >> 4) & 0x0F]).append(HEX_DIGITS[b & 0x0F]);
            }
        }
        return sb.toString();
    }

    private DigestUtil() {
        throw new UnsupportedOperationException();
    }
}
