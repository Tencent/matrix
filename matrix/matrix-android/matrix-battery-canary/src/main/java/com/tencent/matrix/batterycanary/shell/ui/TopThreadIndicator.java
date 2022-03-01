package com.tencent.matrix.batterycanary.shell.ui;

import android.animation.PropertyValuesHolder;
import android.animation.ValueAnimator;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.graphics.PixelFormat;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.Process;
import android.provider.Settings;
import android.util.DisplayMetrics;
import android.util.SparseBooleanArray;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.view.animation.AccelerateInterpolator;
import android.widget.LinearLayout;
import android.widget.PopupMenu;
import android.widget.TextView;
import android.widget.Toast;

import com.tencent.matrix.batterycanary.BatteryCanary;
import com.tencent.matrix.batterycanary.R;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCallback;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature.JiffiesSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Delta;
import com.tencent.matrix.batterycanary.shell.TopThreadFeature;
import com.tencent.matrix.batterycanary.shell.TopThreadFeature.ContinuousCallback;
import com.tencent.matrix.batterycanary.stats.BatteryStatsFeature;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.batterycanary.utils.CallStackCollector;
import com.tencent.matrix.batterycanary.utils.Consumer;
import com.tencent.matrix.util.MatrixLog;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.UiThread;
import androidx.core.util.Pair;

import static com.tencent.matrix.batterycanary.shell.TopThreadFeature.figureCupLoad;
import static com.tencent.matrix.batterycanary.shell.TopThreadFeature.fixedColumn;

/**
 * See {@link TopThreadFeature}.
 *
 * @author Kaede
 * @since 2022/2/23
 */
final public class TopThreadIndicator {
    private static final String TAG = "Matrix.TopThreadIndicator";
    private static final int MAX_PROC_NUM = 10;
    private static final int MAX_THREAD_NUM = 10;
    @SuppressLint("StaticFieldLeak")
    private static final TopThreadIndicator sInstance = new TopThreadIndicator();

    private final DisplayMetrics displayMetrics = new DisplayMetrics();
    private final Handler mUiHandler = new Handler(Looper.getMainLooper());
    private final SparseBooleanArray mRunningRef = new SparseBooleanArray();
    @SuppressLint("RestrictedApi")
    private Pair<Integer, String> mCurrProc = new Pair<>(Process.myPid(), getProcSuffix(BatteryCanaryUtil.getProcessName()));

