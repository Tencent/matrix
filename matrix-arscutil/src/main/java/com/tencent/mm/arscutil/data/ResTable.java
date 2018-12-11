package com.tencent.mm.arscutil.data;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Created by williamjin on 18/7/29.
 */

public class ResTable extends ResChunk {

    private int packageCount; // 资源package数目, 4 bytes
    private ResStringBlock globalStringPool; // 全局资源池
    private ResPackage[] packages; // 资源package数组

    public int getPackageCount() {
        return packageCount;
    }

    public void setPackageCount(int packageCount) {
        this.packageCount = packageCount;
    }

    public ResStringBlock getGlobalStringPool() {
        return globalStringPool;
    }

    public void setGlobalStringPool(ResStringBlock globalStringPool) {
        this.globalStringPool = globalStringPool;
    }

    public ResPackage[] getPackages() {
        return packages;
    }

    public void setPackages(ResPackage[] packages) {
        this.packages = packages;
    }

    public void refresh() {
        recomputeChunkSize();
    }

    private void recomputeChunkSize() {
        chunkSize = 0;
        chunkSize += headSize;
        if (globalStringPool != null) {
            chunkSize += globalStringPool.getChunkSize();
        }
        if (packages != null) {
            for (ResPackage resPackage : packages) {
                chunkSize += resPackage.getChunkSize();
            }
        }
    }

    @Override
    public byte[] toBytes() throws Exception {
        ByteBuffer byteBuffer = ByteBuffer.allocate(chunkSize);
        byteBuffer.order(ByteOrder.LITTLE_ENDIAN);
        byteBuffer.clear();
        byteBuffer.putShort(type);
        byteBuffer.putShort(headSize);
        byteBuffer.putInt(chunkSize);
        byteBuffer.putInt(packageCount);
        if (headPaddingSize > 0) {
            byteBuffer.put(new byte[headPaddingSize]);
        }
        if (globalStringPool != null) {
            byteBuffer.put(globalStringPool.toBytes());
        }
        if (packages != null) {
            for (int i = 0; i < packages.length; i++) {
                byteBuffer.put(packages[i].toBytes());
            }
        }
        if (chunkPaddingSize > 0) {
            byteBuffer.put(new byte[chunkPaddingSize]);
        }
        byteBuffer.flip();

        return byteBuffer.array();
    }

}
