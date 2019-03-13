package com.tencent.matrix.trace.tracer;

import android.os.Handler;
import android.os.Looper;

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

import java.util.LinkedList;
import java.util.List;


public class AnrTracer extends Tracer implements UIThreadMonitor.ILooperObserver {

    private static final String TAG = "Matrix.AnrTracer";
    private Handler anrHandler;
    private final TraceConfig traceConfig;
    private volatile AnrHandleTask anrTask;

    public AnrTracer(TraceConfig traceConfig) {
        this.traceConfig = traceConfig;
    }

    @Override
    public void onAlive() {
        if (traceConfig.isAnrTraceEnable()) {
            UIThreadMonitor.getMonitor().addObserver(this);
            this.anrHandler = new Handler(MatrixHandlerThread.getDefaultHandlerThread().getLooper());
        }
    }

    @Override
    public void onDead() {
        if (traceConfig.isAnrTraceEnable()) {
            UIThreadMonitor.getMonitor().removeObserver(this);
            if (null != anrTask) {
                anrTask.getBeginRecord().release();
            }
            anrHandler.removeCallbacksAndMessages(null);
        }
    }

    @Override
    public void dispatchBegin(long beginMs, long cpuBeginMs, long token) {
        anrTask = new AnrHandleTask(AppMethodBeat.getInstance().maskIndex("AnrTracer#dispatchBegin"), token);
        if (traceConfig.isDevEnv()) {
            MatrixLog.v(TAG, "* [dispatchBegin] token:%s index:%s", token, anrTask.beginRecord.index);
        }
        anrHandler.postDelayed(anrTask, Constants.DEFAULT_ANR);
    }

    @Override
    public void doFrame(String focusedActivityName, long start, long end, long frameCostMs, long inputCost, long animationCost, long traversalCost) {
        if (traceConfig.isDevEnv()) {
            MatrixLog.v(TAG, "--> [doFrame] activityName:%s frameCost:%sms [%s:%s:%s]ns", focusedActivityName, frameCostMs, inputCost, animationCost, traversalCost);
        }
    }


    @Override
    public void dispatchEnd(long beginMs, long cpuBeginMs, long endMs, long cpuEndMs, long token, boolean isBelongFrame) {
        if (traceConfig.isDevEnv()) {
            MatrixLog.v(TAG, "[dispatchEnd] token:%s cost:%sms cpu:%sms usage:%s",
                    token, endMs - beginMs, cpuEndMs - cpuBeginMs, Utils.calculateCpuUsage(cpuEndMs - cpuBeginMs, endMs - beginMs));
        }
        if (null != anrTask) {
            anrTask.getBeginRecord().release();
            anrHandler.removeCallbacks(anrTask);
        }
    }


    class AnrHandleTask implements Runnable {

        AppMethodBeat.IndexRecord beginRecord;
        long token;

        public AppMethodBeat.IndexRecord getBeginRecord() {
            return beginRecord;
        }

        AnrHandleTask(AppMethodBeat.IndexRecord record, long token) {
            this.beginRecord = record;
            this.token = token;
        }

