/*
 * Copyright (C) 2015 Square, Inc.
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

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

/**
 * Created by tangyinsheng on 2017/6/2.
 *
 * This class is ported from LeakCanary.
 */

public final class ActivityLeakResult extends AnalyzeResult {

    public static ActivityLeakResult noLeak(long analysisDurationMs) {
        return new ActivityLeakResult(false, false, null, null, null, analysisDurationMs);
    }

    public static ActivityLeakResult leakDetected(boolean excludedLeak, String className,
                                                  ReferenceChain referenceChain, long analysisDurationMs) {
        return new ActivityLeakResult(true, excludedLeak, className, referenceChain, null, analysisDurationMs);
    }

    public static ActivityLeakResult failure(Throwable failure, long analysisDurationMs) {
        return new ActivityLeakResult(false, false, null, null, failure, analysisDurationMs);
    }

    /**
     * True if a leak was found in the heap dump.
     */
    public final boolean mLeakFound;

    /**
     * True if {@link #mLeakFound} is true and the only path to the leaking reference is
     * through excluded references. Usually, that means you can safely ignore this report.
     */
    public final boolean mExcludedLeak;

    /**
     * Class name of the object that leaked if {@link #mLeakFound} is true, null otherwise.
     * The class name format is the same as what would be returned by {@link Class#getName()}.
     */
    public final String mClassName;

    /**
     * Shortest path to GC roots for the leaking object if {@link #mLeakFound} is true, null
     * otherwise. This can be used as a unique signature for the leak.
     */
    public final ReferenceChain referenceChain;

    /**
     * Null unless the analysis failed.
     */
    public final Throwable mFailure;

    /**
     * Total time spent analyzing the heap.
     */
    public final long mAnalysisDurationMs;

    private ActivityLeakResult(boolean mLeakFound, boolean mExcludedLeak, String mClassName,
                               ReferenceChain referenceChain, Throwable mFailure, long mAnalysisDurationMs) {
        this.mLeakFound = mLeakFound;
        this.mExcludedLeak = mExcludedLeak;
        this.mClassName = mClassName;
        this.referenceChain = referenceChain;
        this.mFailure = mFailure;
        this.mAnalysisDurationMs = mAnalysisDurationMs;
    }

    @Override
    public void encodeToJSON(JSONObject jsonObject) throws JSONException {
        final JSONArray leakTraceJSONArray = new JSONArray();
        if (referenceChain != null) {
            for (ReferenceTraceElement element : referenceChain.elements) {
                leakTraceJSONArray.put(element.toString());
            }
        }
        jsonObject.put("leakFound", mLeakFound)
                  .put("excludedLeak", mExcludedLeak)
                  .put("className", mClassName)
                  .put("failure", String.valueOf(mFailure))
                  .put("analysisDurationMs", mAnalysisDurationMs)
                  .put("referenceChain", leakTraceJSONArray);
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder("Leak Reference:");
        if (referenceChain != null) {
            for (ReferenceTraceElement element : referenceChain.elements) {
                sb.append(element.toCollectableString()).append(";");
            }
        }

        return sb.toString();
    }
}
