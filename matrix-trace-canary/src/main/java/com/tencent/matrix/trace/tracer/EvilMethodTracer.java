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

package com.tencent.matrix.trace.tracer;

import android.app.Activity;
import android.os.Handler;
import android.os.HandlerThread;

import com.tencent.matrix.trace.TracePlugin;
import com.tencent.matrix.trace.config.SharePluginInfo;
import com.tencent.matrix.trace.config.TraceConfig;
import com.tencent.matrix.trace.constants.Constants;
import com.tencent.matrix.trace.schedule.LazyScheduler;
import com.tencent.matrix.trace.util.ViewUtil;
import com.tencent.matrix.util.DeviceUtil;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.ListIterator;

/**
 * Created by caichongyang on 2017/6/5.
 * <p>
 * this class is responsible for finding and reporting evil method which costs lots of times
 * as well as ANR during processing.
 * </p>
 * <p> It will be alive when the app stays at foreground otherwise it is invalid.
 * </p>
 */

public class EvilMethodTracer extends BaseTracer implements LazyScheduler.ILazyTask {

    private static final String TAG = "Matrix.EvilMethodTracer";
    private final TraceConfig mTraceConfig;

    private final LazyScheduler mLazyScheduler;
    private final HashMap<Integer, ActivityCreatedInfo> mActivityCreatedInfoMap;

    private HandlerThread mAnalyseThread;
    private Handler mHandler;
    private volatile boolean isIgnoreFrame;


    public enum Type {
        NORMAL, ENTER, ANR, FULL, STARTUP
    }

    public EvilMethodTracer(TracePlugin plugin, TraceConfig config) {
        super(plugin);
        this.mTraceConfig = config;
        mLazyScheduler = new LazyScheduler(MatrixHandlerThread.getDefaultHandlerThread(), Constants.DEFAULT_ANR);
        mActivityCreatedInfoMap = new HashMap<>();
    }

    @Override
    protected boolean isEnableMethodBeat() {
        return true;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        if (!getMethodBeat().isRealTrace()) {
            MatrixLog.e(TAG, "MethodBeat don't work, maybe it's wrong in trace Build!");
            onDestroy();
            return;
        }
        if (this.mAnalyseThread == null) {
            this.mAnalyseThread = MatrixHandlerThread.getNewHandlerThread("matrix_trace_analyse_thread");
            mHandler = new Handler(mAnalyseThread.getLooper());
        }
        // set up when onCreate
        mLazyScheduler.cancel();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (null != mAnalyseThread) {
            mHandler.removeCallbacksAndMessages(null);
            mAnalyseThread.quit();
            mHandler = null;
            mAnalyseThread = null;
        }

        mLazyScheduler.cancel();
        mActivityCreatedInfoMap.clear();
    }

    /**
     * when the device's Vsync is coming,it will be called.
     * use {@link com.tencent.matrix.trace.core.FrameBeat}
     *
     * @param lastFrameNanos
     * @param frameNanos
     */
    @Override
    public void doFrame(long lastFrameNanos, long frameNanos) {
        if (isIgnoreFrame) {
            mActivityCreatedInfoMap.clear();
            setIgnoreFrame(false);
            getMethodBeat().resetIndex();
            return;
        }

        int index = getMethodBeat().getCurIndex();
        if (frameNanos - lastFrameNanos > mTraceConfig.getEvilThresholdNano()) {
            MatrixLog.e(TAG, "[doFrame] dropped frame too much! lastIndex:%s index:%s", 0, index);
            handleBuffer(Type.NORMAL, 0, index - 1, getMethodBeat().getBuffer(), (frameNanos - lastFrameNanos) / Constants.TIME_MILLIS_TO_NANO);
        }
        getMethodBeat().resetIndex();
        mLazyScheduler.cancel();
        mLazyScheduler.setUp(this, false);

    }

    private void setIgnoreFrame(boolean ignoreFrame) {
        isIgnoreFrame = ignoreFrame;
    }

    /**
     * when the ANR happened,this method will be called.
     */
    @Override
    public void onTimeExpire() {
        // maybe ANR
        if (isBackground()) {
            MatrixLog.w(TAG, "[onTimeExpire] pass this time, on Background!");
            return;
        }
        long happenedAnrTime = getMethodBeat().getCurrentDiffTime();
        MatrixLog.w(TAG, "[onTimeExpire] maybe ANR!");
        setIgnoreFrame(true);
        getMethodBeat().lockBuffer(false);
        handleBuffer(Type.ANR, 0, getMethodBeat().getCurIndex() - 1, getMethodBeat().getBuffer(), null, Constants.DEFAULT_ANR, happenedAnrTime, -1);
    }

