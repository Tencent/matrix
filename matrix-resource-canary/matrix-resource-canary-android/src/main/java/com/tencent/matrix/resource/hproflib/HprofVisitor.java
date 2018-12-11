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

import com.tencent.matrix.resource.hproflib.model.ID;

/**
 * Created by tangyinsheng on 2017/6/25.
 */

@SuppressWarnings("unused")
public class HprofVisitor {
    protected HprofVisitor hv = null;

    public HprofVisitor(HprofVisitor hv) {
        this.hv = hv;
    }

    public void visitHeader(String text, int idSize, long timestamp) {
        if (this.hv != null) {
            this.hv.visitHeader(text, idSize, timestamp);
        }
    }

    public void visitStringRecord(ID id, String text, int timestamp, long length) {
        if (this.hv != null) {
            this.hv.visitStringRecord(id, text, timestamp, length);
        }
    }

    public void visitLoadClassRecord(int serialNumber, ID classObjectId, int stackTraceSerial, ID classNameStringId, int timestamp, long length) {
        if (this.hv != null) {
            this.hv.visitLoadClassRecord(serialNumber, classObjectId, stackTraceSerial, classNameStringId, timestamp, length);
        }
    }

    public void visitStackFrameRecord(ID id, ID methodNameId, ID methodSignatureId, ID sourceFileId, int serial, int lineNumber, int timestamp, long length) {
        if (this.hv != null) {
            this.hv.visitStackFrameRecord(id, methodNameId, methodSignatureId, sourceFileId, serial, lineNumber, timestamp, length);
        }
    }

    public void visitStackTraceRecord(int serialNumber, int threadSerialNumber, ID[] frameIds, int timestamp, long length) {
        if (this.hv != null) {
            this.hv.visitStackTraceRecord(serialNumber, threadSerialNumber, frameIds, timestamp, length);
        }
    }

    public HprofHeapDumpVisitor visitHeapDumpRecord(int tag, int timestamp, long length) {
        if (this.hv != null) {
            return this.hv.visitHeapDumpRecord(tag, timestamp, length);
        } else {
            return null;
        }
    }

    public void visitUnconcernedRecord(int tag, int timestamp, long length, byte[] data) {
        if (this.hv != null) {
            this.hv.visitUnconcernedRecord(tag, timestamp, length, data);
        }
    }

    public void visitEnd() {
        if (this.hv != null) {
            this.hv.visitEnd();
        }
    }
}
