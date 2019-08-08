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

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.OutputStream;

/**
 * Created by tangyinsheng on 2017/6/27.
 */

public class HprofWriter extends HprofVisitor {
    private final OutputStream mStreamOut;
    private int mIdSize = 0;

    private final ByteArrayOutputStream mHeapDumpOut = new ByteArrayOutputStream();

    public HprofWriter(OutputStream os) {
        super(null);
        mStreamOut = os;
    }

    @Override
    public void visitHeader(String text, int idSize, long timestamp) {
        try {
            if (idSize <= 0 || idSize >= (Integer.MAX_VALUE >> 1)) {
                throw new IOException("bad idSize: " + idSize);
            }
            mIdSize = idSize;
            IOUtil.writeNullTerminatedString(mStreamOut, text);
            IOUtil.writeBEInt(mStreamOut, idSize);
            IOUtil.writeBELong(mStreamOut, timestamp);
        } catch (Throwable thr) {
            throw new RuntimeException(thr);
        }
    }

    @Override
    public void visitStringRecord(ID id, String text, int timestamp, long length) {
        try {
            mStreamOut.write(HprofConstants.RECORD_TAG_STRING);
            IOUtil.writeBEInt(mStreamOut, timestamp);
            IOUtil.writeBEInt(mStreamOut, (int) length);
            mStreamOut.write(id.getBytes());
            IOUtil.writeString(mStreamOut, text);
        } catch (Throwable thr) {
            throw new RuntimeException(thr);
        }
    }

    @Override
    public void visitLoadClassRecord(int serialNumber, ID classObjectId, int stackTraceSerial, ID classNameStringId, int timestamp, long length) {
        try {
            mStreamOut.write(HprofConstants.RECORD_TAG_LOAD_CLASS);
            IOUtil.writeBEInt(mStreamOut, timestamp);
            IOUtil.writeBEInt(mStreamOut, (int) length);
            IOUtil.writeBEInt(mStreamOut, serialNumber);
            mStreamOut.write(classObjectId.getBytes());
            IOUtil.writeBEInt(mStreamOut, stackTraceSerial);
            mStreamOut.write(classNameStringId.getBytes());
        } catch (Throwable thr) {
            throw new RuntimeException(thr);
        }
    }

    @Override
    public void visitStackFrameRecord(ID id, ID methodNameId, ID methodSignatureId, ID sourceFileId, int serial, int lineNumber, int timestamp, long length) {
        try {
            mStreamOut.write(HprofConstants.RECORD_TAG_STACK_FRAME);
            IOUtil.writeBEInt(mStreamOut, timestamp);
            IOUtil.writeBEInt(mStreamOut, (int) length);
            mStreamOut.write(id.getBytes());
            mStreamOut.write(methodNameId.getBytes());
            mStreamOut.write(methodSignatureId.getBytes());
            mStreamOut.write(sourceFileId.getBytes());
            IOUtil.writeBEInt(mStreamOut, serial);
            IOUtil.writeBEInt(mStreamOut, lineNumber);
        } catch (Throwable thr) {
            throw new RuntimeException(thr);
        }
    }

    @Override
    public void visitStackTraceRecord(int serialNumber, int threadSerialNumber, ID[] frameIds, int timestamp, long length) {
        try {
            mStreamOut.write(HprofConstants.RECORD_TAG_STACK_TRACE);
            IOUtil.writeBEInt(mStreamOut, timestamp);
            IOUtil.writeBEInt(mStreamOut, (int) length);
            IOUtil.writeBEInt(mStreamOut, serialNumber);
            IOUtil.writeBEInt(mStreamOut, threadSerialNumber);
            IOUtil.writeBEInt(mStreamOut, frameIds.length);
            for (ID frameId : frameIds) {
                mStreamOut.write(frameId.getBytes());
            }
        } catch (Throwable thr) {
            throw new RuntimeException(thr);
        }
    }

    @Override
    public HprofHeapDumpWriter visitHeapDumpRecord(int tag, int timestamp, long length) {
        try {
            return new HprofHeapDumpWriter(tag, timestamp, length);
        } catch (Throwable thr) {
            throw new RuntimeException(thr);
        }
    }

    @Override
    public void visitUnconcernedRecord(int tag, int timestamp, long length, byte[] data) {
        try {
            mStreamOut.write(tag);
            IOUtil.writeBEInt(mStreamOut, timestamp);
            IOUtil.writeBEInt(mStreamOut, (int) length);
            mStreamOut.write(data, 0, (int) length);
        } catch (Throwable thr) {
            throw new RuntimeException(thr);
        }
    }