    /**
     * when the buffer is full,this method will be called.
     *
     * @param start
     * @param end
     * @param buffer
     */
    @Override
    public void pushFullBuffer(int start, int end, long[] buffer) {
        long now = System.nanoTime() / Constants.TIME_MILLIS_TO_NANO - getMethodBeat().getLastDiffTime();
        MatrixLog.w(TAG, "[pushFullBuffer] now:%s diffTime:%s", now, getMethodBeat().getLastDiffTime());
        setIgnoreFrame(true);
        getMethodBeat().lockBuffer(false);
        handleBuffer(Type.FULL, start, end, buffer, now - (buffer[0] & 0x7FFFFFFFFFFL));
        mLazyScheduler.cancel();
    }

    /**
     * when the activity's onWindowFocusChanged method exec,it will be called.
     *
     * @param activity now activity
     * @param isFocus  whether this activity has focus
     * @param nowIndex the index of buffer when this method was called
     * @param buffer   trace buffer
     */
    @Override
    public void onActivityEntered(Activity activity, boolean isFocus, int nowIndex, long[] buffer) {
        MatrixLog.i(TAG, "[onActivityEntered] activity:%s hashCode:%s isFocus:%s nowIndex:%s", activity.getClass().getSimpleName(), activity.hashCode(), isFocus, nowIndex);
        if (isFocus && mActivityCreatedInfoMap.containsKey(activity.hashCode())) {
            long now = System.currentTimeMillis();
            ActivityCreatedInfo createdInfo = mActivityCreatedInfoMap.get(activity.hashCode());
            long cost = now - createdInfo.startTimestamp;
            MatrixLog.i(TAG, "[activity load time] activity name:%s cost:%sms", activity.getClass(), cost);
            if (cost >= mTraceConfig.getLoadActivityThresholdMs()) {
                ViewUtil.ViewInfo viewInfo = ViewUtil.dumpViewInfo(activity.getWindow().getDecorView());
                viewInfo.mActivityName = activity.getClass().getSimpleName();
                MatrixLog.w(TAG, "[onActivityEntered] type:%s cost:%sms index:[%s-%s] viewInfo:%s",
                        viewInfo.mActivityName, cost, createdInfo.index, nowIndex, viewInfo.toString());
                handleBuffer(Type.ENTER, createdInfo.index, nowIndex, buffer, cost, viewInfo);
            }

            getMethodBeat().lockBuffer(false);
        }
        mActivityCreatedInfoMap.remove(activity.hashCode());
    }

    @Override
    public void onActivityCreated(Activity activity) {
        MatrixLog.i(TAG, "[onActivityCreated] activity:%s hashCode:%s", activity.getClass().getSimpleName(), activity.hashCode());
        super.onActivityCreated(activity);
        getMethodBeat().lockBuffer(true);
        mActivityCreatedInfoMap.put(activity.hashCode(), new ActivityCreatedInfo(System.currentTimeMillis(), Math.max(0, getMethodBeat().getCurIndex() - 1)));
    }

    @Override
    public void onActivityPause(Activity activity) {
        super.onActivityPause(activity);
        MatrixLog.i(TAG, "[onActivityPause] activity:%s hashCode:%s", activity.getClass().getSimpleName(), activity.hashCode());
        mActivityCreatedInfoMap.remove(activity.hashCode());
    }

    @Override
    protected String getTag() {
        return SharePluginInfo.TAG_PLUGIN_EVIL_METHOD;
    }

    @Override
    public void onBackground(Activity activity) {
        super.onBackground(activity);
        mLazyScheduler.cancel();
    }

    @Override
    public void cancelFrame() {
        super.cancelFrame();
        mLazyScheduler.cancel();
    }

    private void handleBuffer(Type type, int start, int end, long[] buffer, long cost) {
        long happenedTime = System.nanoTime() / Constants.TIME_MILLIS_TO_NANO - getMethodBeat().getLastDiffTime();
        handleBuffer(type, start, end, buffer, null, cost, happenedTime, -1);
    }

