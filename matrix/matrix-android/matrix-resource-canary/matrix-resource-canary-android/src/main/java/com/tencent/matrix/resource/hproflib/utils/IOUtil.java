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

package com.tencent.matrix.resource.hproflib.utils;

import com.tencent.matrix.resource.hproflib.model.ID;
import com.tencent.matrix.resource.hproflib.model.Type;

import java.io.EOFException;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.Charset;

/**
 * Created by tangyinsheng on 2017/6/25.
 */

@SuppressWarnings("unused")
public final class IOUtil {

    public static short readLEShort(InputStream in) throws IOException {
        final int b0 = in.read();
        final int b1 = in.read();
        if ((b0 | b1) < 0) {
            throw new EOFException();
        }
        return (short) ((b1 << 8) | b0);
    }

    public static int readLEInt(InputStream in) throws IOException {
        int b0 = in.read();
        int b1 = in.read();
        int b2 = in.read();
        int b3 = in.read();
        if ((b0 | b1 | b2 | b3) < 0) {
            throw new EOFException();
        }
        return ((b3 << 24) + (b2 << 16) + (b1 << 8) + (b0));
    }

    public static long readLELong(InputStream in) throws IOException {
        final int len = 8;
        final byte[] buf = new byte[len];
        readFully(in, buf, 0, len);
        return (((long) buf[7] << 56)
            + ((long) (buf[6] & 0xFF) << 48)
            + ((long) (buf[5] & 0xFF) << 40)
            + ((long) (buf[4] & 0xFF) << 32)
            + ((long) (buf[3] & 0xFF) << 24)
            + ((buf[2] & 0xFF) << 16)
            + ((buf[1] & 0xFF) << 8)
            + ((buf[0] & 0xFF)));
    }

    public static short readBEShort(InputStream in) throws IOException {
        final int b0 = in.read();
        final int b1 = in.read();
        if ((b0 | b1) < 0) {
            throw new EOFException();
        }
        return (short) ((b0 << 8) | b1);
    }

    public static int readBEInt(InputStream in) throws IOException {
        int b0 = in.read();
        int b1 = in.read();
        int b2 = in.read();
        int b3 = in.read();
        if ((b0 | b1 | b2 | b3) < 0) {
            throw new EOFException();
        }
        return ((b0 << 24) + (b1 << 16) + (b2 << 8) + (b3));
    }

    public static long readBELong(InputStream in) throws IOException {
        final int len = 8;
        final byte[] buf = new byte[len];
        readFully(in, buf, 0, len);
        return (((long) buf[0] << 56)
            + ((long) (buf[1] & 0xFF) << 48)
            + ((long) (buf[2] & 0xFF) << 40)
            + ((long) (buf[3] & 0xFF) << 32)
            + ((long) (buf[4] & 0xFF) << 24)
            + ((buf[5] & 0xFF) << 16)
            + ((buf[6] & 0xFF) << 8)
            + ((buf[7] & 0xFF)));
    }

    public static void readFully(InputStream in, byte[] buf, int off, long length) throws IOException {
        int n = 0;
        while (n < length) {
            final int count = in.read(buf, n, (int) (length - n));
            if (count < 0) {
                throw new EOFException();
            }
            n += count;
        }
    }

    public static String readNullTerminatedString(InputStream in) throws IOException {
        final StringBuilder sb = new StringBuilder();
        for (int c = in.read(); c != 0; c = in.read()) {
            sb.append((char) c);
        }
        return sb.toString();
    }

    public static String readString(InputStream in, long length) throws IOException {
        final byte[] buf = new byte[(int) length];
        readFully(in, buf, 0, length);
        return new String(buf, Charset.forName("UTF-8"));
    }

    public static ID readID(InputStream in, int idSize) throws IOException {
        final byte[] idBytes = new byte[idSize];
        readFully(in, idBytes, 0, idSize);
        return new ID(idBytes);
    }

    public static Object readValue(InputStream in, Type type, int idSize) throws IOException {
        switch (type) {
            case OBJECT:
                return IOUtil.readID(in, idSize);
            case BOOLEAN:
                return (in.read() != 0);
            case CHAR:
                return (char) IOUtil.readBEShort(in);
            case FLOAT:
                return Float.intBitsToFloat(IOUtil.readBEInt(in));
            case DOUBLE:
                return Double.longBitsToDouble(IOUtil.readBELong(in));
            case BYTE:
                return (byte) in.read();
            case SHORT:
                return IOUtil.readBEShort(in);
            case INT:
                return IOUtil.readBEInt(in);
            case LONG:
                return IOUtil.readBELong(in);
            default:
                return null;
        }
    }

