package com.tencent.matrix.trace.util;

import com.tencent.matrix.trace.tracer.EvilMethodTracer;

import java.util.Iterator;
import java.util.List;

public class TraceDataUtils {

    private static final String TAG = "Matrix.TraceDataUtils";

    public interface IStructuredDataFilter {
        boolean isFilter(long during, int filterCount);

        int getFilterMaxCount();

        void fallback(List<EvilMethodTracer.MethodItem> stack, int size);
    }

    public static void trimStack(List<EvilMethodTracer.MethodItem> stack, int targetCount, IStructuredDataFilter filter) {
        if (0 > targetCount) {
            stack.clear();
            return;
        }

        int filterCount = 1;
        int curStackSize = stack.size();
        while (curStackSize > targetCount) {
            Iterator<EvilMethodTracer.MethodItem> iterator = stack.iterator();
            while (iterator.hasNext()) {
                EvilMethodTracer.MethodItem item = iterator.next();
                if (filter.isFilter(item.durTime, filterCount)) {
                    iterator.remove();
                    curStackSize--;
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

}
