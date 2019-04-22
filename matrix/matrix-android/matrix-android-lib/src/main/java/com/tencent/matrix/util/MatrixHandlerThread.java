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

package com.tencent.matrix.util;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.util.Printer;

import com.tencent.matrix.AppForegroundDelegate;
import com.tencent.matrix.listeners.IAppForeground;

import java.util.Collections;
import java.util.Comparator;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Created by zhangshaowen on 17/7/5.
 */

public class MatrixHandlerThread {
    private static final String TAG = "Matrix.HandlerThread";

    public static final String MATRIX_THREAD_NAME = "default_matrix_thread";

    /**
     * unite defaultHandlerThread for lightweight work,
     * if you have heavy work checking, you can create a new thread
     */
    private static volatile HandlerThread defaultHandlerThread;
    private static volatile Handler defaultHandler;
    private static volatile Handler defaultMainHandler = new Handler(Looper.getMainLooper());
    private static HashSet<HandlerThread> handlerThreads = new HashSet<>();
    public static boolean isDebug = false;

    public static Handler getDefaultMainHandler() {
        return defaultMainHandler;
    }

    public static HandlerThread getDefaultHandlerThread() {

        synchronized (MatrixHandlerThread.class) {
            if (null == defaultHandlerThread) {
                defaultHandlerThread = new HandlerThread(MATRIX_THREAD_NAME);
                defaultHandlerThread.start();
                defaultHandler = new Handler(defaultHandlerThread.getLooper());
                defaultHandlerThread.getLooper().setMessageLogging(isDebug ? new LooperPrinter() : null);
                MatrixLog.w(TAG, "create default handler thread, we should use these thread normal, isDebug:%s", isDebug);
            }
            return defaultHandlerThread;
        }
    }

    public static Handler getDefaultHandler() {
        return defaultHandler;
    }

    public static HandlerThread getNewHandlerThread(String name) {
        for (Iterator<HandlerThread> i = handlerThreads.iterator(); i.hasNext();) {
            HandlerThread element = i.next();
            if (!element.isAlive()) {
                i.remove();
                MatrixLog.w(TAG, "warning: remove dead handler thread with name %s", name);
            }
        }
        HandlerThread handlerThread = new HandlerThread(name);
        handlerThread.start();
        handlerThreads.add(handlerThread);
        MatrixLog.w(TAG, "warning: create new handler thread with name %s, alive thread size:%d", name, handlerThreads.size());
        return handlerThread;
    }

    private static final class LooperPrinter implements Printer, IAppForeground {

        private ConcurrentHashMap<String, Info> hashMap = new ConcurrentHashMap<>();
        private boolean isForeground;

        LooperPrinter() {
            AppForegroundDelegate.INSTANCE.addListener(this);
            this.isForeground = AppForegroundDelegate.INSTANCE.isAppForeground();
        }

        @Override
        public void println(String x) {
            if (isForeground) {
                return;
            }
            if (x.charAt(0) == '>') {
                int start = x.indexOf("to ") + 3;
                int end = x.lastIndexOf(": ");
                String content = x.substring(start, end);
                Info info = hashMap.get(content);
                if (info == null) {
                    info = new Info();
                    info.key = content;
                    hashMap.put(content, info);
                }
                ++info.count;
            }
        }

        @Override
        public void onForeground(boolean isForeground) {
            this.isForeground = isForeground;

            if (isForeground) {
                long start = System.currentTimeMillis();
                LinkedList<Info> list = new LinkedList<>();
                for (Info info : hashMap.values()) {
                    if (info.count > 1) {
                        list.add(info);
                    }
                }
                Collections.sort(list, new Comparator<Info>() {
                    @Override
                    public int compare(Info o1, Info o2) {
                        return o2.count - o1.count;
                    }
                });
                hashMap.clear();
                if (!list.isEmpty()) {
                    MatrixLog.i(TAG, "matrix default thread has exec in background! %s cost:%s", list, System.currentTimeMillis() - start);
                }
            } else {
                hashMap.clear();
            }
        }

        class Info {
            String key;
            int count;

            @Override
            public String toString() {
                return key + ":" + count;
            }
        }

    }
}
