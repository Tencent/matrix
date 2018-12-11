package com.tencent.mm.arscutil.data;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.List;

/**
 * Created by williamjin on 18/7/29.
 */

public class ResType extends ResChunk {

    private static final String TAG = "ArscUtil.ResType";

    private byte id; // id，1 byte
    private byte reserved0; // 保留字段1，1 byte
    private short reserved1; // 保留字段2，2 bytes
    private int entryCount; // entry数目，4 bytes
    private int entryTableOffset; // entryTable的起始位置，4 bytes
    private ResConfig resConfigFlags; // configFlag
    private List<Integer> entryOffsets; // entry 偏移数组，给出每个entry的偏移位置, 0xFFFF表示NO_ENTRY
    private List<ResEntry> entryTable; // entry table

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

    public int getEntryTableOffset() {
        return entryTableOffset;
    }

    public void setEntryTableOffset(int entryTableOffset) {
        this.entryTableOffset = entryTableOffset;
    }

    public ResConfig getResConfigFlags() {
        return resConfigFlags;
    }

    public void setResConfigFlags(ResConfig resConfigFlags) {
        this.resConfigFlags = resConfigFlags;
    }

    public List<Integer> getEntryOffsets() {
        return entryOffsets;
    }

    public void setEntryOffsets(List<Integer> entryOffsets) {
        this.entryOffsets = entryOffsets;
    }

    public List<ResEntry> getEntryTable() {
        return entryTable;
    }

    public void setEntryTable(List<ResEntry> entryTable) {
        this.entryTable = entryTable;
    }

    public void refresh() throws IOException {
        //校正entryOffsets
        int lastOffset = 0;
        for (int i = 0; i < entryCount; i++) {
            if (entryOffsets.get(i) != ArscConstants.NO_ENTRY_INDEX) {
                entryOffsets.set(i, lastOffset);
                lastOffset += entryTable.get(i).toBytes().length;
            }
        }
        recomputeChunkSize();
    }

    private void recomputeChunkSize() throws IOException {
        chunkSize = 0;
        chunkSize += headSize;
        int realEntryCount = 0;
        if (entryOffsets != null) {
            chunkSize += entryOffsets.size() * 4;
        }
        if (entryTable != null) {
            for (ResEntry entry : entryTable) {
                if (entry != null) {
                    realEntryCount++;
                    chunkSize += entry.toBytes().length;            //这里不要使用entry.size，ResEntry后面还紧跟了ResValue或者ResMapValues
                }
            }
        }
        if (realEntryCount == 0) {                    //no entry
            entryCount = 0;
            chunkSize = 0;
        }
    }

    @Override
    public byte[] toBytes() throws IOException {
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
        byteBuffer.putInt(entryTableOffset);
        if (resConfigFlags != null) {
            byteBuffer.put(resConfigFlags.toBytes());
        }
        if (headPaddingSize > 0) {
            byteBuffer.put(new byte[headPaddingSize]);
        }
        if (entryOffsets != null) {
            for (int i = 0; i < entryOffsets.size(); i++) {
                byteBuffer.putInt(entryOffsets.get(i));
            }
        }
        if (entryTable != null) {
            for (int i = 0; i < entryTable.size(); i++) {
                if (entryTable.get(i) != null) {
                    byteBuffer.put(entryTable.get(i).toBytes());
                } else {
                    //NO_ENTRY
                }
            }
        }
        if (chunkPaddingSize > 0) {
            byteBuffer.put(new byte[chunkPaddingSize]);
        }
        byteBuffer.flip();
        return byteBuffer.array();
    }

}
