package sample.tencent.matrix.battery.shell;

import android.annotation.SuppressLint;
import android.app.ActivityManager;
import android.content.Context;
import android.graphics.PixelFormat;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.Process;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.LinearLayout;
import android.widget.PopupMenu;
import android.widget.TextView;
import android.widget.Toast;

import com.tencent.matrix.batterycanary.BatteryCanary;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature.JiffiesSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Delta;
import com.tencent.matrix.batterycanary.shell.TopThreadFeature;
import com.tencent.matrix.batterycanary.shell.TopThreadFeature.ContinuousCallback;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.batterycanary.utils.Consumer;
import com.tencent.matrix.javalib.util.Pair;
import com.tencent.matrix.util.MatrixLog;

import java.util.ArrayList;
import java.util.List;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import sample.tencent.matrix.R;

/**
 * See {@link TopThreadFeature}.
 *
 * @author Kaede
 * @since 2022/2/23
 */
final public class TopThreadIndicator {
    private static final String TAG = "Matrix.TopThreadIndicator";
    private static final int MAX_THREAD_NUM = 10;
    @SuppressLint("StaticFieldLeak")
    private static final TopThreadIndicator sInstance = new TopThreadIndicator();

    private final DisplayMetrics displayMetrics = new DisplayMetrics();
    private final Handler mUiHandler = new Handler(Looper.getMainLooper());
    private boolean mStopped = true;
    @SuppressLint("RestrictedApi")
    private Pair<Integer, String> mCurrProc = new Pair<>(Process.myPid(), getProcSuffix(BatteryCanaryUtil.getProcessName()));

    @Nullable
    private View mRootView;
    @Nullable
    private Delta<JiffiesSnapshot> mCurrDelta;
    @NonNull
    private Consumer<Delta<JiffiesSnapshot>> mDumpHandler = new Consumer<Delta<JiffiesSnapshot>>() {
        @Override
        public void accept(Delta<JiffiesSnapshot> delta) {
            if (mRootView != null) {
                Toast.makeText(mRootView.getContext(), "Dumping not impl", Toast.LENGTH_SHORT).show();
            }
        }
    };

    public static TopThreadIndicator instance() {
        return sInstance;
    }

    TopThreadIndicator() {
    }

