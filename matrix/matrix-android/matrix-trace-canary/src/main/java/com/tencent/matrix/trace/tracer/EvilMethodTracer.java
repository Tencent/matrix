/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

package com.tencent.matrix.trace.tracer;

import android.os.Process;

import com.tencent.matrix.AppActiveMatrixDelegate;
import com.tencent.matrix.Matrix;
import com.tencent.matrix.report.Issue;
import com.tencent.matrix.trace.TracePlugin;
import com.tencent.matrix.trace.config.SharePluginInfo;
import com.tencent.matrix.trace.config.TraceConfig;
import com.tencent.matrix.trace.constants.Constants;
import com.tencent.matrix.trace.core.AppMethodBeat;
import com.tencent.matrix.trace.core.LooperMonitor;
import com.tencent.matrix.trace.items.MethodItem;
import com.tencent.matrix.trace.listeners.IDispatchListener;
import com.tencent.matrix.trace.util.TraceDataUtils;
import com.tencent.matrix.trace.util.Utils;
import com.tencent.matrix.util.DeviceUtil;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;

public class EvilMethodTracer extends Tracer implements IDispatchListener {

    private static final String TAG = "Matrix.EvilMethodTracer";
    private final TraceConfig config;
    private AppMethodBeat.IndexRecord indexRecord;
    private long evilThresholdMs;
    private final boolean isEvilMethodTraceEnable;

    public EvilMethodTracer(TraceConfig config) {
        this.config = config;
        this.evilThresholdMs = config.getEvilThresholdMs();
        this.isEvilMethodTraceEnable = config.isEvilMethodTraceEnable();
    }

    @Override
    public void onAlive() {
        super.onAlive();
        if (isEvilMethodTraceEnable) {
            LooperMonitor.register(this);
        }

    }

    @Override
    public void onDead() {
        super.onDead();
        if (isEvilMethodTraceEnable) {
            LooperMonitor.unregister(this);
        }
    }

    @Override
    public boolean isValid() {
        return true;
    }

    @Override
    public void onDispatchBegin(String log) {
        indexRecord = AppMethodBeat.getInstance().maskIndex("EvilMethodTracer#dispatchBegin");
    }

    @Override
    public void onDispatchEnd(String log, long beginNs, long endNs) {
        long dispatchCost = endNs - beginNs;
        try {
            if (dispatchCost >= evilThresholdMs) {
                long[] data = AppMethodBeat.getInstance().copyData(indexRecord);
                String scene = AppActiveMatrixDelegate.INSTANCE.getVisibleScene();
                MatrixHandlerThread.getDefaultHandler().post(new AnalyseTask(isForeground(), scene, data, dispatchCost, endNs));
            }
        } finally {
            indexRecord.release();
        }
    }

    public void modifyEvilThresholdMs(long evilThresholdMs) {
        this.evilThresholdMs = evilThresholdMs;
    }

    private static class AnalyseTask implements Runnable {
        long[] data;
        long cost;
        long endMs;
        String scene;
        boolean isForeground;

        AnalyseTask(boolean isForeground, String scene, long[] data, long cost, long endMs) {
            this.isForeground = isForeground;
            this.scene = scene;
            this.cost = cost;
            this.data = data;
            this.endMs = endMs;
        }

        void analyse() {

            // process
            int[] processStat = Utils.getProcessPriority(Process.myPid());
            LinkedList<MethodItem> stack = new LinkedList<>();
            if (data.length > 0) {
                TraceDataUtils.structuredDataToStack(data, stack, true, endMs);
                TraceDataUtils.trimStack(stack, Constants.TARGET_EVIL_METHOD_STACK, new TraceDataUtils.IStructuredDataFilter() {
                    @Override
                    public boolean isFilter(long during, int filterCount) {
                        return during < (long) filterCount * Constants.TIME_UPDATE_CYCLE_MS;
                    }

                    @Override
                    public int getFilterMaxCount() {
                        return Constants.FILTER_STACK_MAX_COUNT;
                    }

                    @Override
                    public void fallback(List<MethodItem> stack, int size) {
                        MatrixLog.w(TAG, "[fallback] size:%s targetSize:%s stack:%s", size, Constants.TARGET_EVIL_METHOD_STACK, stack);
                        Iterator<MethodItem> iterator = stack.listIterator(Math.min(size, Constants.TARGET_EVIL_METHOD_STACK));
                        while (iterator.hasNext()) {
                            iterator.next();
                            iterator.remove();
                        }
                    }
                });
            }


            StringBuilder reportBuilder = new StringBuilder();
            StringBuilder logcatBuilder = new StringBuilder();
            long stackCost = Math.max(cost, TraceDataUtils.stackToString(stack, reportBuilder, logcatBuilder));
            String stackKey = TraceDataUtils.getTreeKey(stack, stackCost);

            MatrixLog.w(TAG, "%s", printEvil(scene, processStat, isForeground, logcatBuilder, stack.size(), stackKey, cost)); // for logcat

            // report
            try {
                TracePlugin plugin = Matrix.with().getPluginByClass(TracePlugin.class);
                if (null == plugin) {
                    return;
                }
                JSONObject jsonObject = new JSONObject();
                DeviceUtil.getDeviceInfo(jsonObject, Matrix.with().getApplication());

                jsonObject.put(SharePluginInfo.ISSUE_STACK_TYPE, Constants.Type.NORMAL);
                jsonObject.put(SharePluginInfo.ISSUE_COST, stackCost);
                jsonObject.put(SharePluginInfo.ISSUE_SCENE, scene);
                jsonObject.put(SharePluginInfo.ISSUE_TRACE_STACK, reportBuilder.toString());
                jsonObject.put(SharePluginInfo.ISSUE_STACK_KEY, stackKey);

                Issue issue = new Issue();
                issue.setTag(SharePluginInfo.TAG_PLUGIN_EVIL_METHOD);
                issue.setContent(jsonObject);
                plugin.onDetectIssue(issue);

            } catch (JSONException e) {
                MatrixLog.e(TAG, "[JSONException error: %s", e);
            }

        }

        @Override
        public void run() {
            analyse();
        }

        private String printEvil(String scene, int[] processStat, boolean isForeground, StringBuilder stack, long stackSize, String stackKey, long allCost) {
            StringBuilder print = new StringBuilder();
            print.append(String.format("-\n>>>>>>>>>>>>>>>>>>>>> maybe happens Jankiness!(%sms) <<<<<<<<<<<<<<<<<<<<<\n", allCost));
            print.append("|* [Status]").append("\n");
            print.append("|*\t\tScene: ").append(scene).append("\n");
            print.append("|*\t\tForeground: ").append(isForeground).append("\n");
            print.append("|*\t\tPriority: ").append(processStat[0]).append("\tNice: ").append(processStat[1]).append("\n");
            print.append("|*\t\tis64BitRuntime: ").append(DeviceUtil.is64BitRuntime()).append("\n");
            if (stackSize > 0) {
                print.append("|*\t\tStackKey: ").append(stackKey).append("\n");
                print.append(stack.toString());
            } else {
                print.append(String.format("AppMethodBeat is close[%s].", AppMethodBeat.getInstance().isAlive())).append("\n");
            }

            print.append("=========================================================================");
            return print.toString();
        }
    }

}
