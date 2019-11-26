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

/**
 * Created by jinqiuchen on 18/7/29.
 */

public abstract class ResChunk {

    protected long start; // chunk开始位置

    protected short type; // 类型, 2 bytes
    protected short headSize; // 头大小, 2 bytes
    protected int chunkSize; // chunk大小, 4 bytes

    //header和chunk都要求4字节对齐，不够在后面补0
    protected int headPadding;    //头尾部padding
    protected int chunkPadding;  //chunk尾部padding

    public short getType() {
        return type;
    }

    public void setType(short type) {
        this.type = type;
    }

    public short getHeadSize() {
        return headSize;
    }

    public void setHeadSize(short headSize) {
        this.headSize = headSize;
    }

    public int getChunkSize() {
        return chunkSize;
    }

    public void setChunkSize(int chunkSize) {
        this.chunkSize = chunkSize;
    }

    public long getStart() {
        return start;
    }

    public void setStart(long start) {
        this.start = start;
    }

    public int getHeadPadding() {
        return headPadding;
    }

    public void setHeadPadding(int headPadding) {
        this.headPadding = headPadding;
    }

    public int getChunkPadding() {
        return chunkPadding;
    }

    public void setChunkPadding(int chunkPadding) {
        this.chunkPadding = chunkPadding;
    }

    public abstract byte[] toBytes();

}
