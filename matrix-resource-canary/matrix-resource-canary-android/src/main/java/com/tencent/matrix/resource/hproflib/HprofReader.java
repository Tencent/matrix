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

package com.tencent.matrix.resource.hproflib;

import com.tencent.matrix.resource.hproflib.model.Field;
import com.tencent.matrix.resource.hproflib.model.ID;
import com.tencent.matrix.resource.hproflib.model.Type;
import com.tencent.matrix.resource.hproflib.utils.IOUtil;

import java.io.EOFException;
import java.io.IOException;
import java.io.InputStream;

/**
 * Created by tangyinsheng on 2017/6/25.
 */

public class HprofReader {
    private final InputStream mStreamIn;
    private int mIdSize = 0;

    public HprofReader(InputStream in) {
        mStreamIn = in;
    }

    public void accept(HprofVisitor hv) throws IOException {
        acceptHeader(hv);
        acceptRecord(hv);
        hv.visitEnd();
    }

    private void acceptHeader(HprofVisitor hv) throws IOException {
        final String text = IOUtil.readNullTerminatedString(mStreamIn);
        final int idSize = IOUtil.readBEInt(mStreamIn);
        final long timestamp = IOUtil.readBELong(mStreamIn);
        mIdSize = idSize;
        hv.visitHeader(text, idSize, timestamp);
    }

    private void acceptRecord(HprofVisitor hv) throws IOException {
        try {
            while (true) {
                final int tag = mStreamIn.read();
                final int timestamp = IOUtil.readBEInt(mStreamIn);
                final long length = IOUtil.readBEInt(mStreamIn) & 0x00000000FFFFFFFFL;
                switch (tag) {
                    case HprofConstants.RECORD_TAG_STRING:
                        acceptStringRecord(timestamp, length, hv);
                        break;
                    case HprofConstants.RECORD_TAG_LOAD_CLASS:
                        acceptLoadClassRecord(timestamp, length, hv);
                        break;
                    case HprofConstants.RECORD_TAG_STACK_FRAME:
                        acceptStackFrameRecord(timestamp, length, hv);
                        break;
                    case HprofConstants.RECORD_TAG_STACK_TRACE:
                        acceptStackTraceRecord(timestamp, length, hv);
                        break;
                    case HprofConstants.RECORD_TAG_HEAP_DUMP:
                    case HprofConstants.RECORD_TAG_HEAP_DUMP_SEGMENT:
                        acceptHeapDumpRecord(tag, timestamp, length, hv);
                        break;
                    case HprofConstants.RECORD_TAG_ALLOC_SITES:
                    case HprofConstants.RECORD_TAG_HEAP_SUMMARY:
                    case HprofConstants.RECORD_TAG_START_THREAD:
                    case HprofConstants.RECORD_TAG_END_THREAD:
                    case HprofConstants.RECORD_TAG_HEAP_DUMP_END:
                    case HprofConstants.RECORD_TAG_CPU_SAMPLES:
                    case HprofConstants.RECORD_TAG_CONTROL_SETTINGS:
                    case HprofConstants.RECORD_TAG_UNLOAD_CLASS:
                    case HprofConstants.RECORD_TAG_UNKNOWN:
                    default:
                        acceptUnconcernedRecord(tag, timestamp, length, hv);
                        break;
                }
            }
        } catch (EOFException ignored) {
            // Ignored.
        }
    }

    private void acceptStringRecord(int timestamp, long length, HprofVisitor hv) throws IOException {
        final ID id = IOUtil.readID(mStreamIn, mIdSize);
        final String text = IOUtil.readString(mStreamIn, length - mIdSize);
        hv.visitStringRecord(id, text, timestamp, length);
    }

    private void acceptLoadClassRecord(int timestamp, long length, HprofVisitor hv) throws IOException {
        final int serialNumber = IOUtil.readBEInt(mStreamIn);
        final ID classObjectId = IOUtil.readID(mStreamIn, mIdSize);
        final int stackTraceSerial = IOUtil.readBEInt(mStreamIn);
        final ID classNameStringId = IOUtil.readID(mStreamIn, mIdSize);
        hv.visitLoadClassRecord(serialNumber, classObjectId, stackTraceSerial, classNameStringId, timestamp, length);
    }

    private void acceptStackFrameRecord(int timestamp, long length, HprofVisitor hv) throws IOException {
        final ID id = IOUtil.readID(mStreamIn, mIdSize);
        final ID methodNameId = IOUtil.readID(mStreamIn, mIdSize);
        final ID methodSignatureId = IOUtil.readID(mStreamIn, mIdSize);
        final ID sourceFileId = IOUtil.readID(mStreamIn, mIdSize);
        final int serial = IOUtil.readBEInt(mStreamIn);
        final int lineNumber = IOUtil.readBEInt(mStreamIn);
        hv.visitStackFrameRecord(id, methodNameId, methodSignatureId, sourceFileId, serial, lineNumber, timestamp, length);
    }