    @Override
    public void visitEnd() {
        try {
            mStreamOut.flush();
        } catch (Throwable thr) {
            throw new RuntimeException(thr);
        }
    }

    private class HprofHeapDumpWriter extends HprofHeapDumpVisitor {
        private final int mTag;
        private final int mTimestamp;
        private final long mOrigLength;

        HprofHeapDumpWriter(int tag, int timestamp, long length) {
            super(null);
            mTag = tag;
            mTimestamp = timestamp;
            mOrigLength = length;
        }

        @Override
        public void visitHeapDumpInfo(int heapId, ID heapNameId) {
            try {
                mHeapDumpOut.write(HprofConstants.HEAPDUMP_ROOT_HEAP_DUMP_INFO);
                IOUtil.writeBEInt(mHeapDumpOut, heapId);
                mHeapDumpOut.write(heapNameId.getBytes());
            } catch (Throwable thr) {
                throw new RuntimeException(thr);
            }
        }

        @Override
        public void visitHeapDumpBasicObj(int tag, ID id) {
            try {
                mHeapDumpOut.write(tag);
                mHeapDumpOut.write(id.getBytes());
                if (tag == HprofConstants.HEAPDUMP_ROOT_JNI_GLOBAL) {
                    IOUtil.skip(mHeapDumpOut, mIdSize);
                }
            } catch (Throwable thr) {
                throw new RuntimeException(thr);
            }
        }

        @Override
        public void visitHeapDumpJniLocal(ID id, int threadSerialNumber, int stackFrameNumber) {
            try {
                mHeapDumpOut.write(HprofConstants.HEAPDUMP_ROOT_JNI_LOCAL);
                mHeapDumpOut.write(id.getBytes());
                IOUtil.writeBEInt(mHeapDumpOut, threadSerialNumber);
                IOUtil.writeBEInt(mHeapDumpOut, stackFrameNumber);
            } catch (Throwable thr) {
                throw new RuntimeException(thr);
            }
        }

        @Override
        public void visitHeapDumpJavaFrame(ID id, int threadSerialNumber, int stackFrameNumber) {
            try {
                mHeapDumpOut.write(HprofConstants.HEAPDUMP_ROOT_JAVA_FRAME);
                mHeapDumpOut.write(id.getBytes());
                IOUtil.writeBEInt(mHeapDumpOut, threadSerialNumber);
                IOUtil.writeBEInt(mHeapDumpOut, stackFrameNumber);
            } catch (Throwable thr) {
                throw new RuntimeException(thr);
            }
        }

        @Override
        public void visitHeapDumpNativeStack(ID id, int threadSerialNumber) {
            try {
                mHeapDumpOut.write(HprofConstants.HEAPDUMP_ROOT_NATIVE_STACK);
                mHeapDumpOut.write(id.getBytes());
                IOUtil.writeBEInt(mHeapDumpOut, threadSerialNumber);
            } catch (Throwable thr) {
                throw new RuntimeException(thr);
            }
        }

        @Override
        public void visitHeapDumpThreadBlock(ID id, int threadSerialNumber) {
            try {
                mHeapDumpOut.write(HprofConstants.HEAPDUMP_ROOT_THREAD_BLOCK);
                mHeapDumpOut.write(id.getBytes());
                IOUtil.writeBEInt(mHeapDumpOut, threadSerialNumber);
            } catch (Throwable thr) {
                throw new RuntimeException(thr);
            }
        }

        @Override
        public void visitHeapDumpThreadObject(ID id, int threadSerialNumber, int stackFrameNumber) {
            try {
                mHeapDumpOut.write(HprofConstants.HEAPDUMP_ROOT_THREAD_OBJECT);
                mHeapDumpOut.write(id.getBytes());
                IOUtil.writeBEInt(mHeapDumpOut, threadSerialNumber);
                IOUtil.writeBEInt(mHeapDumpOut, stackFrameNumber);
            } catch (Throwable thr) {
                throw new RuntimeException(thr);
            }
        }

