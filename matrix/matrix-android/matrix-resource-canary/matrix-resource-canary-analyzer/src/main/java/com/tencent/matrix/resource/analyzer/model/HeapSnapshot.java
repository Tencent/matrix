/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.tencent.matrix.resource.analyzer.model;

import com.squareup.haha.perflib.HprofParser;
import com.squareup.haha.perflib.Snapshot;
import com.squareup.haha.perflib.io.HprofBuffer;
import com.squareup.haha.perflib.io.MemoryMappedFileBuffer;
import com.tencent.matrix.resource.analyzer.utils.AnalyzeUtil;

import java.io.File;
import java.io.IOException;

import static com.tencent.matrix.resource.common.utils.Preconditions.checkNotNull;


/**
 * Created by tangyinsheng on 2017/7/4.
 */

public class HeapSnapshot {

    private final File mHprofFile;
    private final Snapshot mSnapshot;

    public HeapSnapshot(File hprofFile) throws IOException {
        mHprofFile = checkNotNull(hprofFile, "hprofFile");
        mSnapshot = initSnapshot(hprofFile);
    }

    public File getHprofFile() {
        return mHprofFile;
    }

    public Snapshot getSnapshot() {
        return mSnapshot;
    }

    private static Snapshot initSnapshot(File hprofFile) throws IOException {
        final HprofBuffer buffer = new MemoryMappedFileBuffer(hprofFile);
        final HprofParser parser = new HprofParser(buffer);
        final Snapshot result = parser.parse();
        AnalyzeUtil.deduplicateGcRoots(result);
        return result;
    }
}
