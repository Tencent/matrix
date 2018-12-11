package com.tencent.mm.arscutil.io;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.OutputStream;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class LittleEndianOutputStream extends OutputStream {

    private RandomAccessFile original;


    public LittleEndianOutputStream(String file) throws FileNotFoundException {
        this(new RandomAccessFile(file, "rw"));
    }


    public LittleEndianOutputStream(RandomAccessFile original) {
        this.original = original;
    }

    @Override
    public void write(int b) throws IOException {
        original.write(b);
    }

    public void writeShort(short data) throws IOException {
        ByteBuffer byteBuffer = ByteBuffer.allocate(2);
        byteBuffer.clear();
        byteBuffer.order(ByteOrder.LITTLE_ENDIAN);
        byteBuffer.putShort(data);
        byteBuffer.flip();
        original.write(byteBuffer.array());
    }

    public void writeInt(int data) throws IOException {
        ByteBuffer byteBuffer = ByteBuffer.allocate(4);
        byteBuffer.clear();
        byteBuffer.order(ByteOrder.LITTLE_ENDIAN);
        byteBuffer.putInt(data);
        byteBuffer.flip();
        original.write(byteBuffer.array());
    }

    public void writeByte(byte data) throws IOException {
        original.write(data);
    }

    public void writeByte(byte[] buffer) throws IOException {
        original.write(buffer);
    }

    public void writeByte(byte[] buffer, int offset, int length) throws IOException {
        original.write(buffer, offset, length);
    }

    public void seek(long pos) throws IOException {
        original.seek(pos);
    }

    public long getFilePointer() throws IOException {
        return original.getFilePointer();
    }

    public long getFileLength() throws IOException {
        return original.length();
    }

    @Override
    public void close() throws IOException {
        super.close();
        original.close();
    }

}
