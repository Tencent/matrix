package com.tencent.matrix.batterycanary.shell.ui;

import android.animation.PropertyValuesHolder;
import android.animation.ValueAnimator;
import android.annotation.SuppressLint;
import android.app.Activity;
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
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature.JiffiesSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Delta;
import com.tencent.matrix.batterycanary.shell.TopThreadFeature;
import com.tencent.matrix.batterycanary.shell.TopThreadFeature.ContinuousCallback;
import com.tencent.matrix.batterycanary.stats.BatteryStatsFeature;
import com.tencent.matrix.batterycanary.stats.ui.BatteryStatsActivity;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.batterycanary.utils.CallStackCollector;
import com.tencent.matrix.batterycanary.utils.Consumer;
import com.tencent.matrix.util.MatrixLog;

import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.UiThread;
import androidx.core.util.Pair;

import static com.tencent.matrix.batterycanary.shell.TopThreadFeature.figureCupLoad;
import static com.tencent.matrix.batterycanary.shell.TopThreadFeature.fixedColumn;
import static com.tencent.matrix.batterycanary.shell.TopThreadFeature.formatFloat;

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
    private BatteryMonitorCore mCore;
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
                float cpuLoad = figureCupLoad(delta.dlt.totalJiffies.get(), delta.during / 10);
                extras.put("load", cpuLoad);
                // Load
                printer.createSection("Proc");
                printer.writeLine("pid", String.valueOf(delta.dlt.pid));
                printer.writeLine("cmm", String.valueOf(delta.dlt.name));
                printer.writeLine("load", formatFloat(cpuLoad, 1) + "%");
                printer.createSubSection("Thread(" + delta.dlt.threadEntries.getList().size() + ")");
                printer.writeLine("  TID\tLOAD \tSTATUS \tTHREAD_NAME \tJIFFY");
                for (JiffiesSnapshot.ThreadJiffiesEntry threadJiffies : delta.dlt.threadEntries.getList()) {
                    long entryJffies = threadJiffies.get();
                    printer.append("|   -> ")
                            .append(fixedColumn(String.valueOf(threadJiffies.tid), 5)).append("\t")
                            .append(fixedColumn(formatFloat(figureCupLoad(entryJffies, delta.during / 10), 1) + "%", 4)).append("\t")
                            .append(threadJiffies.isNewAdded ? "+" : "~").append("/").append(threadJiffies.stat).append("\t")
                            .append(fixedColumn(threadJiffies.name, 16)).append("\t")
                            .append(entryJffies).append("\t")
                            .append("\n");
                }
                // Thread Stack
                printer.createSection("Stacks");
                for (JiffiesSnapshot.ThreadJiffiesEntry threadJiffies : delta.dlt.threadEntries.getList()) {
                    long entryJffies = threadJiffies.get();
                    float load = figureCupLoad(entryJffies, delta.during / 10);
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
                // Stats
                if (mCore != null) {
                    BatteryStatsFeature stats = mCore.getMonitorFeature(BatteryStatsFeature.class);
                    if (stats != null) {
                        String event = "MATRIX_TOP_DUMP";
                        int eventId = delta.dlt.pid;
                        stats.statsEvent(event, eventId, extras);
                    }
                }
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

    @NonNull
    private Runnable mShowReportHandler = new Runnable() {
        @Override
        public void run() {
            if (mCore == null) {
                if (mRootView != null) {
                    String tag = "TOP_THREAD_DUMP";
                    Toast.makeText(mRootView.getContext(), "Search TAG '" + tag + "' for detail report", Toast.LENGTH_LONG).show();
                }
                return;
            }
            // Show battery stats report
            Intent intent = new Intent(mCore.getContext(), BatteryStatsActivity.class);
            if (!(mCore.getContext() instanceof Activity)) {
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            }
            mCore.getContext().startActivity(intent);
        }
    };

    public static TopThreadIndicator instance() {
        return sInstance;
    }

    TopThreadIndicator() {
    }

    public TopThreadIndicator attach(@NonNull BatteryMonitorCore core) {
        mCore = core;
        return this;
    }

    public TopThreadIndicator attach(@NonNull Consumer<Delta<JiffiesSnapshot>> dumpHandler) {
        mDumpHandler = dumpHandler;
        return this;
    }

    public TopThreadIndicator attach(@NonNull CallStackCollector collector) {
        mCollector = collector;
        return this;
    }

    public TopThreadIndicator attach(@NonNull Runnable showReportHandler) {
        mShowReportHandler = showReportHandler;
        return this;
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
            mRootView = LayoutInflater.from(context).inflate(R.layout.float_top_thread_container, null);
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
            layoutParam.width = WindowManager.LayoutParams.WRAP_CONTENT;
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
            View.OnTouchListener onTouchListener = new View.OnTouchListener() {
                float downX = 0;
                float downY = 0;
                int downOffsetX = 0;
                int downOffsetY = 0;

                @SuppressLint("ClickableViewAccessibility")
                @Override
                public boolean onTouch(final View v, MotionEvent event) {
                    boolean consumed = false;
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

                            int upOffsetX = layoutParam.x;
                            int upOffsetY = layoutParam.y;
                            if (Math.abs(upOffsetX - downOffsetX) > 20 || Math.abs(upOffsetY - downOffsetY) > 20) {
                                consumed = true;
                            }
                    }
                    return consumed;
                }

            };
            mRootView.setOnTouchListener(onTouchListener);

            // 4. Listener
            View.OnClickListener listener = new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    if (v.getId() == R.id.layout_dump) {
                        // Dump all threads
                        mDumpHandler.accept(mCurrDelta);
                        return;
                    }
                    if (v.getId() == R.id.layout_proc) {
                        // Choose proc
                        final TextView tvProc = mRootView.findViewById(R.id.tv_proc);
                        PopupMenu menu = new PopupMenu(v.getContext(), tvProc);
                        final List<Pair<Integer, String>> procList = TopThreadFeature.getProcList(v.getContext());
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
                        return;
                    }
                    if (v.getId() == R.id.iv_logo) {
                        // Show dump report
                        mShowReportHandler.run();
                        return;
                    }
                    if (v.getId() == R.id.tv_minify) {
                        // Minify
                        mRootView.findViewById(R.id.layout_top).setVisibility(View.GONE);
                        mRootView.findViewById(R.id.iv_logo_minify).setVisibility(View.VISIBLE);
                        return;
                    }
                    if (v == mRootView && mRootView.findViewById(R.id.layout_top).getVisibility() == View.GONE) {
                        // Minify LOGO
                        View anchorView = mRootView.findViewById(R.id.iv_logo_minify);
                        PopupMenu menu = new PopupMenu(v.getContext(), anchorView);
                        menu.getMenu().add("Expand");
                        menu.getMenu().add("Close");
                        menu.setOnMenuItemClickListener(new PopupMenu.OnMenuItemClickListener() {
                            @SuppressLint("SetTextI18n")
                            @Override
                            public boolean onMenuItemClick(MenuItem item) {
                                String title = item.getTitle().toString();
                                switch (title) {
                                    case "Expand":
                                        mRootView.findViewById(R.id.layout_top).setVisibility(View.VISIBLE);
                                        mRootView.findViewById(R.id.iv_logo_minify).setVisibility(View.GONE);
                                        break;
                                    case "Close":
                                        mUiHandler.postDelayed(new Runnable() {
                                            @Override
                                            public void run() {
                                                dismiss();
                                            }
                                        }, 200L);
                                        break;
                                    default:
                                        break;
                                }
                                return false;
                            }
                        });
                        menu.show();
                    }
                }
            };

            mRootView.findViewById(R.id.layout_proc).setOnClickListener(listener);
            mRootView.findViewById(R.id.layout_dump).setOnClickListener(listener);
            mRootView.findViewById(R.id.iv_logo).setOnClickListener(listener);
            mRootView.findViewById(R.id.tv_minify).setOnClickListener(listener);
            mRootView.setOnClickListener(listener);
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

                int battTemp = BatteryCanaryUtil.getBatteryTemperatureImmediately(mRootView.getContext());
                TextView tvBattTemp = mRootView.findViewById(R.id.tv_header_left);
                tvBattTemp.setText((battTemp > 0 ? battTemp / 10f : "/") + "°C");
                setTextAlertColor(tvBattTemp, battTemp >= 350 ? 2 :  battTemp >= 300 ? 1 : 0);
                tvBattTemp = mRootView.findViewById(R.id.tv_temp_minify);
                tvBattTemp.setText((battTemp > 0 ? battTemp / 10f : "/") + "°C");
                setTextAlertColor(tvBattTemp, battTemp >= 350 ? 2 :  battTemp >= 300 ? 1 : 0);

                // EntryList
                LinearLayout procEntryGroup = mRootView.findViewById(R.id.layout_entry_proc_group);
                for (int i = 0; i < procEntryGroup.getChildCount(); i++) {
                    procEntryGroup.getChildAt(i).setVisibility(View.GONE);
                }
                LinearLayout threadEntryGroup = mRootView.findViewById(R.id.layout_entry_group);
                for (int i = 0; i < threadEntryGroup.getChildCount(); i++) {
                    threadEntryGroup.getChildAt(i).setVisibility(View.GONE);
                }

                List<Pair<Integer, String>> procList = TopThreadFeature.getProcList(mRootView.getContext());
                float totalCpuLoad = 0;
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
                                float threadLoad = figureCupLoad(entryJffies, delta.during / 10L);

                                View threadItemView = threadEntryGroup.getChildAt(idx);
                                threadItemView.setVisibility(View.VISIBLE);
                                TextView tvName = threadItemView.findViewById(R.id.tv_name);
                                TextView tvTid = threadItemView.findViewById(R.id.tv_tid);
                                TextView tvStatus = threadItemView.findViewById(R.id.tv_status);
                                TextView tvLoad = threadItemView.findViewById(R.id.tv_load);
                                tvName.setText(threadName);
                                tvTid.setText(String.valueOf(tid));
                                tvStatus.setText(status);
                                tvLoad.setText(formatFloat(threadLoad, 1) + "%");

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

                        float procLoad = figureCupLoad(delta.dlt.totalJiffies.get(), delta.during / 10L);
                        totalCpuLoad += procLoad;
                        View procItemView = procEntryGroup.getChildAt(i);
                        procItemView.setVisibility(View.VISIBLE);
                        TextView tvProcName = procItemView.findViewById(R.id.tv_name);
                        TextView tvProcPid = procItemView.findViewById(R.id.tv_pid);
                        TextView tvProcLoad = procItemView.findViewById(R.id.tv_load);
                        tvProcName.setText(":" + name);
                        tvProcPid.setText(String.valueOf(pid));
                        tvProcLoad.setText(formatFloat(procLoad, 1) + "%");
                    }
                }

                String totalLoad = formatFloat(totalCpuLoad, 1) + "%";
                TextView tvLoad = mRootView.findViewById(R.id.tv_header_right);
                tvLoad.setText(totalLoad);
                tvLoad = mRootView.findViewById(R.id.tv_load_minify);
                tvLoad.setText(totalLoad);
            }
        });
    }

    private static String getProcSuffix(String input) {
        String proc = "main";
        if (input.contains(":")) {
            proc = input.substring(input.lastIndexOf(":") + 1);
        }
        return proc;
    }
}