    private void acceptStackTraceRecord(int timestamp, long length, HprofVisitor hv) throws IOException {
        final int serialNumber = IOUtil.readBEInt(mStreamIn);
        final int threadSerialNumber = IOUtil.readBEInt(mStreamIn);
        final int numFrames = IOUtil.readBEInt(mStreamIn);
        final ID[] frameIds = new ID[numFrames];
        for (int i = 0; i < numFrames; ++i) {
            frameIds[i] = IOUtil.readID(mStreamIn, mIdSize);
        }
        hv.visitStackTraceRecord(serialNumber, threadSerialNumber, frameIds, timestamp, length);
    }

    private void acceptHeapDumpRecord(int tag, int timestamp, long length, HprofVisitor hv) throws IOException {
        final HprofHeapDumpVisitor hdv = hv.visitHeapDumpRecord(tag, timestamp, length);
        if (hdv == null) {
            IOUtil.skip(mStreamIn, length);
            return;
        }
        while (length > 0) {
            final int heapDumpTag = mStreamIn.read();
            --length;
            switch (heapDumpTag) {
                case HprofConstants.HEAPDUMP_ROOT_UNKNOWN:
                    hdv.visitHeapDumpBasicObj(heapDumpTag, IOUtil.readID(mStreamIn, mIdSize));
                    length -= mIdSize;
                    break;
                case HprofConstants.HEAPDUMP_ROOT_JNI_GLOBAL:
                    hdv.visitHeapDumpBasicObj(heapDumpTag, IOUtil.readID(mStreamIn, mIdSize));
                    IOUtil.skip(mStreamIn, mIdSize);   //  ignored
                    length -= (mIdSize << 1);
                    break;
                case HprofConstants.HEAPDUMP_ROOT_JNI_LOCAL:
                    length -= acceptJniLocal(hdv);
                    break;
                case HprofConstants.HEAPDUMP_ROOT_JAVA_FRAME:
                    length -= acceptJavaFrame(hdv);
                    break;
                case HprofConstants.HEAPDUMP_ROOT_NATIVE_STACK:
                    length -= acceptNativeStack(hdv);
                    break;
                case HprofConstants.HEAPDUMP_ROOT_STICKY_CLASS:
                    hdv.visitHeapDumpBasicObj(heapDumpTag, IOUtil.readID(mStreamIn, mIdSize));
                    length -= mIdSize;
                    break;
                case HprofConstants.HEAPDUMP_ROOT_THREAD_BLOCK:
                    length -= acceptThreadBlock(hdv);
                    break;
                case HprofConstants.HEAPDUMP_ROOT_MONITOR_USED:
                    hdv.visitHeapDumpBasicObj(heapDumpTag, IOUtil.readID(mStreamIn, mIdSize));
                    length -= mIdSize;
                    break;
                case HprofConstants.HEAPDUMP_ROOT_THREAD_OBJECT:
                    length -= acceptThreadObject(hdv);
                    break;
                case HprofConstants.HEAPDUMP_ROOT_CLASS_DUMP:
                    length -= acceptClassDump(hdv);
                    break;
                case HprofConstants.HEAPDUMP_ROOT_INSTANCE_DUMP:
                    length -= acceptInstanceDump(hdv);
                    break;
                case HprofConstants.HEAPDUMP_ROOT_OBJECT_ARRAY_DUMP:
                    length -= acceptObjectArrayDump(hdv);
                    break;
                case HprofConstants.HEAPDUMP_ROOT_PRIMITIVE_ARRAY_DUMP:
                    length -= acceptPrimitiveArrayDump(heapDumpTag, hdv);
                    break;
                case HprofConstants.HEAPDUMP_ROOT_PRIMITIVE_ARRAY_NODATA_DUMP:
                    length -= acceptPrimitiveArrayDump(heapDumpTag, hdv);
                    break;
                case HprofConstants.HEAPDUMP_ROOT_HEAP_DUMP_INFO:
                    length -= acceptHeapDumpInfo(hdv);
                    break;
                case HprofConstants.HEAPDUMP_ROOT_INTERNED_STRING:
                    hdv.visitHeapDumpBasicObj(heapDumpTag, IOUtil.readID(mStreamIn, mIdSize));
                    length -= mIdSize;
                    break;
                case HprofConstants.HEAPDUMP_ROOT_FINALIZING:
                    hdv.visitHeapDumpBasicObj(heapDumpTag, IOUtil.readID(mStreamIn, mIdSize));
                    length -= mIdSize;
                    break;
                case HprofConstants.HEAPDUMP_ROOT_DEBUGGER:
                    hdv.visitHeapDumpBasicObj(heapDumpTag, IOUtil.readID(mStreamIn, mIdSize));
                    length -= mIdSize;
                    break;
                case HprofConstants.HEAPDUMP_ROOT_REFERENCE_CLEANUP:
                    hdv.visitHeapDumpBasicObj(heapDumpTag, IOUtil.readID(mStreamIn, mIdSize));
                    length -= mIdSize;
                    break;
                case HprofConstants.HEAPDUMP_ROOT_VM_INTERNAL:
                    hdv.visitHeapDumpBasicObj(heapDumpTag, IOUtil.readID(mStreamIn, mIdSize));
                    length -= mIdSize;
                    break;
                case HprofConstants.HEAPDUMP_ROOT_JNI_MONITOR:
                    length -= acceptJniMonitor(hdv);
                    break;
                case HprofConstants.HEAPDUMP_ROOT_UNREACHABLE:
                    hdv.visitHeapDumpBasicObj(heapDumpTag, IOUtil.readID(mStreamIn, mIdSize));
                    length -= mIdSize;
                    break;
                default:
                    throw new IllegalArgumentException(
                            "acceptHeapDumpRecord loop with unknown tag " + heapDumpTag
                                    + " with " + mStreamIn.available()
                                    + " bytes possibly remaining");
            }
        }
        hdv.visitEnd();
    }