        @Override
        public void run() {
            anrTask = null;
            long[] data = AppMethodBeat.getInstance().copyData(beginRecord);
            beginRecord.release();

            long[] memoryInfo = dumpMemory();
            Thread.State status = Looper.getMainLooper().getThread().getState();
            String dumpStack = Utils.getStack(Looper.getMainLooper().getThread().getStackTrace(), "|*        ");

            UIThreadMonitor monitor = UIThreadMonitor.getMonitor();
            long inputCost = monitor.getQueueCost(UIThreadMonitor.CALLBACK_INPUT, token);
            long animationCost = monitor.getQueueCost(UIThreadMonitor.CALLBACK_ANIMATION, token);
            long traversalCost = monitor.getQueueCost(UIThreadMonitor.CALLBACK_TRAVERSAL, token);

            LinkedList<MethodItem> stack = new LinkedList();
            TraceDataUtils.structuredDataToStack(data, stack);
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
                    List list = stack.subList(0, Constants.TARGET_EVIL_METHOD_STACK);
                    stack.clear();
                    stack.addAll(list);
                }
            });

            String stackKey = TraceDataUtils.getTreeKey(stack, Constants.MAX_LIMIT_ANALYSE_STACK_KEY_NUM);

            StringBuilder reportBuilder = new StringBuilder();
            StringBuilder logcatBuilder = new StringBuilder();
            long stackCost = Math.max(Constants.DEFAULT_ANR, TraceDataUtils.stackToString(stack, reportBuilder, logcatBuilder));

            MatrixLog.w(TAG, printAnr(memoryInfo, status, logcatBuilder, stack.size(), stackKey, dumpStack, inputCost, animationCost, traversalCost)); // for logcat

            // report
            try {
                TracePlugin plugin = Matrix.with().getPluginByClass(TracePlugin.class);
                JSONObject jsonObject = new JSONObject();
                jsonObject = DeviceUtil.getDeviceInfo(jsonObject, Matrix.with().getApplication());
                jsonObject.put(SharePluginInfo.ISSUE_STACK_TYPE, Constants.Type.ANR);
                jsonObject.put(SharePluginInfo.ISSUE_COST, stackCost);
                jsonObject.put(SharePluginInfo.ISSUE_STACK, reportBuilder.toString());
                // memory info
                JSONObject memJsonObject = new JSONObject();
                memJsonObject.put(SharePluginInfo.ISSUE_MEMORY_DALVIK, memoryInfo[0]);
                memJsonObject.put(SharePluginInfo.ISSUE_MEMORY_NATIVIE, memoryInfo[1]);
                memJsonObject.put(SharePluginInfo.ISSUE_MEMORY_VMSIZE, memoryInfo[2]);
                jsonObject.put(SharePluginInfo.ISSUE_MEMORY, memJsonObject);

                Issue issue = new Issue();
                issue.setTag(SharePluginInfo.TAG_PLUGIN_EVIL_METHOD);
                issue.setContent(jsonObject);
                plugin.onDetectIssue(issue);

            } catch (JSONException e) {
                MatrixLog.e(TAG, "[JSONException error: %s", e);
            }

        }

        private String printAnr(long[] memoryInfo, Thread.State state, StringBuilder stack, long stackSize, String stackKey, String dumpStack, long inputCost, long animationCost, long traversalCost) {
            StringBuilder print = new StringBuilder();
            print.append(" \n>>>>>>>>>>>>>>>>>>>>>>> maybe happens ANR(5s)! <<<<<<<<<<<<<<<<<<<<<<<\n");
            print.append("|* [Memory]").append("\n");  // todo
            print.append("|*   DalvikHeap: ").append(memoryInfo[0]).append("kb\n");
            print.append("|*   NativeHeap: ").append(memoryInfo[1]).append("kb\n");
            print.append("|*   VmSize: ").append(memoryInfo[2]).append("kb\n");
            print.append("|* [doFrame]").append("\n");
            print.append("|*   inputCost: ").append(inputCost).append("\n");
            print.append("|*   animationCost: ").append(animationCost).append("\n");
            print.append("|*   traversalCost: ").append(traversalCost).append("\n");
            print.append("|* [Thread]").append("\n");
            print.append("|*   State: ").append(state).append("\n");
            print.append("|*   Stack: ").append(dumpStack);
            print.append("|* [Trace]").append("\n");
            print.append("|*   StackSize: ").append(stackSize).append("\n");
            print.append("|*   StackKey: ").append(stackKey).append("\n");

            if (traceConfig.isDebug()) {
                print.append(stack.toString());
            }

            print.append("=========================================================================");
            return print.toString();
        }

    }

    private long[] dumpMemory() {
        long[] memory = new long[3];
        memory[0] = DeviceUtil.getDalvikHeap();
        memory[1] = DeviceUtil.getNativeHeap();
        memory[2] = DeviceUtil.getVmSize();
        return memory;
    }
}
