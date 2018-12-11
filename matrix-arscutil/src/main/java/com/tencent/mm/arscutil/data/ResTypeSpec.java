package com.tencent.mm.arscutil.data;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Created by williamjin on 18/7/29.
 */

public class ResTypeSpec extends ResChunk {

    private byte id; // id， 1 byte
    private byte reserved0; // 保留字段1，1 byte
    private short reserved1; // 保留字段2，2 bytes
    private int entryCount; // entry数目，4 bytes
    private byte[] configFlags;

    public byte getId() {
        return id;
    }

    public void setId(byte id) {
        this.id = id;
    }

    public byte getReserved0() {
        return reserved0;
    }

    public void setReserved0(byte reserved0) {
        this.reserved0 = reserved0;
    }

    public short getReserved1() {
        return reserved1;
    }

    public void setReserved1(short reserved1) {
        this.reserved1 = reserved1;
    }

    public int getEntryCount() {
        return entryCount;
    }

    public void setEntryCount(int entryCount) {
        this.entryCount = entryCount;
    }

    public byte[] getConfigFlags() {
        return configFlags;
    }

    public void setConfigFlags(byte[] configFlags) {
        this.configFlags = configFlags;
    }

    public byte[] toBytes() {
        ByteBuffer byteBuffer = ByteBuffer.allocate(chunkSize);
        byteBuffer.order(ByteOrder.LITTLE_ENDIAN);
        byteBuffer.clear();
        byteBuffer.putShort(type);
        byteBuffer.putShort(headSize);
        byteBuffer.putInt(chunkSize);
        byteBuffer.put(id);
        byteBuffer.put(reserved0);
        byteBuffer.putShort(reserved1);
        byteBuffer.putInt(entryCount);
        if (headPaddingSize > 0) {
            byteBuffer.put(new byte[headPaddingSize]);
        }
        if (configFlags != null) {
            byteBuffer.put(configFlags);
        }
        if (chunkPaddingSize > 0) {
            byteBuffer.put(new byte[chunkPaddingSize]);
        }
        byteBuffer.flip();
        return byteBuffer.array();
    }
}
