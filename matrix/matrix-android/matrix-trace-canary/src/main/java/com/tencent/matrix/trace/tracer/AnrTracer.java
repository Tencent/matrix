package com.tencent.matrix.trace.tracer;

import android.os.Handler;
import android.os.Looper;
import android.os.Process;
import android.os.SystemClock;

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

public class AnrTracer extends Tracer {

    private static final String TAG = "Matrix.AnrTracer";
    private Handler anrHandler;
    private Handler lagHandler;
    private final TraceConfig traceConfig;
    private volatile AnrHandleTask anrTask;
    private volatile LagHandleTask lagTask;
    private boolean isAnrTraceEnable;

    public AnrTracer(TraceConfig traceConfig) {
        this.traceConfig = traceConfig;
        this.isAnrTraceEnable = traceConfig.isAnrTraceEnable();
    }

    @Override
    public void onAlive() {
        super.onAlive();
        if (isAnrTraceEnable) {
            UIThreadMonitor.getMonitor().addObserver(this);
            this.anrHandler = new Handler(MatrixHandlerThread.getDefaultHandler().getLooper());
            this.lagHandler = new Handler(MatrixHandlerThread.getDefaultHandler().getLooper());
        }
    }

    @Override
    public void onDead() {
        super.onDead();
        if (isAnrTraceEnable) {
            UIThreadMonitor.getMonitor().removeObserver(this);
            if (null != anrTask) {
                anrTask.getBeginRecord().release();
            }
            anrHandler.removeCallbacksAndMessages(null);
            lagHandler.removeCallbacksAndMessages(null);
        }
    }

    @Override
    public void dispatchBegin(long beginNs, long cpuBeginMs, long token) {
        super.dispatchBegin(beginNs, cpuBeginMs, token);
//        long inputCost = UIThreadMonitor.getMonitor().getInputEventCost();
//        if (inputCost > Constants.DEFAULT_INPUT_EXPIRED_TIME * Constants.TIME_MILLIS_TO_NANO) {
//            printInputExpired(inputCost);
//        }
        anrTask = new AnrHandleTask(AppMethodBeat.getInstance().maskIndex("AnrTracer#dispatchBegin"), token);
        lagTask = new LagHandleTask();

        if (traceConfig.isDevEnv()) {
            MatrixLog.v(TAG, "* [dispatchBegin] token:%s index:%s", token, anrTask.beginRecord.index);
        }
        anrHandler.postDelayed(anrTask, Constants.DEFAULT_ANR - (System.nanoTime() - token) / Constants.TIME_MILLIS_TO_NANO);
        lagHandler.postDelayed(lagTask, Constants.DEFAULT_NORMAL_LAG - (System.nanoTime() - token) / Constants.TIME_MILLIS_TO_NANO);
    }


    @Override
    public void dispatchEnd(long beginNs, long cpuBeginMs, long endNs, long cpuEndMs, long token, boolean isBelongFrame) {
        super.dispatchEnd(beginNs, cpuBeginMs, endNs, cpuEndMs, token, isBelongFrame);
        if (traceConfig.isDevEnv()) {
            long cost = (endNs - beginNs) / Constants.TIME_MILLIS_TO_NANO;
            MatrixLog.v(TAG, "[dispatchEnd] token:%s cost:%sms cpu:%sms usage:%s",
                    token, cost, cpuEndMs - cpuBeginMs, Utils.calculateCpuUsage(cpuEndMs - cpuBeginMs, cost));
        }
        if (null != anrTask) {
            anrTask.getBeginRecord().release();
            anrHandler.removeCallbacks(anrTask);
        }
        if (null != lagTask) {
            lagHandler.removeCallbacks(lagTask);
        }
    }

    class LagHandleTask implements Runnable{

