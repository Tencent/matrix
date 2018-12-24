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

import java.io.UnsupportedEncodingException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.util.List;

/**
 * Created by jinqiuchen on 18/7/29.
 */

public class ResStringBlock extends ResChunk {

    private int stringCount; // string数目, 4 bytes
    private int styleCount; // style数目, 4 bytes
    private int flag; // 4 bytes
    private int stringStart; // string列表的起始位置, 4 bytes
    private int styleStart; // style列表的起始位置, 4 bytes
    private List<Integer> stringOffsets; // 记录每个string相对于string列表起始位置的offset
    private List<Integer> styleOffsets; // 记录每个style相对于style列表起始位置的offset
    private List<ByteBuffer> strings; // string列表
    private byte[] styles; // 所有的style

    public int getStringCount() {
        return stringCount;
    }

    public void setStringCount(int stringCount) {
        this.stringCount = stringCount;
    }

    public int getStyleCount() {
        return styleCount;
    }

    public void setStyleCount(int styleCount) {
        this.styleCount = styleCount;
    }

    public int getFlag() {
        return flag;
    }

    public void setFlag(int flag) {
        this.flag = flag;
    }

    public int getStringStart() {
        return stringStart;
    }

    public void setStringStart(int stringStart) {
        this.stringStart = stringStart;
    }

    public int getStyleStart() {
        return styleStart;
    }

    public void setStyleStart(int styleStart) {
        this.styleStart = styleStart;
    }

    public List<Integer> getStringOffsets() {
        return stringOffsets;
    }

    public void setStringOffsets(List<Integer> stringOffsets) {
        this.stringOffsets = stringOffsets;
    }

    public List<Integer> getStyleOffsets() {
        return styleOffsets;
    }

    public void setStyleOffsets(List<Integer> styleOffsets) {
        this.styleOffsets = styleOffsets;
    }

    public List<ByteBuffer> getStrings() {
        return strings;
    }

    public void setStrings(List<ByteBuffer> strings) {
        this.strings = strings;
    }

    public byte[] getStyles() {
        return styles;
    }

    public void setStyles(byte[] styles) {
        this.styles = styles;
    }

    public Charset getCharSet() {
        if ((flag & ArscConstants.RES_STRING_POOL_UTF8_FLAG) != 0) {
            return StandardCharsets.UTF_8;
        } else {
            return StandardCharsets.UTF_16LE;
        }
    }

    public void refresh() {
        for (int i = 1; i < stringCount; i++) {
            stringOffsets.set(i, stringOffsets.get(i - 1) + strings.get(i - 1).limit());
        }
        styleStart = stringStart + stringOffsets.get(stringCount - 1) + strings.get(stringCount - 1).limit();
        recomputeChunkSize();
    }

    private void recomputeChunkSize() {
        chunkSize = 0;
        chunkSize += headSize;
        if (stringOffsets != null) {
            chunkSize += stringOffsets.size() * 4;
        }
        if (styleOffsets != null) {
            chunkSize += styleOffsets.size() * 4;
        }
        if (strings != null) {
            for (ByteBuffer buffer : strings) {
                chunkSize += buffer.limit();
            }
        }
        if (styles != null) {
            chunkSize += styles.length;
        }
    }

    @Override
    public byte[] toBytes() throws UnsupportedEncodingException {
        ByteBuffer byteBuffer = ByteBuffer.allocate(chunkSize);
        byteBuffer.order(ByteOrder.LITTLE_ENDIAN);
        byteBuffer.clear();
        byteBuffer.putShort(type);
        byteBuffer.putShort(headSize);
        byteBuffer.putInt(chunkSize);
        byteBuffer.putInt(stringCount);
        byteBuffer.putInt(styleCount);
        byteBuffer.putInt(flag);
        byteBuffer.putInt(stringStart);
        byteBuffer.putInt(styleStart);
        if (headPaddingSize > 0) {
            byteBuffer.put(new byte[headPaddingSize]);
        }
        if (stringOffsets != null) {
            for (int i = 0; i < stringOffsets.size(); i++) {
                byteBuffer.putInt(stringOffsets.get(i));
            }
        }
        if (styleOffsets != null) {
            for (int i = 0; i < styleOffsets.size(); i++) {
                byteBuffer.putInt(styleOffsets.get(i));
            }
        }
        if (strings != null) {
            for (int i = 0; i < strings.size(); i++) {
                byteBuffer.put(strings.get(i).array());
            }
        }
        if (styles != null) {
            byteBuffer.put(styles);
        }
        if (chunkPaddingSize > 0) {
            byteBuffer.put(new byte[chunkPaddingSize]);
        }
        byteBuffer.flip();
        return byteBuffer.array();
    }

}