    @SuppressLint("InflateParams")
    public void prepare(Context context) {
        try {
            mRootView = LayoutInflater.from(context).inflate(R.layout.float_top_thread, null);
            if (mRootView == null) {
                return;
            }
            WindowManager windowManager = (WindowManager) context.getApplicationContext().getSystemService(Context.WINDOW_SERVICE);
            DisplayMetrics metrics = new DisplayMetrics();
            if (null != windowManager.getDefaultDisplay()) {
                windowManager.getDefaultDisplay().getMetrics(displayMetrics);
                windowManager.getDefaultDisplay().getMetrics(metrics);
            }

            WindowManager.LayoutParams layoutParam = new WindowManager.LayoutParams();
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

            // init entryGroup
            LinearLayout entryGroup = mRootView.findViewById(R.id.layout_entry_group);
            for (int i = 0; i < MAX_THREAD_NUM - 1; i++) {
                View entryItemView = LayoutInflater.from(entryGroup.getContext()).inflate(R.layout.float_item_entry, entryGroup, false);
                LinearLayout.LayoutParams layoutParams = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
                layoutParams.topMargin = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 5, entryGroup.getContext().getResources().getDisplayMetrics());
                entryItemView.setVisibility(View.GONE);
                entryGroup.addView(entryItemView, layoutParams);
            }

            final TextView tvPid = mRootView.findViewById(R.id.tv_pid);
            tvPid.setText(String.valueOf(mCurrProc.left));

            // listener
            mRootView.findViewById(R.id.layout_proc).setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    final TextView tvProc = mRootView.findViewById(R.id.tv_proc);
                    PopupMenu menu = new PopupMenu(v.getContext(), tvProc);
                    final List<Pair<Integer, String>> procList = getProcList(v.getContext());
                    for (Pair<Integer, String> item : procList) {
                        menu.getMenu().add("Process :" + item.right);
                    }
                    menu.setOnMenuItemClickListener(new PopupMenu.OnMenuItemClickListener() {
                        @SuppressLint("SetTextI18n")
                        @Override
                        public boolean onMenuItemClick(MenuItem item) {
                            String title = item.getTitle().toString();
                            if (title.contains(":")) {
                                String proc = title.substring(title.lastIndexOf(":") + 1);
                                for (Pair<Integer, String> procItem : procList) {
                                    if (title.equals("Process :" + procItem.right)) {
                                        mCurrProc = procItem;
                                        tvProc.setText(":" + proc);
                                        tvPid.setText(String.valueOf(mCurrProc.left));
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
        } catch (Exception e) {
            MatrixLog.w(TAG, "Create float view failed:" + e.getMessage());
        }

    }

    public void start(final int seconds) {
        if (mRootView == null) {
            throw new IllegalStateException("Call prepare first!");
        }
        if (!mStopped) {
            throw new IllegalStateException("Already started!");
        }

        mStopped = false;
        BatteryCanary.getMonitorFeature(TopThreadFeature.class, new Consumer<TopThreadFeature>() {
            @Override
            public void accept(TopThreadFeature topThreadFeat) {
                topThreadFeat.top(seconds, new ContinuousCallback<JiffiesSnapshot>() {
                    @Override
                    public boolean onGetDeltas(List<Delta<JiffiesSnapshot>> deltaList, long windowMillis) {
                        refresh(deltaList);
                        return mStopped;
                    }
                });
            }
        });
    }

    public void stop() {
        mStopped = true;
    }

    public void release() {
        if (mRootView != null) {
            WindowManager windowManager = (WindowManager) mRootView.getContext().getApplicationContext().getSystemService(Context.WINDOW_SERVICE);
            windowManager.removeView(mRootView);
            mRootView = null;
        }
    }

    public void setDumpHandler(@NonNull Consumer<Delta<JiffiesSnapshot>> dumpHandler) {
        mDumpHandler = dumpHandler;
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

                // ThreadList
                LinearLayout entryGroup = mRootView.findViewById(R.id.layout_entry_group);
                for (int i = 0; i < entryGroup.getChildCount(); i++) {
                    entryGroup.getChildAt(i).setVisibility(View.GONE);
                }

                for (Delta<JiffiesSnapshot> delta : deltaList) {
                    if (delta.dlt.pid == mCurrProc.left) {
                        if (delta.isValid()) {
                            int pid = delta.dlt.pid;
                            String name = delta.dlt.name;
                            int procLoad = (int) figureCupLoad(delta.dlt.totalJiffies.get(), delta.during / 10L);
                            TextView tvHeaderRight = mRootView.findViewById(R.id.tv_header_right);
                            tvHeaderRight.setText(procLoad + "%");

                            int idx = 0;
                            for (JiffiesSnapshot.ThreadJiffiesEntry threadJiffies : delta.dlt.threadEntries.getList()) {
                                long entryJffies = threadJiffies.get();
                                int tid = threadJiffies.tid;
                                String threadName = threadJiffies.name;
                                String status = (threadJiffies.isNewAdded ? "+" : "~") + threadJiffies.stat;
                                int threadLoad = (int) figureCupLoad(entryJffies, delta.during / 10L);

                                View entryItemView = entryGroup.getChildAt(idx);
                                entryItemView.setVisibility(View.VISIBLE);
                                TextView tvName = entryItemView.findViewById(R.id.tv_name);
                                TextView tvTid = entryItemView.findViewById(R.id.tv_tid);
                                TextView tvStatus = entryItemView.findViewById(R.id.tv_status);
                                TextView tvLoad = entryItemView.findViewById(R.id.tv_load);
                                tvName.setText(threadName);
                                tvTid.setText(String.valueOf(tid));
                                tvStatus.setText(status);
                                tvLoad.setText(threadLoad + "%");

                                idx++;
                                if (idx >= MAX_THREAD_NUM) {
                                    break;
                                }
                            }
                        }
                        mCurrDelta = delta;
                        break;
                    }
                }
            }
        });
    }

    static List<Pair<Integer, String>> getProcList(Context context) {
        ArrayList<Pair<Integer, String>> list = new ArrayList<>();
        ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        if (am != null) {
            List<ActivityManager.RunningAppProcessInfo> processes = am.getRunningAppProcesses();
            for (ActivityManager.RunningAppProcessInfo item : processes) {
                if (item.processName.contains(context.getPackageName())) {
                    String proc = getProcSuffix(item.processName);
                    list.add(new Pair<>(item.pid, proc));
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

    static float figureCupLoad(long jiffies, long cpuJiffies) {
        return (jiffies / (cpuJiffies * 1f)) * 100;
    }

    static int dip2px(Context context, float dpVale) {
        final float scale = context.getResources().getDisplayMetrics().density;
        return (int) (dpVale * scale + 0.5f);
    }
}
