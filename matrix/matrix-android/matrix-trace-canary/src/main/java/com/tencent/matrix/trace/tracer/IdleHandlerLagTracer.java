package com.tencent.matrix.trace.tracer;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.MessageQueue;

import androidx.annotation.Nullable;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.report.Issue;
import com.tencent.matrix.trace.TracePlugin;
import com.tencent.matrix.trace.config.SharePluginInfo;
import com.tencent.matrix.trace.config.TraceConfig;
import com.tencent.matrix.trace.constants.Constants;
import com.tencent.matrix.trace.core.AppMethodBeat;
import com.tencent.matrix.trace.util.AppForegroundUtil;
import com.tencent.matrix.trace.util.Utils;
import com.tencent.matrix.util.DeviceUtil;
import com.tencent.matrix.util.MatrixLog;

import org.json.JSONObject;

import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

public class IdleHandlerLagTracer extends Tracer {

    private static final String TAG = "Matrix.AnrTracer";
    private final TraceConfig traceConfig;
    private static HandlerThread idleHandlerLagHandlerThread;
    private static Handler idleHandlerLagHandler;
    private static Runnable idleHanlderLagRunnable;

    public IdleHandlerLagTracer(TraceConfig traceConfig) {
        this.traceConfig = traceConfig;
    }

    @Override
    public void onAlive() {
        super.onAlive();
        if (traceConfig.isIdleHandlerEnable()) {
            idleHandlerLagHandlerThread = new HandlerThread("IdleHandlerLagThread");
            idleHanlderLagRunnable = new IdleHandlerLagRunable();
            detectIdleHandler();
        }
    }

    @Override
    public void onDead() {
        super.onDead();
        if (traceConfig.isIdleHandlerEnable()) {
            idleHandlerLagHandler.removeCallbacksAndMessages(null);
        }
    }

    static class IdleHandlerLagRunable implements Runnable {
        @Override
        public void run() {
            try {
                TracePlugin plugin = Matrix.with().getPluginByClass(TracePlugin.class);
                if (null == plugin) {
                    return;
                }

                String stackTrace = Utils.getMainThreadJavaStackTrace();
                boolean currentForeground = AppForegroundUtil.isInterestingToUser();
                String scene = AppMethodBeat.getVisibleScene();

                JSONObject jsonObject = new JSONObject();
                jsonObject = DeviceUtil.getDeviceInfo(jsonObject, Matrix.with().getApplication());
                jsonObject.put(SharePluginInfo.ISSUE_STACK_TYPE, Constants.Type.LAG_IDLE_HANDLER);
                jsonObject.put(SharePluginInfo.ISSUE_SCENE, scene);
                jsonObject.put(SharePluginInfo.ISSUE_THREAD_STACK, stackTrace);
                jsonObject.put(SharePluginInfo.ISSUE_PROCESS_FOREGROUND, currentForeground);

                Issue issue = new Issue();
                issue.setTag(SharePluginInfo.TAG_PLUGIN_EVIL_METHOD);
                issue.setContent(jsonObject);
                plugin.onDetectIssue(issue);
                MatrixLog.e(TAG, "happens idle handler Lag : %s ", jsonObject.toString());


            } catch (Throwable t) {
                MatrixLog.e(TAG, "Matrix error, error = " + t.getMessage());
            }

        }
    }

    private static void detectIdleHandler() {
        try {
            MessageQueue mainQueue = Looper.getMainLooper().getQueue();
            Field field = MessageQueue.class.getDeclaredField("mIdleHandlers");
            field.setAccessible(true);
            MyArrayList<MessageQueue.IdleHandler> myIdleHandlerArrayList = new MyArrayList<>();
            field.set(mainQueue, myIdleHandlerArrayList);
            idleHandlerLagHandlerThread.start();
            idleHandlerLagHandler = new Handler(idleHandlerLagHandlerThread.getLooper());
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }


    static class MyIdleHandler implements MessageQueue.IdleHandler {
        private MessageQueue.IdleHandler idleHandler;

        MyIdleHandler(MessageQueue.IdleHandler idleHandler) {
            this.idleHandler = idleHandler;
        }

        @Override
        public boolean queueIdle() {
            idleHandlerLagHandler.postDelayed(idleHanlderLagRunnable, Constants.DEFAULT_IDLE_HANDLER_LAG);
            boolean ret = this.idleHandler.queueIdle();
            idleHandlerLagHandler.removeCallbacks(idleHanlderLagRunnable);
            return ret;
        }
    }

    static class MyArrayList<T> extends ArrayList {
        Map<MessageQueue.IdleHandler, MyIdleHandler> map = new HashMap<>();

        @Override
        public boolean add(Object o) {
            if (o instanceof MessageQueue.IdleHandler) {
                MyIdleHandler myIdleHandler = new MyIdleHandler((MessageQueue.IdleHandler) o);
                map.put((MessageQueue.IdleHandler) o, myIdleHandler);
                return super.add(myIdleHandler);
            }
            return super.add(o);
        }

        @Override
        public boolean remove(@Nullable Object o) {
            if (o instanceof MyIdleHandler) {
                MessageQueue.IdleHandler idleHandler = ((MyIdleHandler) o).idleHandler;
                map.remove(idleHandler);
                return super.remove(o);
            } else {
                MyIdleHandler myIdleHandler = map.remove(o);
                if (myIdleHandler != null) {
                    return super.remove(myIdleHandler);
                }
                return super.remove(o);
            }
        }
    }

}

