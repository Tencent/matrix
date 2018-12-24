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

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Created by jinqiuchen on 18/7/29.
 */

public class ResMapValue {

    private int name;
    private ResValue resValue;


    public int getName() {
        return name;
    }

    public void setName(int name) {
        this.name = name;
    }

    public ResValue getResValue() {
        return resValue;
    }

    public void setResValue(ResValue resValue) {
        this.resValue = resValue;
    }

    public byte[] toBytes() throws IOException {
        ByteBuffer byteBuffer = ByteBuffer.allocate(4 + resValue.getSize());
        byteBuffer.order(ByteOrder.LITTLE_ENDIAN);
        byteBuffer.clear();
        byteBuffer.putInt(name);
        byteBuffer.put(resValue.toBytes());
        byteBuffer.flip();
        return byteBuffer.array();
    }
}
