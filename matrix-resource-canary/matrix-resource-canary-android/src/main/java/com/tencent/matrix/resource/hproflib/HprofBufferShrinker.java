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

import com.tencent.matrix.resource.common.utils.DigestUtil;
import com.tencent.matrix.resource.hproflib.model.Field;
import com.tencent.matrix.resource.hproflib.model.ID;
import com.tencent.matrix.resource.hproflib.model.Type;
import com.tencent.matrix.resource.hproflib.utils.IOUtil;
import com.tencent.matrix.util.MatrixLog;
import com.tencent.matrix.util.MatrixUtil;
import com.tencent.tinker.ziputils.ziputil.TinkerZipEntry;
import com.tencent.tinker.ziputils.ziputil.TinkerZipFile;
import com.tencent.tinker.ziputils.ziputil.TinkerZipOutputStream;
import com.tencent.tinker.ziputils.ziputil.TinkerZipUtil;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Properties;
import java.util.Set;
import java.util.zip.CRC32;

/**
 * Created by tangyinsheng on 2017/6/29.
 */

public class HprofBufferShrinker {
    public static final String TAG = "Matrix.HprofBufferShrinker";

    private static final String PROPERTY_NAME = "extra.info";

    private final Set<ID>         mBmpBufferIds                   = new HashSet<>();
    private final Map<ID, byte[]> mBufferIdToElementDataMap       = new HashMap<>();
    private final Map<ID, ID>     mBmpBufferIdToDeduplicatedIdMap = new HashMap<>();
    private final Set<ID>         mStringValueIds                 = new HashSet<>();

    private ID mBitmapClassNameStringId    = null;
    private ID mBmpClassId                 = null;
    private ID mMBufferFieldNameStringId   = null;
    private ID mMRecycledFieldNameStringId = null;

    private ID mStringClassNameStringId = null;
    private ID mStringClassId           = null;
    private ID mValueFieldNameStringId  = null;

    private int     mIdSize                    = 0;
    private ID      mNullBufferId              = null;
    private Field[] mBmpClassInstanceFields    = null;
    private Field[] mStringClassInstanceFields = null;

    public static boolean addExtraInfo(File shrinkResultFile, Properties properties) {
        if (shrinkResultFile == null || !shrinkResultFile.exists()) {
            return false;
        }
        if (properties.isEmpty()) {
            return true;
        }
        long start = System.currentTimeMillis();
        OutputStream propertiesOutputStream = null;
        File propertiesFile = new File(shrinkResultFile.getParentFile(), PROPERTY_NAME);
        File tempFile = new File(shrinkResultFile.getAbsolutePath() + "_temp");

        try {
            propertiesOutputStream = new BufferedOutputStream(new FileOutputStream(propertiesFile, false));
            properties.store(propertiesOutputStream, null);
        } catch (Throwable throwable) {
            MatrixLog.e(TAG, "save property error:" + throwable);
            return false;
        } finally {
            MatrixUtil.closeQuietly(propertiesOutputStream);
        }

        TinkerZipOutputStream out = null;
        TinkerZipFile zipFile = null;
        try {

            out = new TinkerZipOutputStream(new BufferedOutputStream(new FileOutputStream(tempFile)));

            zipFile = new TinkerZipFile(shrinkResultFile);
            final Enumeration<? extends TinkerZipEntry> entries = zipFile.entries();

            while (entries.hasMoreElements()) {
                TinkerZipEntry zipEntry = entries.nextElement();
                if (zipEntry == null) {
                    throw new RuntimeException("zipEntry is null when get from oldApk");
                }
                String name = zipEntry.getName();
                if (name.contains("../")) {
                    continue;
                }
                TinkerZipUtil.extractTinkerEntry(zipFile, zipEntry, out);
            }
            Long crc = getCRC32(propertiesFile);
            if (crc == null) {
                MatrixLog.e(TAG, "new crc is null");
                return false;
            }
            TinkerZipEntry propertyEntry = new TinkerZipEntry(propertiesFile.getName());
            // add property file
            TinkerZipUtil.extractLargeModifyFile(propertyEntry, propertiesFile, crc, out);
        } catch (IOException e) {
            MatrixLog.e(TAG, "zip property error:" + e);
            return false;
        } finally {
            MatrixUtil.closeQuietly(zipFile);
            MatrixUtil.closeQuietly(out);
            propertiesFile.delete();
        }

        shrinkResultFile.delete();
        if (!tempFile.renameTo(shrinkResultFile)) {
            MatrixLog.e(TAG, "rename error");
            return false;
        }

        MatrixLog.i(TAG, "addExtraInfo end, path: %s, cost time: %d", shrinkResultFile.getAbsolutePath(), (System.currentTimeMillis() - start));
        return true;
    }

