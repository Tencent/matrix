package com.tencent.matrix.trace.tracer;

import android.os.Handler;

import com.tencent.matrix.trace.config.TraceConfig;
import com.tencent.matrix.trace.constants.Constants;
import com.tencent.matrix.trace.core.MethodBeat;
import com.tencent.matrix.trace.core.UIThreadMonitor;
import com.tencent.matrix.trace.items.MethodItem;
import com.tencent.matrix.trace.util.TraceDataUtils;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;

import java.util.LinkedList;
import java.util.List;


public class AnrTracer implements ITracer, UIThreadMonitor.ILooperObserver {

    private static final String TAG = "Matrix.AnrTracer";
    private Handler anrHandler;
    private final TraceConfig traceConfig;
    private volatile AnrHandleTask anrTask;

    public AnrTracer(TraceConfig traceConfig) {
        this.traceConfig = traceConfig;
        this.anrHandler = new Handler(MatrixHandlerThread.getDefaultHandlerThread().getLooper());
    }

    @Override
    public void onStartTrace() {
        UIThreadMonitor.getMonitor().addObserver(this);
    }

    @Override
    public void onCloseTrace() {
        UIThreadMonitor.getMonitor().removeObserver(this);
        if (null != anrTask) {
            anrTask.getBeginRecord().release();
        }
    }

    @Override
    public void dispatchBegin(long beginNs, long token) {
        anrTask = new AnrHandleTask(MethodBeat.getInstance().maskIndex(), token);
        if (traceConfig.isDebug()) {
            MatrixLog.d(TAG, "* [dispatchBegin] token:%s index:%s", token, anrTask.beginRecord.index);
        }
        anrHandler.postDelayed(anrTask, Constants.DEFAULT_ANR);
    }

    @Override
    public void doFrame(long start, long end, long frameCost, long inputCost, long animationCost, long traversalCost) {
        if (traceConfig.isDebug()) {
            MatrixLog.d(TAG, "--> [doFrame] frameCost:%s [%s:%s:%s]", frameCost, inputCost, animationCost, traversalCost);
        }
    }


    @Override
    public void dispatchEnd(long beginNs, long endNs, long token) {
        if (traceConfig.isDebug()) {
            MatrixLog.d(TAG, "[dispatchEnd] token:%s cost:%sns", token, endNs - beginNs);
        }
        if (null != anrTask) {
            anrTask.getBeginRecord().release();
            anrHandler.removeCallbacks(anrTask);
        }
    }


    class AnrHandleTask implements Runnable {

        MethodBeat.IndexRecord beginRecord;
        long token;

        public MethodBeat.IndexRecord getBeginRecord() {
            return beginRecord;
        }

        AnrHandleTask(MethodBeat.IndexRecord record, long token) {
            this.beginRecord = record;
            this.token = token;
        }

        @Override
        public void run() {
            anrTask = null;
            UIThreadMonitor monitor = UIThreadMonitor.getMonitor();
            long inputCost = monitor.getQueueCost(UIThreadMonitor.CALLBACK_INPUT, token);
            long animationCost = monitor.getQueueCost(UIThreadMonitor.CALLBACK_ANIMATION, token);
            long traversalCost = monitor.getQueueCost(UIThreadMonitor.CALLBACK_TRAVERSAL, token);

            MatrixLog.w(TAG, ">>>>>> maybe happens ANR! [%s:%s:%s]", inputCost, animationCost, traversalCost);

            long[] data = MethodBeat.getInstance().copyData(beginRecord);
            beginRecord.release();

            if (data.length <= 0) {
                MatrixLog.w(TAG, "data.length is 0!");
                return;
            }

            LinkedList<MethodItem> stack = new LinkedList();
            TraceDataUtils.structuredDataToStack(data, stack);
            TraceDataUtils.trimStack(stack, Constants.MAX_EVIL_METHOD_STACK, new TraceDataUtils.IStructuredDataFilter() {
                @Override
                public boolean isFilter(long during, int filterCount) {
                    return during < filterCount * Constants.TIME_UPDATE_CYCLE_MS;
                }

                @Override
                public int getFilterLimitCount() {
                    return Constants.FILTER_STACK_LIMIT_COUNT;
                }

                @Override
                public void fallback(List<MethodItem> stack, int size) {
                    MatrixLog.w(TAG, "[fallback] size:%s targetSize:%s stack:%s", size, Constants.MAX_EVIL_METHOD_STACK, stack);
                    List list = stack.subList(0, Constants.MAX_EVIL_METHOD_STACK);
                    stack.clear();
                    stack.addAll(list);
                }
            });

            String stackKey = TraceDataUtils.getTreeKey(stack, Constants.MAX_LIMIT_ANALYSE_STACK_KEY_NUM);
            StringBuilder ss = new StringBuilder()
                    .append("stackSize:").append(stack.size()).append("\n")
                    .append("stackKey:").append(stackKey).append("\n");

            if (traceConfig.isDebug()) {
                TraceDataUtils.TreeNode root = new TraceDataUtils.TreeNode();
                TraceDataUtils.stackToTree(stack, root);
                TraceDataUtils.printTree(root, 0, ss);
            }

            MatrixLog.i(TAG, ss.toString());

        }
    }
}
