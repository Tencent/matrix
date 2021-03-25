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
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.util.List;
import java.util.Map;

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

    private Map<String, Integer> stringIndexMap;

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

    public Map<String, Integer> getStringIndexMap() {
        return stringIndexMap;
    }

    public void setStringIndexMap(Map<String, Integer> stringIndexMap) {
        this.stringIndexMap = stringIndexMap;
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

    //字符串长度最少占2个字节，最多占4个字节
    public static String resolveStringPoolEntry(byte[] buffer, Charset charSet) {
        String str = "";
        int len = 0;
        if (charSet.equals(StandardCharsets.UTF_8)) {
            len = buffer[0];
            if ((len & 0x80) != 0) {
                byte high = buffer[1];
                len = ((len & 0x7f) << 8) | high;
            }
            str = new String(buffer, 2, buffer.length - 2 - 1, charSet);
        } else {
            ByteBuffer byteBuffer = ByteBuffer.allocate(4);
            byteBuffer.order(ByteOrder.LITTLE_ENDIAN);
            byteBuffer.clear();
            byteBuffer.put(buffer, 0, 2);
            byteBuffer.flip();
            len = byteBuffer.getShort();
            if ((len & 0x8000) != 0) {
                short high = byteBuffer.getShort();
                len = ((len & 0x7fff) << 16) | high;
            }
            str = new String(buffer, byteBuffer.limit(), buffer.length - 4, charSet);
        }
        return str;
    }

    public static byte[] encodeStringPoolEntry(String str, Charset charSet) {
        byte[] content = str.getBytes(charSet);
        int len = str.length();
        ByteBuffer resultBuf;
        if (charSet.equals(StandardCharsets.UTF_8)) {
            resultBuf = ByteBuffer.allocate(content.length + 2 + 1);
            resultBuf.order(ByteOrder.LITTLE_ENDIAN);
            if (len > 0xFF) {
                resultBuf.put((byte) (((len & 0x7F00) >> 8) | 0x80));
                resultBuf.put((byte) (len & 0xFF));
            } else {
                resultBuf.put((byte) (len & 0xFF));
                resultBuf.put((byte) (len & 0xFF));
            }
        } else {
            if (len > 0xFFFF) {
                resultBuf = ByteBuffer.allocate(content.length + 4 + 2);
                resultBuf.order(ByteOrder.LITTLE_ENDIAN);
                resultBuf.putShort((short) (((len & 0x7FFF0000) >> 16) | 0x8000));
                resultBuf.putShort((short) (len & 0xFFFF));
            } else {
                resultBuf = ByteBuffer.allocate(content.length + 2 + 2);
                resultBuf.order(ByteOrder.LITTLE_ENDIAN);
                resultBuf.putShort((short) (len & 0xFFFF));
            }
        }
        resultBuf.put(content);
        resultBuf.rewind();
        return resultBuf.array();
    }

    public void refresh() {
        int oldChunkSize = chunkSize;
        chunkSize = 0;
        chunkSize += headSize;
        chunkSize += stringCount * 4;
        chunkSize += styleCount * 4;

        if (strings != null) {

            stringStart = headSize + styleCount * 4 + stringCount * 4;

            //regenerate stringIndexMap
            if (stringIndexMap != null) {
                stringIndexMap.clear();
                for (int i = 0; i < stringCount; i++) {
                    stringIndexMap.put(resolveStringPoolEntry(strings.get(i).array(), getCharSet()), i);
                }
            }

            stringOffsets.clear();
            if (stringCount > 0) {
                stringOffsets.add(0);
                for (int i = 1; i < stringCount; i++) {
                    stringOffsets.add(stringOffsets.get(i - 1) + strings.get(i - 1).limit());
                }
                if (styleCount > 0) {
                    styleStart = stringStart + stringOffsets.get(stringCount - 1) + strings.get(stringCount - 1).limit();
                }
                for (ByteBuffer buffer : strings) {
                    int strLen = buffer.limit();
                    chunkSize += strLen;
                }
            }
        }

        if (styles != null) {
            chunkSize += styles.length;
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
        byteBuffer.putInt(stringCount);
        byteBuffer.putInt(styleCount);
        byteBuffer.putInt(flag);
        byteBuffer.putInt(stringStart);
        byteBuffer.putInt(styleStart);
        if (headPadding > 0) {
            byteBuffer.put(new byte[headPadding]);
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
        if (chunkPadding > 0) {
            byteBuffer.put(new byte[chunkPadding]);
        }
        byteBuffer.flip();
        return byteBuffer.array();
    }

}
