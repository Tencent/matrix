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

package com.tencent.matrix.batterycanary.detector;

import android.os.Environment;
import android.os.IBinder;
import android.os.SystemClock;
import android.os.WorkSource;

import com.tencent.matrix.batterycanary.detector.config.BatteryConfig;
import com.tencent.matrix.batterycanary.detector.config.SharePluginInfo;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryDetectScheduler;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.report.Issue;
import com.tencent.matrix.report.IssuePublisher;
//import com.tencent.matrix.util.DeviceUtil;
import com.tencent.matrix.util.MatrixLog;
import com.tencent.matrix.util.MatrixUtil;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.Vector;

/**
 *  Checks for issues with wakelocks which can lead to too much battery usage:
 *  1. A wakelock is not still not released after a long period {@link #mWakeLockOnceHoldTimeThreshold}
 *    since its latest acquire
 *    @see #detectWakeLockOnceHoldTime()
 *  2. A wakelock of a same tag, may acquire and release many times.There's a aggregation.
 *    It should not acquire too often {@link #mWakeLockAcquireCnt1HThreshold}
 *    And it should not hold too long {@link #mWakeLockHoldTime1HThreshold}
 *    @see #detectWakeLockAggregation()
 *    It's noteworthy that there is a hypothesis that a wakelock of a certain usage scenario has a unqiue tag.
 *
 *  There is also wakelock info records to help analyzing the usage {@link WakeLockInfoRecorder}
 *
 * @author liyongjie
 *         Created by liyongjie on 2017/7/25.
 */

public class WakeLockDetector extends IssuePublisher {
    private static final String TAG = "Matrix.detector.WakeLock";

    public interface IDelegate {
        void addDetectTask(Runnable detectTask, long delayInMillis);

        boolean isScreenOn();
    }

    private final Map<String, WakeLockInfo> mWakeLockInfoMap = new HashMap<>();
    private final Map<String, WakeLockAggregation> mWakeLockSummaryMap = new HashMap<>();
    private final int mWakeLockOnceHoldTimeThreshold;
    private final int mWakeLockHoldTime1HThreshold;
    private final int mWakeLockAcquireCnt1HThreshold;
    private final IDelegate mDelegate;
    private final Runnable mDetectTask;
    private final WakeLockInfoRecorder mWakeLockRecorder;

    public WakeLockDetector(OnIssueDetectListener issueDetectListener,
                            BatteryConfig config, IDelegate delegate) {
        super(issueDetectListener);
        mWakeLockOnceHoldTimeThreshold = config.getWakeLockHoldTimeThreshold();
        mWakeLockHoldTime1HThreshold = config.getWakeLockHoldTime1HThreshold();
        mWakeLockAcquireCnt1HThreshold = config.getWakeLockAcquireCnt1HThreshold();
        mDelegate = delegate;

        mDetectTask = new Runnable() {
            @Override
            public void run() {
                calWakeLockAggregation();
                detect();
            }
        };

        if (config.isRecordWakeLock()) {
            mWakeLockRecorder = new WakeLockInfoRecorder();
        } else {
            mWakeLockRecorder = null;
        }
    }