    public static void skip(InputStream in, long n) throws IOException {
        long skipped = 0;
        while (skipped < n) {
            long actualSkipped = in.skip(n - skipped);
            if (actualSkipped < 0) {
                throw new EOFException();
            }
            skipped += actualSkipped;
        }
    }

    public static int skipValue(InputStream in, Type type, int idSize) throws IOException {
        final int actualIdSize = type.getSize(idSize);
        IOUtil.skip(in, actualIdSize);
        return actualIdSize;
    }

    public static void writeLEShort(OutputStream out, int value) throws IOException {
        out.write((value) & 0xFF);
        out.write((value >>> 8) & 0xFF);
    }

    public static void writeLEInt(OutputStream out, int value) throws IOException {
        out.write((value) & 0xFF);
        out.write((value >>> 8) & 0xFF);
        out.write((value >>> 16) & 0xFF);
        out.write((value >>> 24) & 0xFF);
    }

    public static void writeLELong(OutputStream out, long value) throws IOException {
        final int len = 8;
        final byte[] buf = new byte[len];
        buf[7] = (byte) (value >>> 56);
        buf[6] = (byte) (value >>> 48);
        buf[5] = (byte) (value >>> 40);
        buf[4] = (byte) (value >>> 32);
        buf[3] = (byte) (value >>> 24);
        buf[2] = (byte) (value >>> 16);
        buf[1] = (byte) (value >>> 8);
        buf[0] = (byte) (value);
        out.write(buf, 0, len);
    }

    public static void writeBEShort(OutputStream out, int value) throws IOException {
        out.write((value >>> 8) & 0xFF);
        out.write((value) & 0xFF);
    }

    public static void writeBEInt(OutputStream out, int value) throws IOException {
        out.write((value >>> 24) & 0xFF);
        out.write((value >>> 16) & 0xFF);
        out.write((value >>> 8) & 0xFF);
        out.write((value) & 0xFF);
    }

    public static void writeBELong(OutputStream out, long value) throws IOException {
        final int len = 8;
        final byte[] buf = new byte[len];
        buf[0] = (byte) (value >>> 56);
        buf[1] = (byte) (value >>> 48);
        buf[2] = (byte) (value >>> 40);
        buf[3] = (byte) (value >>> 32);
        buf[4] = (byte) (value >>> 24);
        buf[5] = (byte) (value >>> 16);
        buf[6] = (byte) (value >>> 8);
        buf[7] = (byte) (value);
        out.write(buf, 0, len);
    }

    public static void writeString(OutputStream out, String text) throws IOException {
        final int length = text.length();
        out.write(text.getBytes(Charset.forName("UTF-8")), 0, length);
    }

    public static void writeNullTerminatedString(OutputStream out, String text) throws IOException {
        out.write(text.getBytes(Charset.forName("UTF-8")));
        out.write(0);
    }

    public static void writeID(OutputStream out, ID id) throws IOException {
        out.write(id.getBytes());
    }

    public static void writeValue(OutputStream out, Object value) throws IOException {
        if (value == null) {
            throw new IllegalArgumentException("value is null.");
        } else if (value instanceof ID) {
            IOUtil.writeID(out, (ID) value);
        } else if (value instanceof Boolean || boolean.class.isAssignableFrom(value.getClass())) {
            out.write((boolean) value ? 1 : 0);
        } else if (value instanceof Character || char.class.isAssignableFrom(value.getClass())) {
            IOUtil.writeBEShort(out, (char) value);
        } else if (value instanceof Float || float.class.isAssignableFrom(value.getClass())) {
            IOUtil.writeBEInt(out, Float.floatToRawIntBits((Float) value));
        } else if (value instanceof Double || double.class.isAssignableFrom(value.getClass())) {
            IOUtil.writeBELong(out, Double.doubleToRawLongBits((Double) value));
        } else if (value instanceof Byte || byte.class.isAssignableFrom(value.getClass())) {
            out.write((byte) value);
        } else if (value instanceof Short || short.class.isAssignableFrom(value.getClass())) {
            IOUtil.writeBEShort(out, (short) value);
        } else if (value instanceof Integer || int.class.isAssignableFrom(value.getClass())) {
            IOUtil.writeBEInt(out, (int) value);
        } else if (value instanceof Long || long.class.isAssignableFrom(value.getClass())) {
            IOUtil.writeBELong(out, (long) value);
        } else {
            throw new IllegalArgumentException("bad value type: " + value.getClass().getName());
        }
    }

    public static void skip(OutputStream out, long size) throws IOException {
        final byte[] emptyBuf = new byte[4096];
        for (int i = 0; i < (size >> 12); ++i) {
            out.write(emptyBuf);
        }
        out.write(emptyBuf, 0, (int) (size & ((1L << 12) - 1)));
    }

    private IOUtil() {
        throw new UnsupportedOperationException();
    }
}