    private void acceptUnconcernedRecord(int tag, int timestamp, long length, HprofVisitor hv) throws IOException {
        final byte[] data = new byte[(int) length];
        IOUtil.readFully(mStreamIn, data, 0, length);
        hv.visitUnconcernedRecord(tag, timestamp, length, data);
    }

    private int acceptHeapDumpInfo(HprofHeapDumpVisitor hdv) throws IOException {
        final int heapId = IOUtil.readBEInt(mStreamIn);
        final ID heapNameId = IOUtil.readID(mStreamIn, mIdSize);
        hdv.visitHeapDumpInfo(heapId, heapNameId);
        return 4 + mIdSize;
    }

    private int acceptJniLocal(HprofHeapDumpVisitor hdv) throws IOException {
        final ID id = IOUtil.readID(mStreamIn, mIdSize);
        final int threadSerialNumber = IOUtil.readBEInt(mStreamIn);
        final int stackFrameNumber = IOUtil.readBEInt(mStreamIn);
        hdv.visitHeapDumpJniLocal(id, threadSerialNumber, stackFrameNumber);
        return mIdSize + 4 + 4;
    }

    private int acceptJavaFrame(HprofHeapDumpVisitor hdv) throws IOException {
        final ID id = IOUtil.readID(mStreamIn, mIdSize);
        final int threadSerialNumber = IOUtil.readBEInt(mStreamIn);
        final int stackFrameNumber = IOUtil.readBEInt(mStreamIn);
        hdv.visitHeapDumpJavaFrame(id, threadSerialNumber, stackFrameNumber);
        return mIdSize + 4 + 4;
    }

    private int acceptNativeStack(HprofHeapDumpVisitor hdv) throws IOException {
        final ID id = IOUtil.readID(mStreamIn, mIdSize);
        final int threadSerialNumber = IOUtil.readBEInt(mStreamIn);
        hdv.visitHeapDumpNativeStack(id, threadSerialNumber);
        return mIdSize + 4;
    }

    private int acceptThreadBlock(HprofHeapDumpVisitor hdv) throws IOException {
        final ID id = IOUtil.readID(mStreamIn, mIdSize);
        final int threadSerialNumber = IOUtil.readBEInt(mStreamIn);
        hdv.visitHeapDumpThreadBlock(id, threadSerialNumber);
        return mIdSize + 4;
    }

    private int acceptThreadObject(HprofHeapDumpVisitor hdv) throws IOException {
        final ID id = IOUtil.readID(mStreamIn, mIdSize);
        final int threadSerialNumber = IOUtil.readBEInt(mStreamIn);
        final int stackFrameNumber = IOUtil.readBEInt(mStreamIn);
        hdv.visitHeapDumpThreadObject(id, threadSerialNumber, stackFrameNumber);
        return mIdSize + 4 + 4;
    }