    public void handleBuffer(Type type, int start, int end, long[] buffer, long cost, int subType) {
        long happenedTime = System.nanoTime() / Constants.TIME_MILLIS_TO_NANO - getMethodBeat().getLastDiffTime();
        handleBuffer(type, start, end, buffer, null, cost, happenedTime, subType);
    }

    private void handleBuffer(Type type, int start, int end, long[] buffer, long cost, ViewUtil.ViewInfo viewInfo) {
        long happenedTime = System.nanoTime() / Constants.TIME_MILLIS_TO_NANO - getMethodBeat().getLastDiffTime();
        handleBuffer(type, start, end, buffer, viewInfo, cost, happenedTime, -1);
    }

    /**
     * copy data and post data to analyse including report result
     * [start:end]
     *
     * @param type
     * @param start
     * @param end
     * @param buffer
     * @param viewInfo
     * @param cost
     */
    private void handleBuffer(Type type, int start, int end, long[] buffer, ViewUtil.ViewInfo viewInfo, long cost, long happenTime, int subType) {
        if (null == buffer) {
            MatrixLog.e(TAG, "null == buffer");
            return;
        }
        if (cost < 0 || cost >= Constants.MAX_EVIL_METHOD_COST) {
            MatrixLog.e(TAG, "[analyse] trace cost invalid:%d", cost);
            return;
        }
        start = Math.max(0, start);
        end = Math.min(buffer.length - 1, end);
        if (start <= end) {
            long[] tmp = new long[end - start + 1];
            System.arraycopy(buffer, start, tmp, 0, end - start + 1);

            if (null != mHandler) {
                AnalyseExtraInfo info = new AnalyseExtraInfo(type, viewInfo, cost, happenTime);
                info.setSubType(subType);
                mHandler.post(new AnalyseTask(tmp, info));
            }
        }
    }

    /**
     * about AnalyseInfo class
     */
    private static final class AnalyseExtraInfo {
        Type type;
        int subType;
        ViewUtil.ViewInfo viewInfo;
        long cost;
        long happenTime;

        AnalyseExtraInfo(Type type, ViewUtil.ViewInfo viewInfo, long cost, long happenTime) {
            this.type = type;
            this.viewInfo = viewInfo;
            this.cost = cost;
            this.happenTime = happenTime;
        }

        public void setSubType(int type) {
            this.subType = type;
        }

    }

    /**
     * this class for analyse the trace info as well as reporting result.
     */
    private final class AnalyseTask implements Runnable {

        private final long[] buffer;
        private final AnalyseExtraInfo analyseExtraInfo;

        private AnalyseTask(long[] buffer, AnalyseExtraInfo analyseExtraInfo) {
            this.buffer = buffer;
            this.analyseExtraInfo = analyseExtraInfo;
        }

        private long getTime(long trueId) {
            return trueId & 0x7FFFFFFFFFFL;
        }

        private int getMethodId(long trueId) {
            return (int) ((trueId >> 43) & 0xFFFFFL);
        }

        private boolean isIn(long trueId) {
            return ((trueId >> 63) & 0x1) == 1;
        }

        @Override
        public void run() {
            analyse(buffer);
        }