        @Override
        public void run() {
            String scene = AppMethodBeat.getVisibleScene();
            boolean isForeground = isForeground();
            try {
                TracePlugin plugin = Matrix.with().getPluginByClass(TracePlugin.class);
                if (null == plugin) {
                    return;
                }

                StackTraceElement[] stackTrace = Looper.getMainLooper().getThread().getStackTrace();

                JSONObject jsonObject = new JSONObject();
                jsonObject = DeviceUtil.getDeviceInfo(jsonObject, Matrix.with().getApplication());
                jsonObject.put(SharePluginInfo.ISSUE_STACK_TYPE, Constants.Type.LAG);
                jsonObject.put(SharePluginInfo.ISSUE_SCENE, scene);
                jsonObject.put(SharePluginInfo.ISSUE_THREAD_STACK, Utils.getStack(stackTrace));
                jsonObject.put(SharePluginInfo.ISSUE_PROCESS_FOREGROUND, isForeground);

                Issue issue = new Issue();
                issue.setTag(SharePluginInfo.TAG_PLUGIN_EVIL_METHOD);
                issue.setContent(jsonObject);
                plugin.onDetectIssue(issue);

            } catch (JSONException e) {
                MatrixLog.e(TAG, "[JSONException error: %s", e);
            }


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
            long curTime = SystemClock.uptimeMillis();
            boolean isForeground = isForeground();
            // process
            int[] processStat = Utils.getProcessPriority(Process.myPid());
            long[] data = AppMethodBeat.getInstance().copyData(beginRecord);
            beginRecord.release();
            String scene = AppMethodBeat.getVisibleScene();

            // memory
            long[] memoryInfo = dumpMemory();

            // Thread state
            Thread.State status = Looper.getMainLooper().getThread().getState();
            StackTraceElement[] stackTrace = Looper.getMainLooper().getThread().getStackTrace();
            String dumpStack = Utils.getStack(stackTrace, "|*\t\t", 12);

            // frame
            UIThreadMonitor monitor = UIThreadMonitor.getMonitor();
            long inputCost = monitor.getQueueCost(UIThreadMonitor.CALLBACK_INPUT, token);
            long animationCost = monitor.getQueueCost(UIThreadMonitor.CALLBACK_ANIMATION, token);
            long traversalCost = monitor.getQueueCost(UIThreadMonitor.CALLBACK_TRAVERSAL, token);

            // trace
            LinkedList<MethodItem> stack = new LinkedList();
            if (data.length > 0) {
                TraceDataUtils.structuredDataToStack(data, stack, true, curTime);
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
            long stackCost = Math.max(Constants.DEFAULT_ANR, TraceDataUtils.stackToString(stack, reportBuilder, logcatBuilder));

            // stackKey
            String stackKey = TraceDataUtils.getTreeKey(stack, stackCost);
            MatrixLog.w(TAG, "%s \npostTime:%s curTime:%s",
                    printAnr(scene, processStat, memoryInfo, status, logcatBuilder, isForeground, stack.size(),
                            stackKey, dumpStack, inputCost, animationCost, traversalCost, stackCost), token / Constants.TIME_MILLIS_TO_NANO, curTime); // for logcat

            if (stackCost >= Constants.DEFAULT_ANR_INVALID) {
                MatrixLog.w(TAG, "The checked anr task was not executed on time. "
                        + "The possible reason is that the current process has a low priority. just pass this report");
                return;
            }
            // report
            try {
                TracePlugin plugin = Matrix.with().getPluginByClass(TracePlugin.class);
                if (null == plugin) {
                    return;
                }
                JSONObject jsonObject = new JSONObject();
                jsonObject = DeviceUtil.getDeviceInfo(jsonObject, Matrix.with().getApplication());
                jsonObject.put(SharePluginInfo.ISSUE_STACK_TYPE, Constants.Type.ANR);
                jsonObject.put(SharePluginInfo.ISSUE_COST, stackCost);
                jsonObject.put(SharePluginInfo.ISSUE_STACK_KEY, stackKey);
                jsonObject.put(SharePluginInfo.ISSUE_SCENE, scene);
                jsonObject.put(SharePluginInfo.ISSUE_TRACE_STACK, reportBuilder.toString());
                jsonObject.put(SharePluginInfo.ISSUE_THREAD_STACK, Utils.getStack(stackTrace));
                jsonObject.put(SharePluginInfo.ISSUE_PROCESS_PRIORITY, processStat[0]);
                jsonObject.put(SharePluginInfo.ISSUE_PROCESS_NICE, processStat[1]);
                jsonObject.put(SharePluginInfo.ISSUE_PROCESS_FOREGROUND, isForeground);
                // memory info
                JSONObject memJsonObject = new JSONObject();
                memJsonObject.put(SharePluginInfo.ISSUE_MEMORY_DALVIK, memoryInfo[0]);
                memJsonObject.put(SharePluginInfo.ISSUE_MEMORY_NATIVE, memoryInfo[1]);
                memJsonObject.put(SharePluginInfo.ISSUE_MEMORY_VM_SIZE, memoryInfo[2]);
                jsonObject.put(SharePluginInfo.ISSUE_MEMORY, memJsonObject);

                Issue issue = new Issue();
                issue.setKey(token + "");
                issue.setTag(SharePluginInfo.TAG_PLUGIN_EVIL_METHOD);
                issue.setContent(jsonObject);
                plugin.onDetectIssue(issue);

            } catch (JSONException e) {
                MatrixLog.e(TAG, "[JSONException error: %s", e);
            }

        }

        private String printAnr(String scene, int[] processStat, long[] memoryInfo, Thread.State state, StringBuilder stack, boolean isForeground,
                                long stackSize, String stackKey, String dumpStack, long inputCost, long animationCost, long traversalCost, long stackCost) {
            StringBuilder print = new StringBuilder();
            print.append(String.format("-\n>>>>>>>>>>>>>>>>>>>>>>> maybe happens ANR(%s ms)! <<<<<<<<<<<<<<<<<<<<<<<\n", stackCost));
            print.append("|* [Status]").append("\n");
            print.append("|*\t\tScene: ").append(scene).append("\n");
            print.append("|*\t\tForeground: ").append(isForeground).append("\n");
            print.append("|*\t\tPriority: ").append(processStat[0]).append("\tNice: ").append(processStat[1]).append("\n");
            print.append("|*\t\tis64BitRuntime: ").append(DeviceUtil.is64BitRuntime()).append("\n");

            print.append("|* [Memory]").append("\n");
            print.append("|*\t\tDalvikHeap: ").append(memoryInfo[0]).append("kb\n");
            print.append("|*\t\tNativeHeap: ").append(memoryInfo[1]).append("kb\n");
            print.append("|*\t\tVmSize: ").append(memoryInfo[2]).append("kb\n");
            print.append("|* [doFrame]").append("\n");
            print.append("|*\t\tinputCost:animationCost:traversalCost").append("\n");
            print.append("|*\t\t").append(inputCost).append(":").append(animationCost).append(":").append(traversalCost).append("\n");
            print.append("|* [Thread]").append("\n");
            print.append(String.format("|*\t\tStack(%s): ", state)).append(dumpStack);
            print.append("|* [Trace]").append("\n");
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

    private String printInputExpired(long inputCost) {
        StringBuilder print = new StringBuilder();
        String scene = AppMethodBeat.getVisibleScene();
        boolean isForeground = isForeground();
        // memory
        long[] memoryInfo = dumpMemory();
        // process
        int[] processStat = Utils.getProcessPriority(Process.myPid());
        print.append(String.format("-\n>>>>>>>>>>>>>>>>>>>>>>> maybe happens Input ANR(%s ms)! <<<<<<<<<<<<<<<<<<<<<<<\n", inputCost));
        print.append("|* [Status]").append("\n");
        print.append("|*\t\tScene: ").append(scene).append("\n");
        print.append("|*\t\tForeground: ").append(isForeground).append("\n");
        print.append("|*\t\tPriority: ").append(processStat[0]).append("\tNice: ").append(processStat[1]).append("\n");
        print.append("|*\t\tis64BitRuntime: ").append(DeviceUtil.is64BitRuntime()).append("\n");
        print.append("|* [Memory]").append("\n");
        print.append("|*\t\tDalvikHeap: ").append(memoryInfo[0]).append("kb\n");
        print.append("|*\t\tNativeHeap: ").append(memoryInfo[1]).append("kb\n");
        print.append("|*\t\tVmSize: ").append(memoryInfo[2]).append("kb\n");
        print.append("=========================================================================");
        return print.toString();
    }

    private long[] dumpMemory() {
        long[] memory = new long[3];
        memory[0] = DeviceUtil.getDalvikHeap();
        memory[1] = DeviceUtil.getNativeHeap();
        memory[2] = DeviceUtil.getVmSize();
        return memory;
    }


}