    /**
     * Run in {@link BatteryCanaryDetectScheduler}  single thread
     * @see BatteryCanaryCore
     */
    public void onAcquireWakeLock(IBinder token, int flags, String tag, String packageName,
                                  WorkSource workSource, String historyTag, String stackTrace, final long acquireTime) {
        MatrixLog.i(TAG, "onAcquireWakeLock token:%s tag:%s", token, tag);

        if (mWakeLockRecorder != null) {
            mWakeLockRecorder.onAcquireWakeLock(token.toString(), flags, tag, stackTrace, acquireTime);
        }

        String tokenId = token.toString();
        WakeLockInfo wakeLockInfo;
        if (!mWakeLockInfoMap.containsKey(tokenId)) {
            wakeLockInfo = new WakeLockInfo(tokenId, tag, flags, acquireTime);
            mWakeLockInfoMap.put(tokenId, wakeLockInfo);
        } else {
            wakeLockInfo = mWakeLockInfoMap.get(tokenId);
        }
        wakeLockInfo.stackTraceHistory.recordStackTrace(stackTrace);

        if (!mWakeLockSummaryMap.containsKey(tag)) {
            mWakeLockSummaryMap.put(tag, new WakeLockAggregation(tag));
        }
        WakeLockAggregation wakeLockAggregation = mWakeLockSummaryMap.get(tag);
        wakeLockAggregation.whenAcquire(tokenId, mDelegate.isScreenOn());
        wakeLockAggregation.stackTraceHistory.recordStackTrace(stackTrace);

        mDelegate.addDetectTask(mDetectTask, mWakeLockOnceHoldTimeThreshold);
    }

    /**
     * Run in {@link BatteryCanaryDetectScheduler}  single thread
     * @see BatteryCanaryCore
     * @param token
     * @param flags
     */
    public void onReleaseWakeLock(IBinder token, int flags, long releaseTime) {
        MatrixLog.i(TAG, "onReleaseWakeLock token:%s", token);
        if (mWakeLockRecorder != null) {
            mWakeLockRecorder.onReleaseWakeLock(token.toString(), flags, releaseTime);
        }

        final String tokenId = token.toString();
        if (mWakeLockInfoMap.containsKey(tokenId)) {
            WakeLockInfo wakeLockInfo = mWakeLockInfoMap.get(tokenId);
            String tag = wakeLockInfo.tag;
            if (mWakeLockSummaryMap.containsKey(tag)) {
                mWakeLockSummaryMap.get(tag).whenRelease(tokenId);
            }
        } else {
            MatrixLog.i(TAG, "onReleaseWakeLock not in mWakeLockInfoMap: %s", tokenId);
        }

        detect();

        mWakeLockInfoMap.remove(tokenId);
    }

    private void detect() {
        detectWakeLockOnceHoldTime();
        detectWakeLockAggregation();
    }

    private void detectWakeLockOnceHoldTime() {
        Iterator<Map.Entry<String, WakeLockInfo>> it = mWakeLockInfoMap.entrySet().iterator();
        Map.Entry<String, WakeLockInfo> entry;
        WakeLockInfo wakeLockInfo;
        String issueKey;
        //imprecisely but acceptable
        final long nowUptimeMillis = SystemClock.uptimeMillis();
        while (it.hasNext()) {
            entry = it.next();
            wakeLockInfo = entry.getValue();
            if ((nowUptimeMillis - wakeLockInfo.uptimeMillis) >= mWakeLockOnceHoldTimeThreshold) {
                issueKey = String.format("%s:%d", wakeLockInfo.tag, SharePluginInfo.IssueType.ISSUE_WAKE_LOCK_ONCE_TOO_LONG);
                if (isPublished(issueKey)) {
                    MatrixLog.v(TAG, "detectWakeLockOnceHoldTime issue already published: %s", issueKey);
                } else {
                    Issue batteryIssue = new Issue(SharePluginInfo.IssueType.ISSUE_WAKE_LOCK_ONCE_TOO_LONG);
                    batteryIssue.setKey(issueKey);
                    JSONObject content = new JSONObject();
                    try {
                        content.put(SharePluginInfo.ISSUE_BATTERY_SUB_TAG, SharePluginInfo.SUB_TAG_WAKE_LOCK);
                        content.put(SharePluginInfo.ISSUE_WAKE_LOCK_TAG, wakeLockInfo.tag);
                        content.put(SharePluginInfo.ISSUE_WAKE_FLAGS, wakeLockInfo.flags);
                        content.put(SharePluginInfo.ISSUE_WAKE_HOLD_TIME, (nowUptimeMillis - wakeLockInfo.uptimeMillis));
                        content.put(SharePluginInfo.ISSUE_WAKE_LOCK_STACK_HISTORY, wakeLockInfo.stackTraceHistory);
                    } catch (JSONException e) {
                        MatrixLog.e(TAG, "json content error: %s", e);
                    }
                    MatrixLog.i(TAG, "detected lock once too long, token:%s, tag:%s", wakeLockInfo.tokenId, wakeLockInfo.tag);
                    batteryIssue.setContent(content);
                    publishIssue(batteryIssue);
                    markPublished(issueKey);
                }
            }
        }
    }

