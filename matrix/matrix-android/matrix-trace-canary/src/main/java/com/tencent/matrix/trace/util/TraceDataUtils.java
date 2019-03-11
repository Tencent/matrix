package com.tencent.matrix.trace.util;

import com.tencent.matrix.trace.constants.Constants;
import com.tencent.matrix.trace.core.AppMethodBeat;
import com.tencent.matrix.trace.items.MethodItem;
import com.tencent.matrix.util.MatrixLog;

import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.ListIterator;

public class TraceDataUtils {

    private static final String TAG = "Matrix.TraceDataUtils";

    public interface IStructuredDataFilter {
        boolean isFilter(long during, int filterCount);

        int getFilterMaxCount();

        void fallback(List<MethodItem> stack, int size);
    }

    public static void structuredDataToStack(long[] buffer, LinkedList<MethodItem> result) {
        structuredDataToStack(buffer, result, true);
    }

    public static void structuredDataToStack(long[] buffer, LinkedList<MethodItem> result, boolean isStrict) {

        LinkedList<Long> rawData = new LinkedList<>();
        boolean isBegin = !isStrict;
        for (long trueId : buffer) {
            if (0 == trueId) {
                continue;
            }
            if (isStrict) {
                if (isIn(trueId) && AppMethodBeat.METHOD_ID_DISPATCH == getMethodId(trueId)) {
                    isBegin = true;
                }

                if (!isBegin) {
                    MatrixLog.d(TAG, "never begin! pass this method[%s]", getMethodId(trueId));
                    continue;
                }
            }

            if (isIn(trueId)) {
                rawData.push(trueId);
                int methodId = getMethodId(trueId); // in
                MatrixLog.i(TAG, "in :%s", methodId);
            } else {
                int methodId = getMethodId(trueId); // out
                MatrixLog.i(TAG, "out :%s", methodId);
                if (!rawData.isEmpty()) {
                    long in = rawData.pop();
                    while (getMethodId(in) != methodId) {
                        MatrixLog.w(TAG, "[structuredDataToStack] method[%s] not match in! pop[%s] to continue find!", in, methodId);
                        in = rawData.pop();
                    }

                    long outTime = getTime(trueId);
                    long inTime = getTime(in);
                    long during = outTime - inTime;
                    if (during < 0) {
                        MatrixLog.e(TAG, "[structuredDataToStack] trace during invalid:%d", during);
                        rawData.clear();
                        result.clear();
                        return;
                    }
                    if (during < Constants.TIME_UPDATE_CYCLE_MS) {
                        continue;
                    }
                    MethodItem methodItem = new MethodItem(methodId, (int) during, rawData.size());
                    addMethodItem(result, methodItem);
                } else {
                    MatrixLog.w(TAG, "[structuredDataToStack] method[%s] not found in! ", methodId);
                }
            }
        }

        while (!rawData.isEmpty()) {
            long trueId = rawData.pop();
            int methodId = getMethodId(trueId);
            boolean isIn = isIn(trueId);
            MatrixLog.w(TAG, "[structuredDataToStack] has never out method[%s], isIn:%s, rawData size:%s", methodId, isIn, rawData.size());
            if (!isIn) {
                MatrixLog.e(TAG, "[structuredDataToStack] why has out Method[%s]? is wrong! ", methodId);
                continue;
            }
            MethodItem methodItem = new MethodItem(methodId, Constants.DEFAULT_ANR, rawData.size());
            addMethodItem(result, methodItem);
        }
    }

    private static boolean isIn(long trueId) {
        return ((trueId >> 63) & 0x1) == 1;
    }

    private static long getTime(long trueId) {
        return trueId & 0x7FFFFFFFFFFL;
    }

    private static int getMethodId(long trueId) {
        return (int) ((trueId >> 43) & 0xFFFFFL);
    }

    private static void addMethodItem(LinkedList<MethodItem> resultStack, MethodItem item) {
        MethodItem last = null;
        if (!resultStack.isEmpty()) {
            last = resultStack.peek();
        }
        resultStack.push(item);
//        if (null != last && last.methodId == item.methodId && last.depth == item.depth) {
//            last.mergeMore(item.durTime);
//        } else {
//            resultStack.push(item);
//        }
    }

    /**
     * Structured the method stack as a tree Data structure
     *
     * @param resultStack
     * @return
     */
    public static int stackToTree(LinkedList<MethodItem> resultStack, TreeNode root) {
        TreeNode lastNode = null;
        ListIterator<MethodItem> iterator = resultStack.listIterator(0);
        int count = 0;
        while (iterator.hasNext()) {
            TreeNode node = new TreeNode(iterator.next(), lastNode);
            count++;
            if (null == lastNode && node.depth() != 0) {
                MatrixLog.e(TAG, "[stackToTree] begin error! why the first node'depth is not 0!");
                return 0;
            }
            int depth = node.depth();
            if (lastNode == null || depth == 0) {
                root.add(node);
            } else if (lastNode.depth() >= depth) {
                while (lastNode.depth() > depth) {
                    lastNode = lastNode.father;
                }
                if (lastNode.father != null) {
                    node.father = lastNode.father;
                    lastNode.father.add(node);
                }
            } else if (lastNode.depth() < depth) {
                lastNode.add(node);
            }
            lastNode = node;
        }
        return count;
    }


