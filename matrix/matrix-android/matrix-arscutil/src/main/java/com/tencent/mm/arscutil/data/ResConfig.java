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

public class ResConfig {

    private int size; // 总大小（包括size字段）， 4 bytes
    private byte[] content;

    public int getSize() {
        return size;
    }

    public void setSize(int size) {
        this.size = size;
    }

    public byte[] getContent() {
        return content;
    }

    public void setContent(byte[] content) {
        this.content = content;
    }

    public byte[] toBytes() {
        ByteBuffer byteBuffer = ByteBuffer.allocate(size);
        byteBuffer.order(ByteOrder.LITTLE_ENDIAN);
        byteBuffer.clear();
        byteBuffer.putInt(size);
        if (content != null && content.length > 0) {
            byteBuffer.put(content);
        }
        byteBuffer.flip();
        return byteBuffer.array();
    }

}