        private void analyse(long[] buffer) {
            LinkedList<Long> rawData = new LinkedList<>();
            LinkedList<MethodItem> resultStack = new LinkedList<>();

            for (long trueId : buffer) {
                if (isIn(trueId)) {
                    rawData.push(trueId);
                } else {
                    int methodId = getMethodId(trueId);
                    if (!rawData.isEmpty()) {
                        long in = rawData.pop();
                        while (getMethodId(in) != methodId) {
                            MatrixLog.w(TAG, "[analyse] method[%s] not match in! continue to find!", methodId);
                            //complete out for not found in
                            long outTime = analyseExtraInfo.happenTime;
                            long inTime = getTime(in);
                            long during = outTime - inTime;
                            if (during < 0 || during >= Constants.MAX_EVIL_METHOD_DUR_TIME) {
                                MatrixLog.e(TAG, "[analyse] trace during invalid:%d", during);
                                return;
                            }

                            int methodId2 = getMethodId(in);
                            MethodItem methodItem = new MethodItem(methodId2, (int) during, rawData.size());
                            addMethodItem(resultStack, methodItem);

                            if (!rawData.isEmpty()) {
                                in = rawData.pop();
                            } else {
                                MatrixLog.e(TAG, "[analyse] method[%s] not match in finally! ", methodId);
                                in = 0;
                                break;
                            }
                        }
                        long outTime = getTime(trueId);
                        long inTime = getTime(in);
                        long during = outTime - inTime;
                        if (during < 0 || during >= Constants.MAX_EVIL_METHOD_DUR_TIME) {
                            MatrixLog.e(TAG, "[analyse] trace during invalid:%d", during);
                            return;
                        }

                        MethodItem methodItem = new MethodItem(methodId, (int) during, rawData.size());
                        addMethodItem(resultStack, methodItem);

                    } else {
                        MatrixLog.w(TAG, "[analyse] method[%s] not found in! ", methodId);
                    }
                }
            }

            // maybe ANR or Full
            while (!rawData.isEmpty()) {
                MatrixLog.w(TAG, "[analyse] has never out method. rawData size:%s result size:%s", rawData.size(), resultStack.size());
                long trueId = rawData.pop();
                int methodId = getMethodId(trueId);
                long inTime = getTime(trueId);
                MethodItem methodItem = new MethodItem(methodId, Math.min((int) (analyseExtraInfo.happenTime - inTime), Constants.DEFAULT_ANR), rawData.size());
                addMethodItem(resultStack, methodItem);
            }
            MatrixLog.i(TAG, "resultStack:%s", resultStack);
            LinkedList<MethodItem> finalResultStack = new LinkedList<>();
            TreeNode root = stackToTree(resultStack);
            int round = 1;
            while (true) {
                boolean ret = trimResultStack(root, analyseExtraInfo, 0, (.1f * (float) round));
                if (ret) {
                    MatrixLog.e(TAG, "type:%s [stack result is empty after trim, just ignore]", analyseExtraInfo.type.name());
                    return;
                }
                if (countTreeNode(root) <= Constants.MAX_EVIL_METHOD_STACK) {
                    break;
                }

                round++;
                if (round > 3) {
                    break;
                }
            }

            makeKey(root, analyseExtraInfo);

            preTraversalTree(root, finalResultStack);
//            trimResultStack(finalResultStack, analyseExtraInfo);
            if (finalResultStack.isEmpty()) {
                MatrixLog.e(TAG, "type:%s [stack result is empty after trim, just ignore]", analyseExtraInfo.type.name());
                return;
            }
            ListIterator<MethodItem> listIterator = finalResultStack.listIterator();
            StringBuilder printBuilder = new StringBuilder("\n"); // print to logcat
            StringBuilder reportBuilder = new StringBuilder(); // report result
            while (listIterator.hasNext()) {
                MethodItem item = listIterator.next();
                printBuilder.append(item.print()).append('\n');
                reportBuilder.append(item.toString()).append('\n');
            }
            MatrixLog.i(TAG, "[analyse result] %s", printBuilder.toString());

            StringBuilder keyBuilder = new StringBuilder(); // key for cache
            getKey(root, keyBuilder);
            String key = keyBuilder.toString();
            if (analyseExtraInfo.type == Type.FULL) {
                key = "TypeFull";
            } else if (key.length() == 0) {
                MatrixLog.e(TAG, "get key null, type:%s", analyseExtraInfo.type.name());
                return;
            }
            MatrixLog.i(TAG, "[analyse key] %s", key);

            // report
            try {
                JSONObject jsonObject = new JSONObject();
                jsonObject = DeviceUtil.getDeviceInfo(jsonObject, getPlugin().getApplication());

                jsonObject.put(SharePluginInfo.ISSUE_STACK_TYPE, analyseExtraInfo.type.name());
                jsonObject.put(SharePluginInfo.ISSUE_SUB_TYPE, analyseExtraInfo.subType);
                jsonObject.put(SharePluginInfo.ISSUE_COST, analyseExtraInfo.cost);

                if (analyseExtraInfo.type == Type.ENTER) {
                    JSONObject viewInfoJson = new JSONObject();
                    ViewUtil.ViewInfo viewInfo = analyseExtraInfo.viewInfo;
                    viewInfoJson.put(SharePluginInfo.ISSUE_VIEW_DEEP, null == viewInfo ? 0 : viewInfo.mViewDeep);
                    viewInfoJson.put(SharePluginInfo.ISSUE_VIEW_COUNT, null == viewInfo ? 0 : viewInfo.mViewCount);
                    viewInfoJson.put(SharePluginInfo.ISSUE_VIEW_ACTIVITY, null == viewInfo ? 0 : viewInfo.mActivityName);
                    jsonObject.put(SharePluginInfo.ISSUE_VIEW_INFO, viewInfoJson);
                }
                jsonObject.put(SharePluginInfo.ISSUE_STACK, reportBuilder.toString());
                jsonObject.put(SharePluginInfo.ISSUE_STACK_KEY, key);
                sendReport(jsonObject);

            } catch (JSONException e) {
                MatrixLog.e(TAG, "[JSONException for stack %s, error: %s", reportBuilder.toString(), e);
            }
        }
    }