    private static Long getCRC32(File file) {
        CRC32 crc32 = new CRC32();
        // MessageDigest.get
        FileInputStream fileInputStream = null;
        try {
            fileInputStream = new FileInputStream(file);
            byte[] buffer = new byte[8192];
            int length;
            while ((length = fileInputStream.read(buffer)) != -1) {
                crc32.update(buffer, 0, length);
            }
            return crc32.getValue();
        } catch (IOException e) {
            return null;
        } finally {
            MatrixUtil.closeQuietly(fileInputStream);
        }
    }

    public void shrink(File hprofIn, File hprofOut) throws IOException {
        FileInputStream is = null;
        OutputStream os = null;
        try {
            is = new FileInputStream(hprofIn);
            os = new BufferedOutputStream(new FileOutputStream(hprofOut));
            final HprofReader reader = new HprofReader(new BufferedInputStream(is));
            reader.accept(new HprofInfoCollectVisitor());
            // Reset.
            is.getChannel().position(0);
            reader.accept(new HprofKeptBufferCollectVisitor());
            // Reset.
            is.getChannel().position(0);
            reader.accept(new HprofBufferShrinkVisitor(new HprofWriter(os)));
        } finally {
            if (os != null) {
                try {
                    os.close();
                } catch (Throwable thr) {
                    // Ignored.
                }
            }
            if (is != null) {
                try {
                    is.close();
                } catch (Throwable thr) {
                    // Ignored.
                }
            }
        }
    }

    private class HprofInfoCollectVisitor extends HprofVisitor {

        HprofInfoCollectVisitor() {
            super(null);
        }

        @Override
        public void visitHeader(String text, int idSize, long timestamp) {
            mIdSize = idSize;
            mNullBufferId = ID.createNullID(idSize);
        }

        @Override
        public void visitStringRecord(ID id, String text, int timestamp, long length) {
            if (mBitmapClassNameStringId == null && "android.graphics.Bitmap".equals(text)) {
                mBitmapClassNameStringId = id;
            } else if (mMBufferFieldNameStringId == null && "mBuffer".equals(text)) {
                mMBufferFieldNameStringId = id;
            } else if (mMRecycledFieldNameStringId == null && "mRecycled".equals(text)) {
                mMRecycledFieldNameStringId = id;
            } else if (mStringClassNameStringId == null && "java.lang.String".equals(text)) {
                mStringClassNameStringId = id;
            } else if (mValueFieldNameStringId == null && "value".equals(text)) {
                mValueFieldNameStringId = id;
            }
        }

        @Override
        public void visitLoadClassRecord(int serialNumber, ID classObjectId, int stackTraceSerial, ID classNameStringId, int timestamp, long length) {
            if (mBmpClassId == null && mBitmapClassNameStringId != null && mBitmapClassNameStringId.equals(classNameStringId)) {
                mBmpClassId = classObjectId;
            } else if (mStringClassId == null && mStringClassNameStringId != null && mStringClassNameStringId.equals(classNameStringId)) {
                mStringClassId = classObjectId;
            }
        }