    @Nullable
    private View mRootView;
    @Nullable
    private Delta<JiffiesSnapshot> mCurrDelta;
    @NonNull
    private CallStackCollector mCollector = new CallStackCollector();
    @NonNull
    private Consumer<Delta<JiffiesSnapshot>> mDumpHandler = new Consumer<Delta<JiffiesSnapshot>>() {
        @Override
        public void accept(final Delta<JiffiesSnapshot> delta) {
            if (delta == null) {
                if (mRootView != null) {
                    Toast.makeText(mRootView.getContext(), "Skip dump: no data", Toast.LENGTH_SHORT).show();
                }
                return;
            }
            if (delta.dlt.pid != Process.myPid()) {
                if (mRootView != null) {
                    Toast.makeText(mRootView.getContext(), "Skip dump: only support curr process now", Toast.LENGTH_SHORT).show();
                }
                return;
            }

            String tag = "TOP_THREAD_DUMP";
            BatteryMonitorCallback.BatteryPrinter.Printer printer = new BatteryMonitorCallback.BatteryPrinter.Printer();
            printer.writeTitle();
            printer.append("| " + tag + "\n");
            if (delta.isValid()) {
                final Map<String, Object> extras = new LinkedHashMap<>();
                int cpuLoad = (int) figureCupLoad(delta.dlt.totalJiffies.get(), delta.during / 10);
                extras.put("load", cpuLoad);
                // Load
                printer.createSection("Proc");
                printer.writeLine("pid", String.valueOf(delta.dlt.pid));
                printer.writeLine("cmm", String.valueOf(delta.dlt.name));
                printer.writeLine("load", cpuLoad + "%");
                printer.createSubSection("Thread(" + delta.dlt.threadEntries.getList().size() + ")");
                printer.writeLine("  TID\tLOAD \tSTATUS \tTHREAD_NAME \tJIFFY");
                for (JiffiesSnapshot.ThreadJiffiesEntry threadJiffies : delta.dlt.threadEntries.getList()) {
                    long entryJffies = threadJiffies.get();
                    printer.append("|   -> ")
                            .append(fixedColumn(String.valueOf(threadJiffies.tid), 5)).append("\t")
                            .append(fixedColumn((int) figureCupLoad(entryJffies, delta.during / 10) + "%", 4)).append("\t")
                            .append(threadJiffies.isNewAdded ? "+" : "~").append("/").append(threadJiffies.stat).append("\t")
                            .append(fixedColumn(threadJiffies.name, 16)).append("\t")
                            .append(entryJffies).append("\t")
                            .append("\n");
                }
                // Thread Stack
                printer.createSection("Stacks");
                for (JiffiesSnapshot.ThreadJiffiesEntry threadJiffies : delta.dlt.threadEntries.getList()) {
                    long entryJffies = threadJiffies.get();
                    int load = (int) figureCupLoad(entryJffies, delta.during / 10);
                    if (load > 0) {
                        printer.createSubSection(threadJiffies.name + "(" + threadJiffies.tid + ")");
                        String stack = mCollector.collect(threadJiffies.tid);
                        extras.put("stack_" + threadJiffies.name + "(" + threadJiffies.tid + ")", stack);
                        int idx = 0;
                        for (String line : stack.split("\n")) {
                            printer.append(idx == 0 ? "|   -> " : "|      ").append(line).append("\n");
                            idx++;
                        }
                    } else {
                        break;
                    }
                }

                // FIXME: remove dependency of BatteryCanary
                BatteryCanary.getMonitorFeature(BatteryStatsFeature.class, new Consumer<BatteryStatsFeature>() {
                    @Override
                    public void accept(BatteryStatsFeature stats) {
                        String event = "MATRIX_TOP_DUMP";
                        int eventId = delta.dlt.pid;
                        stats.statsEvent(event, eventId, extras);
                    }
                });
            } else {
                printer.createSection("Invalid data, ignore");
            }
            printer.writeEnding();
            printer.dump();
            if (mRootView != null) {
                Toast.makeText(mRootView.getContext(), "Dump finish, search TAG '" + tag + "' for detail", Toast.LENGTH_LONG).show();
            }
        }
    };

    public static TopThreadIndicator instance() {
        return sInstance;
    }

    TopThreadIndicator() {
    }

