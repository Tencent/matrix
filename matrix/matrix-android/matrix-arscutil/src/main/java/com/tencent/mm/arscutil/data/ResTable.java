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

package com.tencent.mm.arscutil.data;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Created by jinqiuchen on 18/7/29.
 */

public class ResTable extends ResChunk {

    private int packageCount; // 资源package数目, 4 bytes
    private ResStringBlock globalStringPool; // 全局资源池
    private ResPackage[] packages; // 资源package数组

    public int getPackageCount() {
        return packageCount;
    }

    public void setPackageCount(int packageCount) {
        this.packageCount = packageCount;
    }

    public ResStringBlock getGlobalStringPool() {
        return globalStringPool;
    }

    public void setGlobalStringPool(ResStringBlock globalStringPool) {
        this.globalStringPool = globalStringPool;
    }

    public ResPackage[] getPackages() {
        return packages;
    }

    public void setPackages(ResPackage[] packages) {
        this.packages = packages;
    }

    public void refresh() {
        recomputeChunkSize();
    }

    private void recomputeChunkSize() {
        chunkSize = 0;
        chunkSize += headSize;
        if (globalStringPool != null) {
            chunkSize += globalStringPool.getChunkSize();
        }
        if (packages != null) {
            for (ResPackage resPackage : packages) {
                chunkSize += resPackage.getChunkSize();
            }
        }
        if (chunkSize % 4 != 0) {
            chunkPadding = 4 - chunkSize % 4;
            chunkSize += chunkPadding;
        } else {
            chunkPadding = 0;
        }
    }

    @Override
    public byte[] toBytes() {
        ByteBuffer byteBuffer = ByteBuffer.allocate(chunkSize);
        byteBuffer.order(ByteOrder.LITTLE_ENDIAN);
        byteBuffer.clear();
        byteBuffer.putShort(type);
        byteBuffer.putShort(headSize);
        byteBuffer.putInt(chunkSize);
        byteBuffer.putInt(packageCount);
        if (headPadding > 0) {
            byteBuffer.put(new byte[headPadding]);
        }
        if (globalStringPool != null) {
            byteBuffer.put(globalStringPool.toBytes());
        }
        if (packages != null) {
            for (int i = 0; i < packages.length; i++) {
                byteBuffer.put(packages[i].toBytes());
            }
        }
        if (chunkPadding > 0) {
            byteBuffer.put(new byte[chunkPadding]);
        }
        byteBuffer.flip();

        return byteBuffer.array();
    }

}