    private int acceptClassDump(HprofHeapDumpVisitor hdv) throws IOException {
        final ID id = IOUtil.readID(mStreamIn, mIdSize);
        final int stackSerialNumber = IOUtil.readBEInt(mStreamIn);
        final ID superClassId = IOUtil.readID(mStreamIn, mIdSize);
        final ID classLoaderId = IOUtil.readID(mStreamIn, mIdSize);
        IOUtil.skip(mStreamIn, (mIdSize << 2));
        final int instanceSize = IOUtil.readBEInt(mStreamIn);

        int bytesRead = (7 * mIdSize) + 4 + 4;

        //  Skip over the constant pool
        int numEntries = IOUtil.readBEShort(mStreamIn);
        bytesRead += 2;
        for (int i = 0; i < numEntries; ++i) {
            IOUtil.skip(mStreamIn, 2);
            bytesRead += 2 + skipValue();
        }

        //  Static fields
        numEntries = IOUtil.readBEShort(mStreamIn);
        Field[] staticFields = new Field[numEntries];
        bytesRead += 2;
        for (int i = 0; i < numEntries; ++i) {
            final ID nameId = IOUtil.readID(mStreamIn, mIdSize);
            final int typeId = mStreamIn.read();
            final Type type = Type.getType(typeId);
            if (type == null) {
                throw new IllegalStateException("accept class failed, lost type def of typeId: " + typeId);
            }
            final Object staticValue = IOUtil.readValue(mStreamIn, type, mIdSize);
            staticFields[i] = new Field(typeId, nameId, staticValue);
            bytesRead += mIdSize + 1 + type.getSize(mIdSize);
        }

        //  Instance fields
        numEntries = IOUtil.readBEShort(mStreamIn);
        final Field[] instanceFields = new Field[numEntries];
        bytesRead += 2;
        for (int i = 0; i < numEntries; i++) {
            final ID nameId = IOUtil.readID(mStreamIn, mIdSize);
            final int typeId = mStreamIn.read();
            instanceFields[i] = new Field(typeId, nameId, null);
            bytesRead += mIdSize + 1;
        }

        hdv.visitHeapDumpClass(id, stackSerialNumber, superClassId, classLoaderId, instanceSize, staticFields, instanceFields);

        return bytesRead;
    }

    private int acceptInstanceDump(HprofHeapDumpVisitor hdv) throws IOException {
        final ID id = IOUtil.readID(mStreamIn, mIdSize);
        final int stackId = IOUtil.readBEInt(mStreamIn);
        final ID typeId = IOUtil.readID(mStreamIn, mIdSize);
        final int remaining = IOUtil.readBEInt(mStreamIn);
        final byte[] instanceData = new byte[remaining];
        IOUtil.readFully(mStreamIn, instanceData, 0, remaining);
        hdv.visitHeapDumpInstance(id, stackId, typeId, instanceData);
        return mIdSize + 4 + mIdSize + 4 + remaining;
    }

    private int acceptObjectArrayDump(HprofHeapDumpVisitor hdv) throws IOException {
        final ID id = IOUtil.readID(mStreamIn, mIdSize);
        final int stackId = IOUtil.readBEInt(mStreamIn);
        final int numElements = IOUtil.readBEInt(mStreamIn);
        final ID typeId = IOUtil.readID(mStreamIn, mIdSize);
        final int remaining = numElements * mIdSize;
        final byte[] elements = new byte[remaining];
        IOUtil.readFully(mStreamIn, elements, 0, remaining);
        hdv.visitHeapDumpObjectArray(id, stackId, numElements, typeId, elements);
        return mIdSize + 4 + 4 + mIdSize + remaining;
    }

    private int acceptPrimitiveArrayDump(int tag, HprofHeapDumpVisitor hdv) throws IOException {
        final ID id = IOUtil.readID(mStreamIn, mIdSize);
        final int stackId = IOUtil.readBEInt(mStreamIn);
        final int numElements = IOUtil.readBEInt(mStreamIn);
        final int typeId = mStreamIn.read();
        final Type type = Type.getType(typeId);
        if (type == null) {
            throw new IllegalStateException("accept primitive array failed, lost type def of typeId: " + typeId);
        }
        final int remaining = numElements * type.getSize(mIdSize);
        final byte[] elements = new byte[remaining];
        IOUtil.readFully(mStreamIn, elements, 0, remaining);
        hdv.visitHeapDumpPrimitiveArray(tag, id, stackId, numElements, typeId, elements);
        return mIdSize + 4 + 4 + 1 + remaining;
    }

    private int acceptJniMonitor(HprofHeapDumpVisitor hdv) throws IOException {
        final ID id = IOUtil.readID(mStreamIn, mIdSize);
        final int threadSerialNumber = IOUtil.readBEInt(mStreamIn);
        final int stackDepth = IOUtil.readBEInt(mStreamIn);
        hdv.visitHeapDumpJniMonitor(id, threadSerialNumber, stackDepth);
        return mIdSize + 4 + 4;
    }

    private int skipValue() throws IOException {
        final int typeId = mStreamIn.read();
        final Type type = Type.getType(typeId);
        if (type == null) {
            throw new IllegalStateException("failure to skip type, cannot find type def of typeid: " + typeId);
        }
        final int size = type.getSize(mIdSize);
        IOUtil.skip(mStreamIn, size);
        return size + 1;
    }
}