    public void requestPermission(Context context, int reqCode) {
        if (checkPermission(context)) {
            return;
        }
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.M) {
            Intent intent = new Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION, Uri.parse("package:" + context.getPackageName()));
            if (reqCode > 0 && context instanceof Activity) {
                ((Activity) context).startActivityForResult(intent, reqCode);
            } else {
                context.startActivity(intent);
            }
        }
    }

    public boolean checkPermission(Context context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            return Settings.canDrawOverlays(context);
        } else {
            return true;
        }
    }

    @UiThread
    public boolean isShowing() {
        return mRootView != null;
    }

    @UiThread
    public boolean isRunning() {
        return mRootView != null && mRunningRef.get(mRootView.hashCode(), false);
    }


    @SuppressLint("InflateParams")
    @UiThread
    public boolean show(Context context) {
        if (!checkPermission(context)) {
            return false;
        }

        try {
            // 1. Window View
            mRootView = LayoutInflater.from(context).inflate(R.layout.float_top_thread, null);
            if (mRootView == null) {
                MatrixLog.w(TAG, "Can not load indicator view!");
                return false;
            }
            final int hashcode = mRootView.hashCode();
            final WindowManager windowManager = (WindowManager) context.getApplicationContext().getSystemService(Context.WINDOW_SERVICE);
            DisplayMetrics metrics = new DisplayMetrics();
            if (null != windowManager.getDefaultDisplay()) {
                windowManager.getDefaultDisplay().getMetrics(displayMetrics);
                windowManager.getDefaultDisplay().getMetrics(metrics);
            }

            final WindowManager.LayoutParams layoutParam = new WindowManager.LayoutParams();
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                layoutParam.type = WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY;
            } else {
                layoutParam.type = WindowManager.LayoutParams.TYPE_PHONE;
            }
            layoutParam.flags = WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                    | WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL;
            layoutParam.gravity = Gravity.START | Gravity.TOP;
            // if (null != rootView) {
            //     layoutParam.x = metrics.widthPixels - rootView.getLayoutParams().width * 2;
            // }
            layoutParam.y = 0;
            layoutParam.width = dip2px(context, 250f);
            layoutParam.height = WindowManager.LayoutParams.WRAP_CONTENT;
            layoutParam.format = PixelFormat.TRANSPARENT;

            windowManager.addView(mRootView, layoutParam);

            // 2. Init views
            final TextView tvPid = mRootView.findViewById(R.id.tv_curr_pid);
            tvPid.setText(String.valueOf(mCurrProc.first));
            final TextView tvProc = mRootView.findViewById(R.id.tv_proc);
            tvProc.setText(mCurrProc.second);

            // init thread entryGroup
            LinearLayout procEntryGroup = mRootView.findViewById(R.id.layout_entry_proc_group);
            for (int i = 0; i < MAX_PROC_NUM - 1; i++) {
                View entryItemView = LayoutInflater.from(procEntryGroup.getContext()).inflate(R.layout.float_item_proc_entry, procEntryGroup, false);
                LinearLayout.LayoutParams layoutParams = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
                layoutParams.topMargin = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 5, procEntryGroup.getContext().getResources().getDisplayMetrics());
                entryItemView.setVisibility(View.GONE);
                procEntryGroup.addView(entryItemView, layoutParams);
            }
            // init thread entryGroup
            LinearLayout threadEntryGroup = mRootView.findViewById(R.id.layout_entry_group);
            for (int i = 0; i < MAX_THREAD_NUM - 1; i++) {
                View entryItemView = LayoutInflater.from(threadEntryGroup.getContext()).inflate(R.layout.float_item_thread_entry, threadEntryGroup, false);
                LinearLayout.LayoutParams layoutParams = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
                layoutParams.topMargin = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 5, threadEntryGroup.getContext().getResources().getDisplayMetrics());
                entryItemView.setVisibility(View.GONE);
                threadEntryGroup.addView(entryItemView, layoutParams);
            }

            // 3. Drag
            mRootView.setOnTouchListener(new View.OnTouchListener() {
                float downX = 0;
                float downY = 0;
                int downOffsetX = 0;
                int downOffsetY = 0;

                @SuppressLint("ClickableViewAccessibility")
                @Override
                public boolean onTouch(final View v, MotionEvent event) {
                    switch (event.getAction()) {
                        case MotionEvent.ACTION_DOWN:
                            downX = event.getX();
                            downY = event.getY();
                            downOffsetX = layoutParam.x;
                            downOffsetY = layoutParam.y;
                            break;
                        case MotionEvent.ACTION_MOVE:
                            float moveX = event.getX();
                            float moveY = event.getY();
                            layoutParam.x += (moveX - downX) / 3;
                            layoutParam.y += (moveY - downY) / 3;
                            if (v != null) {
                                windowManager.updateViewLayout(v, layoutParam);
                            }
                            break;
                        case MotionEvent.ACTION_UP:
                            PropertyValuesHolder holder = PropertyValuesHolder.ofInt(
                                    "trans",
                                    layoutParam.x,
                                    layoutParam.x > (displayMetrics.widthPixels - mRootView.getWidth()) / 2 ? displayMetrics.widthPixels - mRootView.getWidth() : 0
                            );
                            ValueAnimator animator = ValueAnimator.ofPropertyValuesHolder(holder);
                            animator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
                                @Override
                                public void onAnimationUpdate(ValueAnimator animation) {
                                    if (mRootView == null || mRootView.hashCode() != hashcode) {
                                        return;
                                    }
                                    layoutParam.x = (int) animation.getAnimatedValue("trans");
                                    windowManager.updateViewLayout(v, layoutParam);
                                }
                            });
                            animator.setInterpolator(new AccelerateInterpolator());
                            animator.setDuration(180).start();
                            break;
                    }
                    return true;
                }

            });

            // 4. Listener
            mRootView.findViewById(R.id.layout_proc).setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    final TextView tvProc = mRootView.findViewById(R.id.tv_proc);
                    PopupMenu menu = new PopupMenu(v.getContext(), tvProc);
                    final List<Pair<Integer, String>> procList = getProcList(v.getContext());
                    for (Pair<Integer, String> item : procList) {
                        //noinspection ConstantConditions
                        menu.getMenu().add("Process :" + getProcSuffix(item.second));
                    }
                    menu.setOnMenuItemClickListener(new PopupMenu.OnMenuItemClickListener() {
                        @SuppressLint("SetTextI18n")
                        @Override
                        public boolean onMenuItemClick(MenuItem item) {
                            String title = item.getTitle().toString();
                            if (title.contains(":")) {
                                String proc = title.substring(title.lastIndexOf(":") + 1);
                                for (Pair<Integer, String> procItem : procList) {
                                    //noinspection ConstantConditions
                                    if (title.equals("Process :" + getProcSuffix(procItem.second))) {
                                        mCurrProc = procItem;
                                        tvProc.setText(":" + proc);
                                        tvPid.setText(String.valueOf(mCurrProc.first));
                                    }
                                }
                            }
                            return false;
                        }
                    });
                    menu.show();
                }
            });
            mRootView.findViewById(R.id.layout_dump).setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    // Dump all threads
                    mDumpHandler.accept(mCurrDelta);
                }
            });

            return true;
        } catch (Exception e) {
            MatrixLog.w(TAG, "Create float view failed:" + e.getMessage());
            return false;
        }
    }

    public void start(final int seconds) {
        if (mRootView == null) {
            MatrixLog.w(TAG, "Call #prepare first to show the indicator");
            return;
        }
        if (isRunning()) {
            MatrixLog.w(TAG, "Already started!");
            return;
        }
        final int hashcode = mRootView.hashCode();
        mRunningRef.clear();
        mRunningRef.put(hashcode, true);
        BatteryCanary.getMonitorFeature(TopThreadFeature.class, new Consumer<TopThreadFeature>() {
            @Override
            public void accept(TopThreadFeature topThreadFeat) {
                topThreadFeat.top(seconds, new ContinuousCallback<JiffiesSnapshot>() {
                    @Override
                    public boolean onGetDeltas(List<Delta<JiffiesSnapshot>> deltaList, long windowMillis) {
                        refresh(deltaList);
                        if (mRootView == null || !mRunningRef.get(hashcode, false)) {
                            return true;
                        }
                        return false;
                    }
                });
            }
        });
    }

    public void stop() {
        if (mRootView != null) {
            mRunningRef.put(mRootView.hashCode(), false);
        }
    }

    @UiThread
    public void dismiss() {
        if (mRootView != null) {
            WindowManager windowManager = (WindowManager) mRootView.getContext().getApplicationContext().getSystemService(Context.WINDOW_SERVICE);
            windowManager.removeView(mRootView);
            mRootView = null;
        }
    }

    @NonNull
    public Consumer<Delta<JiffiesSnapshot>> getDumpHandler() {
        return mDumpHandler;
    }

    public void setDumpHandler(@NonNull Consumer<Delta<JiffiesSnapshot>> dumpHandler) {
        mDumpHandler = dumpHandler;
    }

    public void setCollector(@NonNull CallStackCollector collector) {
        mCollector = collector;
    }

    private void setTextAlertColor(TextView tv, int level) {
        tv.setTextColor(tv.getResources().getColor(
                level == 2 ? R.color.Red_80 :  level == 1 ? R.color.Orange_80 : R.color.FG_2
        ));
    }

    @SuppressLint({"SetTextI18n", "RestrictedApi"})
    private void refresh(final List<Delta<JiffiesSnapshot>> deltaList) {
        mUiHandler.post(new Runnable() {
            @Override
            public void run() {
                if (mRootView == null) {
                    return;
                }

                TextView tvHeaderLeft = mRootView.findViewById(R.id.tv_header_left);
                int battTemp = BatteryCanaryUtil.getBatteryTemperatureImmediately(mRootView.getContext());
                tvHeaderLeft.setText((battTemp > 0 ? battTemp / 10f : "/") + "°C");
                setTextAlertColor(tvHeaderLeft, battTemp >= 350 ? 2 :  battTemp >= 300 ? 1 : 0);

                // EntryList
                LinearLayout procEntryGroup = mRootView.findViewById(R.id.layout_entry_proc_group);
                for (int i = 0; i < procEntryGroup.getChildCount(); i++) {
                    procEntryGroup.getChildAt(i).setVisibility(View.GONE);
                }
                LinearLayout threadEntryGroup = mRootView.findViewById(R.id.layout_entry_group);
                for (int i = 0; i < threadEntryGroup.getChildCount(); i++) {
                    threadEntryGroup.getChildAt(i).setVisibility(View.GONE);
                }

                List<Pair<Integer, String>> procList = getProcList(mRootView.getContext());
                int totalCpuLoad = 0;
                for (int i = 0; i < deltaList.size(); i++) {
                    Delta<JiffiesSnapshot> delta = deltaList.get(i);
                    if (delta.isValid()) {
                        // Proc
                        int pid = delta.dlt.pid;
                        String name = delta.dlt.name;
                        for (Pair<Integer, String> item : procList) {
                            //noinspection ConstantConditions
                            if (item.first == pid) {
                                //noinspection ConstantConditions
                                name = getProcSuffix(item.second);
                            }
                        }

                        //noinspection ConstantConditions
                        if (pid == mCurrProc.first) {
                            // Curr Selected Proc's threads
                            name = name + " <-";
                            int idx = 0;
                            for (JiffiesSnapshot.ThreadJiffiesEntry threadJiffies : delta.dlt.threadEntries.getList()) {
                                long entryJffies = threadJiffies.get();
                                int tid = threadJiffies.tid;
                                String threadName = threadJiffies.name;
                                String status = (threadJiffies.isNewAdded ? "+" : "~") + threadJiffies.stat;
                                int threadLoad = (int) figureCupLoad(entryJffies, delta.during / 10L);

                                View threadItemView = threadEntryGroup.getChildAt(idx);
                                threadItemView.setVisibility(View.VISIBLE);
                                TextView tvName = threadItemView.findViewById(R.id.tv_name);
                                TextView tvTid = threadItemView.findViewById(R.id.tv_tid);
                                TextView tvStatus = threadItemView.findViewById(R.id.tv_status);
                                TextView tvLoad = threadItemView.findViewById(R.id.tv_load);
                                tvName.setText(threadName);
                                tvTid.setText(String.valueOf(tid));
                                tvStatus.setText(status);
                                tvLoad.setText(threadLoad + "%");

                                int alertLevel = threadJiffies.isNewAdded ? 0 : threadLoad >= 50 ? 2 : threadLoad >= 10 ? 1 : 0;
                                setTextAlertColor(tvName, alertLevel);
                                setTextAlertColor(tvTid, alertLevel);
                                setTextAlertColor(tvStatus, alertLevel);
                                setTextAlertColor(tvLoad, alertLevel);

                                idx++;
                                if (idx >= MAX_THREAD_NUM) {
                                    break;
                                }
                            }
                            mCurrDelta = delta;
                        }

                        int procLoad = (int) figureCupLoad(delta.dlt.totalJiffies.get(), delta.during / 10L);
                        totalCpuLoad += procLoad;
                        View procItemView = procEntryGroup.getChildAt(i);
                        procItemView.setVisibility(View.VISIBLE);
                        TextView tvProcName = procItemView.findViewById(R.id.tv_name);
                        TextView tvProcPid = procItemView.findViewById(R.id.tv_pid);
                        TextView tvProcLoad = procItemView.findViewById(R.id.tv_load);
                        tvProcName.setText(":" + name);
                        tvProcPid.setText(String.valueOf(pid));
                        tvProcLoad.setText(procLoad + "%");
                    }
                }

                TextView tvHeaderRight = mRootView.findViewById(R.id.tv_header_right);
                tvHeaderRight.setText(totalCpuLoad + "%");
            }
        });
    }

    public static List<Pair<Integer, String>> getProcList(Context context) {
        ArrayList<Pair<Integer, String>> list = new ArrayList<>();
        ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        if (am != null) {
            List<ActivityManager.RunningAppProcessInfo> processes = am.getRunningAppProcesses();
            for (ActivityManager.RunningAppProcessInfo item : processes) {
                if (item.processName.contains(context.getPackageName())) {
                    list.add(new Pair<>(item.pid, item.processName));
                }
            }
        }
        return list;
    }

    private static String getProcSuffix(String input) {
        String proc = "main";
        if (input.contains(":")) {
            proc = input.substring(input.lastIndexOf(":") + 1);
        }
        return proc;
    }

    @SuppressWarnings("SameParameterValue")
    static int dip2px(Context context, float dpVale) {
        final float scale = context.getResources().getDisplayMetrics().density;
        return (int) (dpVale * scale + 0.5f);
    }
}
