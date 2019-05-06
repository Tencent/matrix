package com.tencent.matrix.trace.tracer;

import android.os.Process;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.report.Issue;
import com.tencent.matrix.trace.TracePlugin;
import com.tencent.matrix.trace.config.SharePluginInfo;
import com.tencent.matrix.trace.config.TraceConfig;
import com.tencent.matrix.trace.constants.Constants;
import com.tencent.matrix.trace.core.AppMethodBeat;
import com.tencent.matrix.trace.core.UIThreadMonitor;
import com.tencent.matrix.trace.items.MethodItem;
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

public class EvilMethodTracer extends Tracer {

    private static final String TAG = "Matrix.EvilMethodTracer";
    private final TraceConfig config;
    private AppMethodBeat.IndexRecord indexRecord;
    private long[] queueTypeCosts = new long[3];
    private long evilThresholdMs;
    private boolean isEvilMethodTraceEnable;

    public EvilMethodTracer(TraceConfig config) {
        this.config = config;
        this.evilThresholdMs = config.getEvilThresholdMs();
        this.isEvilMethodTraceEnable = config.isEvilMethodTraceEnable();
    }

    @Override
    public void onAlive() {
        super.onAlive();
        if (isEvilMethodTraceEnable) {
            UIThreadMonitor.getMonitor().addObserver(this);
        }

    }

    @Override
    public void onDead() {
        super.onDead();
        if (isEvilMethodTraceEnable) {
            UIThreadMonitor.getMonitor().removeObserver(this);
        }
    }


    @Override
    public void dispatchBegin(long beginMs, long cpuBeginMs, long token) {
        super.dispatchBegin(beginMs, cpuBeginMs, token);
        indexRecord = AppMethodBeat.getInstance().maskIndex("EvilMethodTracer#dispatchBegin");
    }


    @Override
    public void doFrame(String focusedActivityName, long start, long end, long frameCostMs, long inputCostNs, long animationCostNs, long traversalCostNs) {
        queueTypeCosts[0] = inputCostNs;
        queueTypeCosts[1] = animationCostNs;
        queueTypeCosts[2] = traversalCostNs;
    }


    @Override
    public void dispatchEnd(long beginMs, long cpuBeginMs, long endMs, long cpuEndMs, long token, boolean isBelongFrame) {
        super.dispatchEnd(beginMs, cpuBeginMs, endMs, cpuEndMs, token, isBelongFrame);
        long start = config.isDevEnv() ? System.currentTimeMillis() : 0;
        try {
            long dispatchCost = endMs - beginMs;
            if (dispatchCost >= evilThresholdMs) {
                long[] data = AppMethodBeat.getInstance().copyData(indexRecord);
                long[] queueCosts = new long[3];
                System.arraycopy(queueTypeCosts, 0, queueCosts, 0, 3);
                String scene = AppMethodBeat.getVisibleScene();
                MatrixHandlerThread.getDefaultHandler().post(new AnalyseTask(isForeground(), scene, data, queueCosts, cpuEndMs - cpuBeginMs, endMs - beginMs, endMs));
            }
        } finally {
            indexRecord.release();
            if (config.isDevEnv()) {
                String usage = Utils.calculateCpuUsage(cpuEndMs - cpuBeginMs, endMs - beginMs);
                MatrixLog.v(TAG, "[dispatchEnd] token:%s cost:%sms cpu:%sms usage:%s innerCost:%s",
                        token, endMs - beginMs, cpuEndMs - cpuBeginMs, usage, System.currentTimeMillis() - start);
            }
        }
    }

    private class AnalyseTask implements Runnable {
        long[] queueCost;
        long[] data;
        long cpuCost;
        long cost;
        long endMs;
        String scene;
        boolean isForeground;

        AnalyseTask(boolean isForeground, String scene, long[] data, long[] queueCost, long cpuCost, long cost, long endMs) {
            this.isForeground = isForeground;
            this.scene = scene;
            this.cost = cost;
            this.cpuCost = cpuCost;
            this.data = data;
            this.queueCost = queueCost;
            this.endMs = endMs;
        }

        void analyse() {

            // process
            int[] processStat = Utils.getProcessPriority(Process.myPid());
            String usage = Utils.calculateCpuUsage(cpuCost, cost);
            LinkedList<MethodItem> stack = new LinkedList();
            if (data.length > 0) {
                TraceDataUtils.structuredDataToStack(data, stack, true, endMs);
                TraceDataUtils.trimStack(stack, Constants.TARGET_EVIL_METHOD_STACK, new TraceDataUtils.IStructuredDataFilter() {
                    @Override
                    public boolean isFilter(long during, int filterCount) {
                        return during < filterCount * Constants.TIME_UPDATE_CYCLE_MS;
                    }

                    @Override
                    public int getFilterMaxCount() {
                        return Constants.FILTER_STACK_MAX_COUNT;
                    }

                    @Override
                    public void fallback(List<MethodItem> stack, int size) {
                        MatrixLog.w(TAG, "[fallback] size:%s targetSize:%s stack:%s", size, Constants.TARGET_EVIL_METHOD_STACK, stack);
                        Iterator iterator = stack.listIterator(Math.min(size, Constants.TARGET_EVIL_METHOD_STACK));
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

            MatrixLog.w(TAG, "%s", printEvil(processStat, isForeground, logcatBuilder, stack.size(), stackKey, usage, queueCost[0], queueCost[1], queueCost[2], cost)); // for logcat

            // report
            try {
                TracePlugin plugin = Matrix.with().getPluginByClass(TracePlugin.class);
                JSONObject jsonObject = new JSONObject();
                jsonObject = DeviceUtil.getDeviceInfo(jsonObject, Matrix.with().getApplication());

                jsonObject.put(SharePluginInfo.ISSUE_STACK_TYPE, Constants.Type.NORMAL);
                jsonObject.put(SharePluginInfo.ISSUE_COST, stackCost);
                jsonObject.put(SharePluginInfo.ISSUE_CPU_USAGE, usage);
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

        private String printEvil(int[] processStat, boolean isForeground, StringBuilder stack, long stackSize, String stackKey, String usage, long inputCost,
                                 long animationCost, long traversalCost, long allCost) {
            StringBuilder print = new StringBuilder();
            print.append(String.format(" \n>>>>>>>>>>>>>>>>>>>>> maybe happens Jankiness!(%sms) <<<<<<<<<<<<<<<<<<<<<\n", allCost));
            print.append("|* [ProcessStat]").append("\n");
            print.append("|*\tPriority: ").append(processStat[0]);
            print.append("|*\tNice: ").append(processStat[1]);
            print.append("|*\tForeground: ").append(isForeground);
            print.append("|* [CPU]").append("\n");
            print.append("|*\tusage: ").append(usage).append("\n");
            print.append("|* [Memory]").append("\n");  // todo
            print.append("|* [doFrame]").append("\n");
            print.append("|*\tinputCost: ").append(inputCost).append("\n");
            print.append("|*\tanimationCost: ").append(animationCost).append("\n");
            print.append("|*\ttraversalCost: ").append(traversalCost).append("\n");
            print.append("|* [Trace]").append("\n");
            print.append("|*\tStackSize: ").append(stackSize).append("\n");
            print.append("|*\tStackKey: ").append(stackKey).append("\n");

            if (config.isDebug()) {
                print.append(stack.toString());
            }

            print.append("=========================================================================");
            return print.toString();
        }
    }

}
