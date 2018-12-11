package com.tencent.mm.arscutil.data;

/**
 * Created by williamjin on 18/7/29.
 */

public abstract class ResChunk {

    protected long start; // chunk开始位置

    protected short type; // 类型, 2 bytes
    protected short headSize; // 头大小, 2 bytes
    protected int chunkSize; // chunk大小, 4 bytes

    protected int headPaddingSize;    //头尾部padding大小，值补0
    protected int chunkPaddingSize;  //chunk尾部padding大小，值补0

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

    public int getHeadPaddingSize() {
        return headPaddingSize;
    }

    public void setHeadPaddingSize(int headPaddingSize) {
        this.headPaddingSize = headPaddingSize;
    }

    public int getChunkPaddingSize() {
        return chunkPaddingSize;
    }

    public void setChunkPaddingSize(int chunkPaddingSize) {
        this.chunkPaddingSize = chunkPaddingSize;
    }


    public abstract byte[] toBytes() throws Exception;

}
