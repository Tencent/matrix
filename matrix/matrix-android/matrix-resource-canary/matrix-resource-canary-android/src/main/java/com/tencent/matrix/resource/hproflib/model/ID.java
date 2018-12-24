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

package com.tencent.matrix.resource.hproflib.model;

import java.util.Arrays;

/**
 * Created by tangyinsheng on 2017/6/25.
 */

public final class ID {
    private final byte[] mIdBytes;

    public static ID createNullID(int size) {
        return new ID(new byte[size]);
    }

    public ID(byte[] idBytes) {
        final int len = idBytes.length;
        mIdBytes = new byte[len];
        System.arraycopy(idBytes, 0, mIdBytes, 0, len);
    }

    public byte[] getBytes() {
        return mIdBytes;
    }

    public int getSize() {
        return mIdBytes.length;
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof ID)) {
            return false;
        }
        return Arrays.equals(mIdBytes, ((ID) obj).mIdBytes);
    }

    @Override
    public int hashCode() {
        return Arrays.hashCode(mIdBytes);
    }

    @Override
    public String toString() {
        final StringBuilder sb = new StringBuilder();
        sb.append("0x");
        for (byte b : mIdBytes) {
            final int eb = b & 0xFF;
            sb.append(Integer.toHexString(eb));
        }
        return sb.toString();
    }
}