    private void addMethodItem(LinkedList<MethodItem> resultStack, MethodItem methodItem) {
        MethodItem lastMethodItem = null;
        if (!resultStack.isEmpty()) {
            lastMethodItem = resultStack.peek();
        }
        if (null != lastMethodItem && lastMethodItem.equals(methodItem)) {
            lastMethodItem.addMore(methodItem.durTime);
        } else {
            resultStack.push(methodItem);
        }
    }

    private int countTreeNode(TreeNode node) {
        int count = node.mChildNodes.size();
        Iterator<TreeNode> iterator = node.mChildNodes.iterator();
        while (iterator.hasNext()) {
            count += countTreeNode(iterator.next());
        }
        return count;
    }

    private void makeKey(TreeNode node, AnalyseExtraInfo analyseExtraInfo) {
        if (analyseExtraInfo.type == Type.FULL) {
            return;
        }

        long totalCost = analyseExtraInfo.cost;
        Iterator<TreeNode> iterator = node.mChildNodes.iterator();
        while (iterator.hasNext()) {
            TreeNode tmpNode = iterator.next();
            if (null == tmpNode || null == tmpNode.mItem) {
                MatrixLog.e(TAG, "Null Tree Node, Must check.");
                continue;
            }

            long thresold = 0;
            if (tmpNode.mItem.depth == 0) {
                thresold = (long) (totalCost * Constants.MAX_ANALYSE_METHOD_ALL_PERCENT);
            } else if (tmpNode.mFather != null && tmpNode.mFather.mItem != null) {
                thresold = (long) (tmpNode.mFather.mItem.durTime * Constants.MAX_ANALYSE_METHOD_NEXT_PERCENT);
            } else {
                thresold = (long) (totalCost * Constants.MAX_ANALYSE_METHOD_ALL_PERCENT);
            }
            if (tmpNode.mItem.durTime >= thresold) {
                if (tmpNode.mItem.depth > 0) {
                    mAnalyseStackList.pop();
                    mAnalyseStackList.push(tmpNode.mItem);
                    makeKey(tmpNode, analyseExtraInfo);
                    return; //no need to analyse other childs
                }
                mAnalyseStackList.push(tmpNode.mItem);
                makeKey(tmpNode, analyseExtraInfo);
            }
        }

        return;
    }

    private void getKey(TreeNode root, StringBuilder builder) {
        if (mAnalyseStackList.isEmpty()) {
            Iterator<TreeNode> iterator = root.mChildNodes.iterator();
            while (iterator.hasNext()) {
                mAnalyseStackList.add(iterator.next().mItem);
            }
        }

        if (mAnalyseStackList.size() > Constants.MAX_LIMIT_ANALYSE_STACK_KEY_NUM) {
            mAnalyseStackList.subList(0, Constants.MAX_LIMIT_ANALYSE_STACK_KEY_NUM);
        }

        for (MethodItem methodItem : mAnalyseStackList) {
            builder.append(methodItem.methodId);
            builder.append('\n');
        }
        mAnalyseStackList.clear();
    }

    /**
     * Simplify method stack
     *
     * @param node
     * @param analyseInfo
     * @return true:trim current node
     */
    private boolean trimResultStack(TreeNode node, AnalyseExtraInfo analyseInfo, long parentCost, float round) {
        long durTime = node.mItem == null ? analyseInfo.cost : node.mItem.durTime;
        if (node.mItem == null) {    //root node
            if (analyseInfo.type == Type.ENTER && durTime < mTraceConfig.getLoadActivityThresholdMs()) {
                MatrixLog.w(TAG, "trimResultStack analyse enter type, max cost: %dms less than threshold: %dms, just ignore",
                        durTime, mTraceConfig.getLoadActivityThresholdMs());
                node.mChildNodes.clear();
                return true;
            }
        }

        if (durTime <= (analyseInfo.cost / 20)) { //only keep 5% of total cost node
            return true;
        }

        long minCost = (long) (parentCost * round);  //will be 0.1, 0.2, 0.3
        if (durTime <= minCost) {
            node.mChildNodes.clear();
        }

        Iterator<TreeNode> iterator = node.mChildNodes.iterator();
        while (iterator.hasNext()) {
            TreeNode tmpNode = iterator.next();
            boolean ret = trimResultStack(tmpNode, analyseInfo, durTime, round);
            if (ret) {
                iterator.remove();
            }
        }

        return false;
    }


