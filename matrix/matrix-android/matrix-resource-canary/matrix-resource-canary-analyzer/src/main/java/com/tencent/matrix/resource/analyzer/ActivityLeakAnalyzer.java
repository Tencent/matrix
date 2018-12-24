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

package com.tencent.matrix.resource.analyzer;

import com.squareup.haha.perflib.ClassInstance;
import com.squareup.haha.perflib.ClassObj;
import com.squareup.haha.perflib.Instance;
import com.squareup.haha.perflib.Snapshot;
import com.tencent.matrix.resource.analyzer.model.ActivityLeakResult;
import com.tencent.matrix.resource.analyzer.model.ExcludedRefs;
import com.tencent.matrix.resource.analyzer.model.HeapSnapshot;
import com.tencent.matrix.resource.analyzer.model.ReferenceChain;
import com.tencent.matrix.resource.analyzer.utils.AnalyzeUtil;
import com.tencent.matrix.resource.analyzer.utils.ShortestPathFinder;

import java.util.ArrayList;
import java.util.List;

import static com.squareup.haha.perflib.HahaHelper.asString;
import static com.squareup.haha.perflib.HahaHelper.classInstanceValues;
import static com.squareup.haha.perflib.HahaHelper.fieldValue;

/**
 * Created by tangyinsheng on 2017/6/2.
 *
 * This class is ported from LeakCanary.
 */

public class ActivityLeakAnalyzer implements HeapSnapshotAnalyzer<ActivityLeakResult> {
    private static final String DESTROYED_ACTIVITY_INFO_CLASSNAME
            = "com.tencent.matrix.resource.analyzer.model.DestroyedActivityInfo";
    private static final String ACTIVITY_REFERENCE_KEY_FIELDNAME = "mKey";
    private static final String ACTIVITY_REFERENCE_FIELDNAME = "mActivityRef";

    private final String mRefKey;
    private final ExcludedRefs mExcludedRefs;

    public ActivityLeakAnalyzer(String refKey, ExcludedRefs excludedRefs) {
        mRefKey = refKey;
        mExcludedRefs = excludedRefs;
    }

    @Override
    public ActivityLeakResult analyze(HeapSnapshot heapSnapshot) {
        return checkForLeak(heapSnapshot, mRefKey);
    }

    /**
     * Searches the heap dump for a <code>DestroyedActivityInfo</code> instance with the corresponding key,
     * and then computes the shortest strong reference path from the leaked activity that instance holds
     * to the GC roots.
     */
    private ActivityLeakResult checkForLeak(HeapSnapshot heapSnapshot, String refKey) {
        long analysisStartNanoTime = System.nanoTime();

        try {
            final Snapshot snapshot = heapSnapshot.getSnapshot();
            final Instance leakingRef = findLeakingReference(refKey, snapshot);

            // False alarm, weak reference was cleared in between key check and heap dump.
            if (leakingRef == null) {
                return ActivityLeakResult.noLeak(AnalyzeUtil.since(analysisStartNanoTime));
            }

            return findLeakTrace(analysisStartNanoTime, snapshot, leakingRef);
        } catch (Throwable e) {
            e.printStackTrace();
            return ActivityLeakResult.failure(e, AnalyzeUtil.since(analysisStartNanoTime));
        }
    }

    private Instance findLeakingReference(String key, Snapshot snapshot) {
        final ClassObj infoClass = snapshot.findClass(DESTROYED_ACTIVITY_INFO_CLASSNAME);
        if (infoClass == null) {
            throw new IllegalStateException("Unabled to find destroy activity info class with name: "
                    + DESTROYED_ACTIVITY_INFO_CLASSNAME);
        }
        List<String> keysFound = new ArrayList<>();
        for (Instance infoInstance : infoClass.getInstancesList()) {
            final List<ClassInstance.FieldValue> values = classInstanceValues(infoInstance);
            final String keyCandidate = asString(fieldValue(values, ACTIVITY_REFERENCE_KEY_FIELDNAME));
            if (keyCandidate.equals(key)) {
                final Instance weakRefObj = fieldValue(values, ACTIVITY_REFERENCE_FIELDNAME);
                if (weakRefObj == null) {
                    continue;
                }
                final List<ClassInstance.FieldValue> activityRefs = classInstanceValues(weakRefObj);
                return fieldValue(activityRefs, "referent");
            }
            keysFound.add(keyCandidate);
        }
        throw new IllegalStateException(
                "Could not find weak reference with key " + key + " in " + keysFound);
    }

    private ActivityLeakResult findLeakTrace(long analysisStartNanoTime, Snapshot snapshot,
                                         Instance leakingRef) {

        ShortestPathFinder pathFinder = new ShortestPathFinder(mExcludedRefs);
        ShortestPathFinder.Result result = pathFinder.findPath(snapshot, leakingRef);

        // False alarm, no strong reference path to GC Roots.
        if (result.referenceChainHead == null) {
            return ActivityLeakResult.noLeak(AnalyzeUtil.since(analysisStartNanoTime));
        }

        final ReferenceChain referenceChain = result.buildReferenceChain();
        final String className = leakingRef.getClassObj().getClassName();
        if (result.excludingKnown || referenceChain.isEmpty()) {
            return ActivityLeakResult.noLeak(AnalyzeUtil.since(analysisStartNanoTime));
        } else {
            return ActivityLeakResult.leakDetected(false, className, referenceChain,
                    AnalyzeUtil.since(analysisStartNanoTime));
        }
    }
}
