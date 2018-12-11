package com.tencent.mm.arscutil.data;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.List;

/**
 * Created by williamjin on 18/7/29.
 */

public class ResPackage extends ResChunk {

    private int id; // package id, 4 bytes
    private byte[] name; // package name, 256 bytes
    private int resTypePoolOffset; // 资源类型 stringPool offset, 4 bytes
    private int lastPublicType; // 4 bytes
    private int resNamePoolOffset; // 资源项名称 stringPool offset, 4 bytes
    private int lastPublicName; // 4 bytes
    private ResStringBlock resTypePool; // 资源类型 string pool
    private ResStringBlock resNamePool; // 资源项名称 string pool
    private List<ResChunk> resTypeArray; // 保存资源类型spec或者资源详细信息的数组

    public int getId() {
        return id;
    }

    public void setId(int id) {
        this.id = id;
    }

    public byte[] getName() {
        return name;
    }

    public void setName(byte[] name) {
        this.name = name;
    }

    public int getResTypePoolOffset() {
        return resTypePoolOffset;
    }

    public void setResTypePoolOffset(int resTypePoolOffset) {
        this.resTypePoolOffset = resTypePoolOffset;
    }

    public int getLastPublicType() {
        return lastPublicType;
    }

    public void setLastPublicType(int lastPublicType) {
        this.lastPublicType = lastPublicType;
    }

    public int getResNamePoolOffset() {
        return resNamePoolOffset;
    }

    public void setResNamePoolOffset(int resNamePoolOffset) {
        this.resNamePoolOffset = resNamePoolOffset;
    }

    public int getLastPublicName() {
        return lastPublicName;
    }

    public void setLastPublicName(int lastPublicName) {
        this.lastPublicName = lastPublicName;
    }

    public ResStringBlock getResTypePool() {
        return resTypePool;
    }

    public void setResTypePool(ResStringBlock resTypePool) {
        this.resTypePool = resTypePool;
    }

    public ResStringBlock getResNamePool() {
        return resNamePool;
    }

    public void setResNamePool(ResStringBlock resNamePool) {
        this.resNamePool = resNamePool;
    }

    public List<ResChunk> getResTypeArray() {
        return resTypeArray;
    }

    public void setResTypeArray(List<ResChunk> resTypes) {
        this.resTypeArray = resTypes;
    }

    public void refresh() {
        recomputeChunkSize();
    }

    private void recomputeChunkSize() {
        chunkSize = 0;
        chunkSize += headSize;
        if (resTypePool != null) {
            chunkSize += resTypePool.getChunkSize();
        }
        if (resNamePool != null) {
            chunkSize += resNamePool.getChunkSize();
        }
        if (resTypeArray != null) {
            for (ResChunk resType : resTypeArray) {
                if (resType != null) {
                    chunkSize += resType.getChunkSize();
                }
            }
        }
    }

    public byte[] toBytes() throws Exception {
        ByteBuffer byteBuffer = ByteBuffer.allocate(chunkSize);
        byteBuffer.order(ByteOrder.LITTLE_ENDIAN);
        byteBuffer.clear();
        byteBuffer.putShort(type);
        byteBuffer.putShort(headSize);
        byteBuffer.putInt(chunkSize);
        byteBuffer.putInt(id);
        byteBuffer.put(name);
        byteBuffer.putInt(resTypePoolOffset);
        byteBuffer.putInt(lastPublicType);
        byteBuffer.putInt(resNamePoolOffset);
        byteBuffer.putInt(lastPublicName);
        if (headPaddingSize > 0) {
            byteBuffer.put(new byte[headPaddingSize]);
        }
        if (resTypePool != null) {
            byteBuffer.put(resTypePool.toBytes());
        }
        if (resNamePool != null) {
            byteBuffer.put(resNamePool.toBytes());
        }
        if (resTypeArray != null && !resTypeArray.isEmpty()) {
            for (int i = 0; i < resTypeArray.size(); i++) {
                if (resTypeArray.get(i).chunkSize > 0) {
                    byteBuffer.put(resTypeArray.get(i).toBytes());
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