    private void detectWakeLockAggregation() {
        Iterator<Map.Entry<String, WakeLockAggregation>> it = mWakeLockSummaryMap.entrySet().iterator();
        Map.Entry<String, WakeLockAggregation> entry;
        WakeLockAggregation wakeLockAggregation;
        String wakeLockTag;
        final long now = System.currentTimeMillis();
        long statisticalTimeFrame, averageHoldTime1H;
        int currentHours, averageAcquireCntWhenScreenOff1H;
        while (it.hasNext()) {
            entry = it.next();
            wakeLockTag = entry.getKey();
            wakeLockAggregation = entry.getValue();
            statisticalTimeFrame = now - wakeLockAggregation.sinceTime;
            currentHours = (int) (statisticalTimeFrame / 3600000L) + 1;
            currentHours = currentHours <= 0 ? 1 : currentHours;
//            averageAcquireCnt1H = wakeLockAggregation.totalAcquireCnt / currentHours;
            averageAcquireCntWhenScreenOff1H = wakeLockAggregation.totalAcquireCntWhenScreenOff / currentHours;
            averageHoldTime1H = wakeLockAggregation.totalHoldTimeWhenScreenOff / currentHours;

//            MatrixLog.i(TAG, "detectWakeLockAggregation tag:%s currentHours:%d averageAcquireCnt1H:%d averageAcquireCntWhenScreenOff1H:%d averageHoldTime1H:%d",
//                    wakeLockTag, currentHours, averageAcquireCnt1H, averageAcquireCntWhenScreenOff1H, averageHoldTime1H);

//            if (averageAcquireCnt1H >= mWakeLockAcquireCnt1HThreshold
//                    || averageAcquireCntWhenScreenOff1H > mWakeLockAcquireCnt1HThreshold / 2) {
            if (averageAcquireCntWhenScreenOff1H > (mWakeLockAcquireCnt1HThreshold / 2)) {   // only when screen off
                String issueKey = String.format("%s:%d", wakeLockTag, SharePluginInfo.IssueType.ISSUE_WAKE_LOCK_TOO_OFTEN);
                if (isPublished(issueKey)) {
                    MatrixLog.v(TAG, "detectWakeLockAggregation issue already published: %s", issueKey);
                } else {
                    Issue batteryIssue = new Issue(SharePluginInfo.IssueType.ISSUE_WAKE_LOCK_TOO_OFTEN);
                    batteryIssue.setKey(issueKey);
                    batteryIssue.setContent(makeWakeLockSummaryReportContent(wakeLockAggregation, statisticalTimeFrame));
                    publishIssue(batteryIssue);
                    markPublished(issueKey);
                }
            }

            if (averageHoldTime1H >= mWakeLockHoldTime1HThreshold) {
                String issueKey = String.format("%s:%d", wakeLockTag, SharePluginInfo.IssueType.ISSUE_WAKE_LOCK_TOTAL_TOO_LONG);
                if (isPublished(issueKey)) {
                    MatrixLog.v(TAG, "detectWakeLockAggregation issue already published: %s", issueKey);
                } else {
                    Issue batteryIssue = new Issue(SharePluginInfo.IssueType.ISSUE_WAKE_LOCK_TOTAL_TOO_LONG);
                    batteryIssue.setKey(issueKey);
                    batteryIssue.setContent(makeWakeLockSummaryReportContent(wakeLockAggregation, statisticalTimeFrame));
                    publishIssue(batteryIssue);
                    markPublished(issueKey);
                }
            }

        }
    }

