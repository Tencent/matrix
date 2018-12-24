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

package com.tencent.mm.arscutil.io;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Created by jinqiuchen on 18/7/29.
 */

public class LittleEndianInputStream extends InputStream {

    private RandomAccessFile original;


    public LittleEndianInputStream(String file) throws FileNotFoundException {
        this(new RandomAccessFile(file, "r"));
    }


    public LittleEndianInputStream(RandomAccessFile original) {
        this.original = original;
    }

    @Override
    public int read() throws IOException {
        // TODO Auto-generated method stub
        return original.read();
    }

    public short readShort() throws IOException {
        ByteBuffer byteBuffer = ByteBuffer.allocate(2);
        byteBuffer.clear();
        byteBuffer.order(ByteOrder.LITTLE_ENDIAN);
        byteBuffer.put(original.readByte());
        byteBuffer.put(original.readByte());
        byteBuffer.flip();
        return byteBuffer.getShort();
    }

    public int readInt() throws IOException {
        ByteBuffer byteBuffer = ByteBuffer.allocate(4);
        byteBuffer.order(ByteOrder.LITTLE_ENDIAN);
        byteBuffer.clear();
        for (int i = 1; i <= 4; i++) {
            byteBuffer.put(original.readByte());
        }
        byteBuffer.flip();
        return byteBuffer.getInt();
    }

    public byte readByte() throws IOException {
        return original.readByte();
    }

    public void readByte(byte[] buffer) throws IOException {
        readByte(buffer, 0, buffer.length);
    }

    public void readByte(byte[] buffer, int offset, int length) throws IOException {
        ByteBuffer byteBuffer = ByteBuffer.allocate(length);
        byteBuffer.clear();
        for (int i = 1; i <= length; i++) {
            byteBuffer.put(original.readByte());
        }
        byteBuffer.flip();
        byteBuffer.get(buffer, offset, length);
    }

    public void seek(long pos) throws IOException {
        original.seek(pos);
    }

    public long getFilePointer() throws IOException {
        return original.getFilePointer();
    }

    public long getFileLength() throws IOException {
        return original.length();
    }

    @Override
    public void close() throws IOException {
        // TODO Auto-generated method stub
        super.close();
        original.close();
    }

}