        @Override
        public HprofHeapDumpVisitor visitHeapDumpRecord(int tag, int timestamp, long length) {
            return new HprofHeapDumpVisitor(null) {
                @Override
                public void visitHeapDumpClass(ID id, int stackSerialNumber, ID superClassId, ID classLoaderId, int instanceSize, Field[] staticFields, Field[] instanceFields) {
                    if (mBmpClassInstanceFields == null && mBmpClassId != null && mBmpClassId.equals(id)) {
                        mBmpClassInstanceFields = instanceFields;
                    } else if (mStringClassInstanceFields == null && mStringClassId != null && mStringClassId.equals(id)) {
                        mStringClassInstanceFields = instanceFields;
                    }
                }
            };
        }
    }

    private class HprofKeptBufferCollectVisitor extends HprofVisitor {

        HprofKeptBufferCollectVisitor() {
            super(null);
        }

        @Override
        public HprofHeapDumpVisitor visitHeapDumpRecord(int tag, int timestamp, long length) {
            return new HprofHeapDumpVisitor(null) {

                @Override
                public void visitHeapDumpInstance(ID id, int stackId, ID typeId, byte[] instanceData) {
                    try {
                        if (mBmpClassId != null && mBmpClassId.equals(typeId)) {
                            ID bufferId = null;
                            Boolean isRecycled = null;
                            final ByteArrayInputStream bais = new ByteArrayInputStream(instanceData);
                            for (Field field : mBmpClassInstanceFields) {
                                final ID fieldNameStringId = field.nameId;
                                final Type fieldType = Type.getType(field.typeId);
                                if (fieldType == null) {
                                    throw new IllegalStateException("visit bmp instance failed, lost type def of typeId: " + field.typeId);
                                }
                                if (mMBufferFieldNameStringId.equals(fieldNameStringId)) {
                                    bufferId = (ID) IOUtil.readValue(bais, fieldType, mIdSize);
                                } else if (mMRecycledFieldNameStringId.equals(fieldNameStringId)) {
                                    isRecycled = (Boolean) IOUtil.readValue(bais, fieldType, mIdSize);
                                } else if (bufferId == null || isRecycled == null) {
                                    IOUtil.skipValue(bais, fieldType, mIdSize);
                                } else {
                                    break;
                                }
                            }
                            bais.close();
                            final boolean reguardAsNotRecycledBmp = (isRecycled == null || !isRecycled);
                            if (bufferId != null && reguardAsNotRecycledBmp && !bufferId.equals(mNullBufferId)) {
                                mBmpBufferIds.add(bufferId);
                            }
                        } else if (mStringClassId != null && mStringClassId.equals(typeId)) {
                            ID strValueId = null;
                            final ByteArrayInputStream bais = new ByteArrayInputStream(instanceData);
                            for (Field field : mStringClassInstanceFields) {
                                final ID fieldNameStringId = field.nameId;
                                final Type fieldType = Type.getType(field.typeId);
                                if (fieldType == null) {
                                    throw new IllegalStateException("visit string instance failed, lost type def of typeId: " + field.typeId);
                                }
                                if (mValueFieldNameStringId.equals(fieldNameStringId)) {
                                    strValueId = (ID) IOUtil.readValue(bais, fieldType, mIdSize);
                                } else if (strValueId == null) {
                                    IOUtil.skipValue(bais, fieldType, mIdSize);
                                } else {
                                    break;
                                }
                            }
                            bais.close();
                            if (strValueId != null && !strValueId.equals(mNullBufferId)) {
                                mStringValueIds.add(strValueId);
                            }
                        }
                    } catch (Throwable thr) {
                        throw new RuntimeException(thr);
                    }
                }

                @Override
                public void visitHeapDumpPrimitiveArray(int tag, ID id, int stackId, int numElements, int typeId, byte[] elements) {
                    mBufferIdToElementDataMap.put(id, elements);
                }
            };
        }

