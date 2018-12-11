package com.tencent.mm.arscutil.data;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Created by williamjin on 18/7/29.
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