    private static final class MethodItem {
        int methodId;
        int durTime;
        int depth;
        int count = 1;

        MethodItem(int methodId, int durTime, int depth) {
            this.methodId = methodId;
            this.durTime = durTime;
            this.depth = depth;
        }

        @Override
        public String toString() {
            return depth + "," + methodId + "," + count + "," + durTime;
        }

        public String getKey() {
            return depth + "," + methodId + "," + count;
        }

        @Override
        public boolean equals(Object obj) {
            if (obj instanceof MethodItem) {
                MethodItem leftObj = (MethodItem) obj;
                if (leftObj.methodId == methodId && leftObj.depth == depth) {
                    return true;
                }
            }
            return false;
        }

        @Override
        public int hashCode() {
            return super.hashCode();
        }

        public void addMore(long cost) {
            count++;
            durTime += cost;
        }

        public String print() {
            StringBuffer inner = new StringBuffer();
            for (int i = 0; i < depth; i++) {
                inner.append('.');
            }
            return inner.toString() + methodId + " " + count + " " + durTime;
        }
    }

    private static class ActivityCreatedInfo {
        long startTimestamp;
        int index;

        private ActivityCreatedInfo(long startTimestamp, int index) {
            this.startTimestamp = startTimestamp;
            this.index = index;
        }
    }

    /**
     * it's the node for the stack tree
     */
    private static final class TreeNode {
        MethodItem mItem;
        TreeNode mFather;

        LinkedList<TreeNode> mChildNodes = new LinkedList<>();

        TreeNode(MethodItem item, TreeNode father) {
            this.mItem = item;
            this.mFather = father;
        }

        private int depth() {
            if (null == mItem) {
                return 0;
            } else {
                return mItem.depth;
            }
        }

        private void push(TreeNode node) {
            mChildNodes.push(node);
        }

        private boolean isLeaf() {
            return mChildNodes.isEmpty();
        }
    }

    /**
     * Structured the method stack as a tree Data structure
     *
     * @param resultStack
     * @return
     */
    private TreeNode stackToTree(LinkedList<MethodItem> resultStack) {
        TreeNode root = new TreeNode(null, null);
        TreeNode lastNode = null;
        ListIterator<MethodItem> iterator = resultStack.listIterator(0);

        while (iterator.hasNext()) {
            TreeNode node = new TreeNode(iterator.next(), lastNode);
            if (null == lastNode && node.depth() != 0) {
                MatrixLog.e(TAG, "[stackToTree] begin error! why the frist node'depth is not 0!");
                break;
            }
            // MatrixLog.i(TAG, "node :%s", node.mItem.toString());
            int depth = node.depth();
            if (depth == 0) {
                root.push(node);
            } else if (null != lastNode && lastNode.depth() >= depth) {
                while (lastNode.depth() > depth) {
                    lastNode = lastNode.mFather;
                    // MatrixLog.e(TAG, "node :%s lastNode:%s", node.depth(), lastNode.depth());
                }
                if (lastNode.mFather != null) {
                    node.mFather = lastNode.mFather;
                    lastNode.mFather.push(node);
                }

            } else if (null != lastNode && lastNode.depth() < depth) {
                lastNode.push(node);
            }
            lastNode = node;
        }
        return root;
    }

    private void preTraversalTree(TreeNode rootNode, LinkedList<MethodItem> list) {
        if (null == rootNode || rootNode.isLeaf()) {
            return;
        }
        LinkedList<TreeNode> childNodes = rootNode.mChildNodes;
        while (!childNodes.isEmpty()) {
            TreeNode node = childNodes.pop();
            list.addLast(node.mItem);
            preTraversalTree(node, list);
        }
    }

    private final LinkedList<MethodItem> mAnalyseStackList = new LinkedList<>();

}