        @Override
        public void visitEnd() {
            final Set<Map.Entry<ID, byte[]>> idDataSet = mBufferIdToElementDataMap.entrySet();
            final Map<String, ID> duplicateBufferFilterMap = new HashMap<>();
            for (Map.Entry<ID, byte[]> idDataPair : idDataSet) {
                final ID bufferId = idDataPair.getKey();
                final byte[] elementData = idDataPair.getValue();
                if (!mBmpBufferIds.contains(bufferId)) {
                    // Discard non-bitmap buffer.
                    continue;
                }
                final String buffMd5 = DigestUtil.getMD5String(elementData);
                final ID mergedBufferId = duplicateBufferFilterMap.get(buffMd5);
                if (mergedBufferId == null) {
                    duplicateBufferFilterMap.put(buffMd5, bufferId);
                } else {
                    mBmpBufferIdToDeduplicatedIdMap.put(mergedBufferId, mergedBufferId);
                    mBmpBufferIdToDeduplicatedIdMap.put(bufferId, mergedBufferId);
                }
            }
            // Save memory cost.
            mBufferIdToElementDataMap.clear();
        }
    }

    private class HprofBufferShrinkVisitor extends HprofVisitor {

        HprofBufferShrinkVisitor(HprofWriter hprofWriter) {
            super(hprofWriter);
        }

        @Override
        public HprofHeapDumpVisitor visitHeapDumpRecord(int tag, int timestamp, long length) {
            return new HprofHeapDumpVisitor(super.visitHeapDumpRecord(tag, timestamp, length)) {
                @Override
                public void visitHeapDumpInstance(ID id, int stackId, ID typeId, byte[] instanceData) {
                    try {
                        if (typeId.equals(mBmpClassId)) {
                            ID bufferId = null;
                            int bufferIdPos = 0;
                            final ByteArrayInputStream bais = new ByteArrayInputStream(instanceData);
                            for (Field field : mBmpClassInstanceFields) {
                                final ID fieldNameStringId = field.nameId;
                                final Type fieldType = Type.getType(field.typeId);
                                if (fieldType == null) {
                                    throw new IllegalStateException("visit instance failed, lost type def of typeId: " + field.typeId);
                                }
                                if (mMBufferFieldNameStringId.equals(fieldNameStringId)) {
                                    bufferId = (ID) IOUtil.readValue(bais, fieldType, mIdSize);
                                    break;
                                } else {
                                    bufferIdPos += IOUtil.skipValue(bais, fieldType, mIdSize);
                                }
                            }
                            if (bufferId != null) {
                                final ID deduplicatedId = mBmpBufferIdToDeduplicatedIdMap.get(bufferId);
                                if (deduplicatedId != null && !bufferId.equals(deduplicatedId) && !bufferId.equals(mNullBufferId)) {
                                    modifyIdInBuffer(instanceData, bufferIdPos, deduplicatedId);
                                }
                            }
                        }
                    } catch (Throwable thr) {
                        throw new RuntimeException(thr);
                    }
                    super.visitHeapDumpInstance(id, stackId, typeId, instanceData);
                }

                private void modifyIdInBuffer(byte[] buf, int off, ID newId) {
                    final ByteBuffer bBuf = ByteBuffer.wrap(buf);
                    bBuf.position(off);
                    bBuf.put(newId.getBytes());
                }

                @Override
                public void visitHeapDumpPrimitiveArray(int tag, ID id, int stackId, int numElements, int typeId, byte[] elements) {
                    final ID deduplicatedID = mBmpBufferIdToDeduplicatedIdMap.get(id);
                    // Discard non-bitmap or duplicated bitmap buffer but keep reference key.
                    if (deduplicatedID == null || !id.equals(deduplicatedID)) {
                        if (!mStringValueIds.contains(id)) {
                            return;
                        }
                    }
                    super.visitHeapDumpPrimitiveArray(tag, id, stackId, numElements, typeId, elements);
                }
            };
        }
    }
}