    public static long stackToString(LinkedList<MethodItem> stack, StringBuilder reportBuilder, StringBuilder logcatBuilder) {
        logcatBuilder.append("|*   TraceStack:[id count cost] ").append("\n");
        ListIterator<MethodItem> listIterator = stack.listIterator();
        long stackCost = 0; // fix cost
        while (listIterator.hasNext()) {
            MethodItem item = listIterator.next();
            reportBuilder.append(item.toString()).append('\n');
            logcatBuilder.append("|*        ").append(item.print()).append('\n');

            if (stackCost < item.durTime) {
                stackCost = item.durTime;
            }
        }
        return stackCost;
    }


    public static int countTreeNode(TreeNode node) {
        int count = node.children.size();
        Iterator<TreeNode> iterator = node.children.iterator();
        while (iterator.hasNext()) {
            count += countTreeNode(iterator.next());
        }
        return count;
    }

    /**
     * it's the node for the stack tree
     */
    public static final class TreeNode {
        MethodItem item;
        TreeNode father;

        LinkedList<TreeNode> children = new LinkedList<>();

        TreeNode(MethodItem item, TreeNode father) {
            this.item = item;
            this.father = father;
        }

        public TreeNode() {

        }

        private int depth() {
            return null == item ? 0 : item.depth;
        }

        private void add(TreeNode node) {
            children.addFirst(node);
        }

        private boolean isLeaf() {
            return children.isEmpty();
        }
    }

    public static void printTree(TreeNode root, StringBuilder print) {
        print.append("|*   TraceStack: ").append("\n");
        printTree(root, 0, print, "|*        ");
    }

    public static void printTree(TreeNode root, int depth, StringBuilder ss, String prefixStr) {

        StringBuilder empty = new StringBuilder(prefixStr);

        for (int i = 0; i <= depth; i++) {
            empty.append("    ");
        }
        for (int i = 0; i < root.children.size(); i++) {
            TreeNode node = root.children.get(i);
            ss.append(empty.toString()).append(node.item.methodId).append("[").append(node.item.durTime).append("]").append("\n");
            if (!node.children.isEmpty()) {
                printTree(node, depth + 1, ss, prefixStr);
            }
        }
    }


    public static void trimStack(List<MethodItem> stack, int targetCount, IStructuredDataFilter filter) {
        if (0 > targetCount) {
            stack.clear();
            return;
        }

        int filterCount = 1;
        int curStackSize = stack.size();
        while (curStackSize > targetCount) {
            Iterator<MethodItem> iterator = stack.iterator();
            while (iterator.hasNext()) {
                MethodItem item = iterator.next();
                if (filter.isFilter(item.durTime, filterCount)) {
                    iterator.remove();
                    curStackSize--;
                    if (curStackSize <= targetCount) {
                        return;
                    }
                }
            }
            curStackSize = stack.size();
            filterCount++;
            if (filter.getFilterMaxCount() < filterCount) {
                break;
            }
        }
        int size = stack.size();
        if (size > targetCount) {
            filter.fallback(stack, size);
        }
    }

    public static String getTreeKey(List<MethodItem> stack, final int targetCount) {
        StringBuilder ss = new StringBuilder();
        final List<MethodItem> tmp = new LinkedList<>(stack);
        trimStack(tmp, targetCount, new TraceDataUtils.IStructuredDataFilter() {
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
                MatrixLog.w(TAG, "[getTreeKey] size:%s targetSize:%s stack:%s", size, targetCount, tmp);
                List list = tmp.subList(0, targetCount);
                tmp.clear();
                tmp.addAll(list);
            }
        });
        for (MethodItem item : tmp) {
            ss.append(item.methodId + "|");
        }
        return ss.toString();
    }

    /**
     * for Test
     */

    private static final int TEST_LENGTH = 16;

    public static void testStructuredDataToTree() {
        long[] data = new long[TEST_LENGTH];
        data[0] = testMergeData(1, true, 0);
        data[1] = testMergeData(2, true, 0);
        data[2] = testMergeData(3, true, 0);
        data[3] = testMergeData(4, true, 0);
        data[4] = testMergeData(4, false, 10);
        data[5] = testMergeData(3, false, 20);
        data[6] = testMergeData(5, true, 35);
        data[7] = testMergeData(6, true, 35);
        data[8] = testMergeData(6, false, 50);
        data[9] = testMergeData(7, true, 55);
        data[10] = testMergeData(7, false, 70);
        data[11] = testMergeData(5, false, 80);
        data[12] = testMergeData(2, false, 70);
        data[13] = testMergeData(8, true, 90);
        data[14] = testMergeData(8, false, 105);
        data[15] = testMergeData(1, false, 200);
        TreeNode root = new TreeNode();
        LinkedList<MethodItem> stack = new LinkedList();
        structuredDataToStack(data, stack);
        String stackKey = getTreeKey(stack, 5);

        stackToTree(stack, root);
        MatrixLog.i(TAG, "[testStructuredDataToTree] stackKey:%s size:%s count:%s", stackKey, stack.size(), countTreeNode(root));
        /**
         /*    1[200]
         /*        2[70]
         /*            3[20]
         /*                4[10]
         /*            5[45]
         /*                6[15]
         /*                7[15]
         /*        8[15]
         **/
        StringBuilder ss = new StringBuilder("print tree\n");
        printTree(root, ss);
        MatrixLog.i(TAG, ss.toString());
    }

    private static long testMergeData(int methodId, boolean isIn, long time) {
        long trueId = 0L;
        if (isIn) {
            trueId |= 1L << 63;
        }
        trueId |= (long) methodId << 43;
        trueId |= time & 0x7FFFFFFFFFFL;
        return trueId;
    }


}
