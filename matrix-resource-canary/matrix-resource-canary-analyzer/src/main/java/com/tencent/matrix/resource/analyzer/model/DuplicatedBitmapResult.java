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

package com.tencent.matrix.resource.analyzer.model;

import com.tencent.matrix.resource.common.utils.DigestUtil;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.List;

/**
 * Created by tangyinsheng on 2017/6/6.
 */

public class DuplicatedBitmapResult extends AnalyzeResult {

    private final List<DuplicatedBitmapEntry> mDuplicatedBitmapEntries;
    private final long                        mAnalyzeDurationMs;
    private final Throwable                   mFailure;

    public static DuplicatedBitmapResult noDuplicatedBitmap(long analyzeDurationMs) {
        return new DuplicatedBitmapResult(Collections.<DuplicatedBitmapEntry>emptyList(), analyzeDurationMs, null);
    }

    public static DuplicatedBitmapResult failure(Throwable failure, long analyzeDurationMs) {
        return new DuplicatedBitmapResult(Collections.<DuplicatedBitmapEntry>emptyList(), analyzeDurationMs, failure);
    }

    public static DuplicatedBitmapResult duplicatedBitmapDetected(Collection<DuplicatedBitmapEntry> duplicatedBitmapEntries, long analyzeDurationMs) {
        return new DuplicatedBitmapResult(duplicatedBitmapEntries, analyzeDurationMs, null);
    }

    private DuplicatedBitmapResult(Collection<DuplicatedBitmapEntry> duplicatedBitmapEntries, long analyzeDurationMs, Throwable failure) {
        mDuplicatedBitmapEntries = Collections.unmodifiableList(new ArrayList<>(duplicatedBitmapEntries));
        mAnalyzeDurationMs = analyzeDurationMs;
        mFailure = failure;
    }

    public List<DuplicatedBitmapEntry> getDuplicatedBitmapEntries() {
        return mDuplicatedBitmapEntries;
    }

    @Override
    public void encodeToJSON(JSONObject jsonObject) throws JSONException {
        final JSONArray duplicatedBitmapEntriesJSONArr = new JSONArray();
        for (DuplicatedBitmapEntry entry : mDuplicatedBitmapEntries) {
            duplicatedBitmapEntriesJSONArr.put(entry.toJSONObject());
        }
        jsonObject.put("targetFound", !mDuplicatedBitmapEntries.isEmpty())
                  .put("analyzeDurationMs", mAnalyzeDurationMs)
                  .put("mFailure", String.valueOf(mFailure))
                  .put("duplicatedBitmapEntries", duplicatedBitmapEntriesJSONArr);
    }

    public static class DuplicatedBitmapEntry implements Serializable {
        private final String               mBufferHash;
        private final int                  mWidth;
        private final int                  mHeight;
        private final byte[]               mBuffer;
        private final List<ReferenceChain> mReferenceChains;

        public DuplicatedBitmapEntry(int width, int height, byte[] rawBuffer, Collection<ReferenceChain> referenceChains) {
            mBufferHash = DigestUtil.getMD5String(rawBuffer);
            mWidth = width;
            mHeight = height;
            mBuffer = rawBuffer;
            mReferenceChains = Collections.unmodifiableList(new ArrayList<>(referenceChains));
        }

        public String getBufferHash() {
            return mBufferHash;
        }

        public int getWidth() {
            return mWidth;
        }

        public int getHeight() {
            return mHeight;
        }

        public byte[] getBuffer() {
            return mBuffer;
        }

        public int getBufferSize() {
            return (mBuffer != null ? mBuffer.length : 0);
        }

        public JSONObject toJSONObject() throws JSONException {
            final JSONObject result = new JSONObject();
            final JSONArray referenceChainsJson = new JSONArray();
            for (ReferenceChain referenceChain : mReferenceChains) {
                final JSONArray referenceChainJson = new JSONArray();
                for (ReferenceTraceElement element : referenceChain.elements) {
                    referenceChainJson.put(element);
                }
                referenceChainsJson.put(referenceChainJson);
            }
            result.put("bufferHash", mBufferHash);
            result.put("width", mWidth);
            result.put("height", mHeight);
            result.put("bufferSize", getBufferSize());
            result.put("referenceChains", referenceChainsJson);
            return result;
        }
    }
}