    private void calWakeLockAggregation() {
        Iterator<Map.Entry<String, WakeLockAggregation>> it = mWakeLockSummaryMap.entrySet().iterator();
        while (it.hasNext()) {
           it.next().getValue().calAggregation();
        }
    }

    private JSONObject makeWakeLockSummaryReportContent(WakeLockAggregation wakeLockAggregation, long statisticalTimeFrame) {
        JSONObject content = new JSONObject();
        try {
//            content = DeviceUtil.getDeviceInfo(content, getApp)

            content.put(SharePluginInfo.ISSUE_WAKE_LOCK_TAG, wakeLockAggregation.tag);
            content.put(SharePluginInfo.ISSUE_BATTERY_SUB_TAG, SharePluginInfo.SUB_TAG_WAKE_LOCK);
            content.put(SharePluginInfo.ISSUE_WAKE_LOCK_STATISTICAL_TIME_FRAME, statisticalTimeFrame);
            content.put(SharePluginInfo.ISSUE_WAKE_LOCK_STATISTICAL_ACQUIRE_CNT, wakeLockAggregation.totalAcquireCnt);
            content.put(SharePluginInfo.ISSUE_WAKE_LOCK_STATISTICAL_ACQUIRE_CNT_WHEN_SCREEN_OFF, wakeLockAggregation.totalAcquireCntWhenScreenOff);
            content.put(SharePluginInfo.ISSUE_WAKE_LOCK_STATISTICAL_HOLD_TIME, wakeLockAggregation.totalHoldTime);
            content.put(SharePluginInfo.ISSUE_WAKE_LOCK_STACK_HISTORY, wakeLockAggregation.stackTraceHistory);
        } catch (JSONException e) {
            MatrixLog.e(TAG, "json content error: %s", e);
        }

        return content;
    }

    private static final class WakeLockInfo {
        final String tokenId;
        final long acquireTime;
        final long uptimeMillis;
        final String tag;
        final int flags;
        StackTraceHistory stackTraceHistory;

        WakeLockInfo(String tokenId, String tag, int flags, long acquireTime) {
            this.tokenId = tokenId;
            this.tag = tag;
            this.flags = flags;
            this.acquireTime = acquireTime;
            this.uptimeMillis = SystemClock.uptimeMillis();
            stackTraceHistory = new StackTraceHistory();
        }
    }

    private static final class WakeLockAggregation {
        final String tag;
        final long sinceTime;
        long totalHoldTime;
        long totalHoldTimeWhenScreenOff;
        int totalAcquireCnt;
        int totalAcquireCntWhenScreenOff;
        StackTraceHistory stackTraceHistory;
        private final Map<String, Boolean> currentWakeLocks;
        private long lastCaledHoldTime;
        boolean lastScreenOn;

        WakeLockAggregation(String tag) {
            this.tag = tag;
            totalHoldTime = 0;
            totalHoldTimeWhenScreenOff = 0;
            totalAcquireCnt = 0;
            totalAcquireCntWhenScreenOff = 0;
            lastCaledHoldTime = -1;
            stackTraceHistory = new StackTraceHistory();
            sinceTime = System.currentTimeMillis();
            currentWakeLocks = new HashMap<>();
        }

        void whenAcquire(String tokenId, boolean isScreenOn) {
            lastScreenOn = isScreenOn;
            totalAcquireCnt++;
            if (!isScreenOn) {
                totalAcquireCntWhenScreenOff++;
            }
            currentWakeLocks.put(tokenId, true);
            if (lastCaledHoldTime < 0) {
                lastCaledHoldTime = SystemClock.uptimeMillis();
            }
        }

        void whenRelease(String tokenId) {
            calAggregation();
            currentWakeLocks.remove(tokenId);
            if (!isHeld()) {
                lastCaledHoldTime = -1;
            }
        }