        @Override
        public void visitHeapDumpClass(ID id, int stackSerialNumber, ID superClassId, ID classLoaderId,
                                       int instanceSize, Field[] staticFields, Field[] instanceFields) {
            try {
                mHeapDumpOut.write(HprofConstants.HEAPDUMP_ROOT_CLASS_DUMP);
                mHeapDumpOut.write(id.getBytes());
                IOUtil.writeBEInt(mHeapDumpOut, stackSerialNumber);
                mHeapDumpOut.write(superClassId.getBytes());
                mHeapDumpOut.write(classLoaderId.getBytes());
                IOUtil.skip(mHeapDumpOut, mIdSize << 2);
                IOUtil.writeBEInt(mHeapDumpOut, instanceSize);

                // Write empty constant pool.
                IOUtil.writeBEShort(mHeapDumpOut, 0);

                // Write static fields.
                IOUtil.writeBEShort(mHeapDumpOut, staticFields.length);
                for (Field field : staticFields) {
                    IOUtil.writeID(mHeapDumpOut, field.nameId);
                    mHeapDumpOut.write(field.typeId);
                    IOUtil.writeValue(mHeapDumpOut, field.staticValue);
                }

                // Write instance fields.
                IOUtil.writeBEShort(mHeapDumpOut, instanceFields.length);
                for (Field field : instanceFields) {
                    IOUtil.writeID(mHeapDumpOut, field.nameId);
                    mHeapDumpOut.write(field.typeId);
                }
            } catch (Throwable thr) {
                throw new RuntimeException(thr);
            }
        }

        @Override
        public void visitHeapDumpInstance(ID id, int stackId, ID typeId, byte[] instanceData) {
            try {
                mHeapDumpOut.write(HprofConstants.HEAPDUMP_ROOT_INSTANCE_DUMP);
                mHeapDumpOut.write(id.getBytes());
                IOUtil.writeBEInt(mHeapDumpOut, stackId);
                mHeapDumpOut.write(typeId.getBytes());
                IOUtil.writeBEInt(mHeapDumpOut, instanceData.length);
                mHeapDumpOut.write(instanceData);
            } catch (Throwable thr) {
                throw new RuntimeException(thr);
            }
        }

        @Override
        public void visitHeapDumpObjectArray(ID id, int stackId, int numElements, ID typeId, byte[] elements) {
            try {
                mHeapDumpOut.write(HprofConstants.HEAPDUMP_ROOT_OBJECT_ARRAY_DUMP);
                mHeapDumpOut.write(id.getBytes());
                IOUtil.writeBEInt(mHeapDumpOut, stackId);
                IOUtil.writeBEInt(mHeapDumpOut, numElements);
                mHeapDumpOut.write(typeId.getBytes());
                final int remaining = numElements * mIdSize;
                mHeapDumpOut.write(elements, 0, remaining);
            } catch (Throwable thr) {
                throw new RuntimeException(thr);
            }
        }

        @Override
        public void visitHeapDumpPrimitiveArray(int tag, ID id, int stackId, int numElements, int typeId, byte[] elements) {
            try {
                mHeapDumpOut.write(tag);
                mHeapDumpOut.write(id.getBytes());
                IOUtil.writeBEInt(mHeapDumpOut, stackId);
                IOUtil.writeBEInt(mHeapDumpOut, numElements);
                mHeapDumpOut.write(typeId);
                final int remaining = numElements * Type.getType(typeId).getSize(mIdSize);
                mHeapDumpOut.write(elements, 0, remaining);
            } catch (Throwable thr) {
                throw new RuntimeException(thr);
            }
        }

        @Override
        public void visitHeapDumpJniMonitor(ID id, int threadSerialNumber, int stackDepth) {
            try {
                mHeapDumpOut.write(HprofConstants.HEAPDUMP_ROOT_JNI_MONITOR);
                mHeapDumpOut.write(id.getBytes());
                IOUtil.writeBEInt(mHeapDumpOut, threadSerialNumber);
                IOUtil.writeBEInt(mHeapDumpOut, stackDepth);
            } catch (Throwable thr) {
                throw new RuntimeException(thr);
            }
        }

        @Override
        public void visitEnd() {
            try {
                mStreamOut.write(mTag);
                IOUtil.writeBEInt(mStreamOut, mTimestamp);
                IOUtil.writeBEInt(mStreamOut, mHeapDumpOut.size());
                mStreamOut.write(mHeapDumpOut.toByteArray());
                mHeapDumpOut.reset();
            } catch (Throwable thr) {
                throw new RuntimeException(thr);
            }
        }
    }
}
