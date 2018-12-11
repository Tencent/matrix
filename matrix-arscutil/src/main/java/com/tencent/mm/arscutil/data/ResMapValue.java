package com.tencent.mm.arscutil.data;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

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