        private boolean isHeld() {
            return !currentWakeLocks.isEmpty();
        }

        private void calAggregation() {
            if (lastCaledHoldTime < 0) {
                return;
            }
            final long now = SystemClock.uptimeMillis();
            totalHoldTime = totalHoldTime + (now - lastCaledHoldTime);
            if (!lastScreenOn) {
                totalHoldTimeWhenScreenOff += (now - lastCaledHoldTime);
            }
            lastCaledHoldTime = now;
        }
    }

    private static final class StackTraceHistory {
        final Vector<String> stackTraceHistory;

        StackTraceHistory() {
            stackTraceHistory = new Vector<>();
        }

        void recordStackTrace(String stackTrace) {
            stackTraceHistory.add(stackTrace);
        }

        @Override
        public String toString() {
            StringBuilder sb = new StringBuilder();
            for (int i = 0; i < stackTraceHistory.size(); i++) {
                sb.append(stackTraceHistory.get(i)).append("\t\t");
            }

            return sb.toString();
        }
    }

    private static final class WakeLockInfoRecorder {
        private static final int MAX_CACHE_RECORD_NUM = 10;

        private int mCacheRecordNum;
        @SuppressWarnings("PMD")
        private final StringBuilder mCacheRecord;
        private final String mRecordFilePath;

        WakeLockInfoRecorder() {
            String date = MatrixUtil.formatTime("yyyy-MM-dd", System.currentTimeMillis());
            mRecordFilePath = String.format("%s/com.tencent.matrix/wakelock-detector-record/%s/wakelocks-%s",
                    Environment.getExternalStorageDirectory().getAbsolutePath(), BatteryCanaryUtil.getPackageName(), date);
            mCacheRecord = new StringBuilder();

            MatrixLog.i(TAG, "WakeLockInfoRecorder path:%s", mRecordFilePath);
        }

        public void onAcquireWakeLock(String token, int flags, String tag,
                                      String stackTrace, long acquireTime) {
            String time = MatrixUtil.formatTime("yyyy-MM-dd HH:mm", acquireTime);
            mCacheRecord.append(time)
                    .append(" onAcquireWakeLock token:").append(token)
                    .append(" flags:").append(flags)
                    .append(" tag:").append(tag)
                    .append('\n').append(stackTrace).append('\n');

            mCacheRecordNum++;

            checkDumpCache();
        }

        public void onReleaseWakeLock(String token, int flags, long releaseTime) {
            String time = MatrixUtil.formatTime("yyyy-MM-dd HH:mm", releaseTime);
            mCacheRecord.append(time)
                    .append(" onReleaseWakeLock token:").append(token)
                    .append(" flags:").append(flags)
                    .append("\n\n");

            mCacheRecordNum++;

            checkDumpCache();
        }

        private void checkDumpCache() {
            if (mCacheRecordNum >= MAX_CACHE_RECORD_NUM) {
                dumpCache();
                mCacheRecordNum = 0;
                mCacheRecord.delete(0, mCacheRecord.length());
            }
        }

        private void dumpCache() {
            BufferedWriter bw = null;
            try {
                File f = new File((mRecordFilePath));
                if (!f.getParentFile().mkdirs() && !f.getParentFile().exists()) {
                    MatrixLog.e(TAG, "doRecord mkdirs failed");
                    return;
                }

                bw = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(f, true)));
                final String record = mCacheRecord.toString();
                bw.write(record, 0, record.length());
                bw.flush();
            } catch (FileNotFoundException e) {
                MatrixLog.w(TAG, "dumpCache exp:%s", e.getLocalizedMessage());
            } catch (IOException e) {
                MatrixLog.w(TAG, "dumpCache exp:%s", e.getLocalizedMessage());
            } finally {
                try {
                    if (bw != null) {
                        bw.close();
                    }
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }

        }
    }
}
