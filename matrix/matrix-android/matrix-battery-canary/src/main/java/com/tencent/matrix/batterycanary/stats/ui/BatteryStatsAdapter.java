package com.tencent.matrix.batterycanary.stats.ui;

import android.annotation.SuppressLint;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import com.tencent.matrix.batterycanary.R;
import com.tencent.matrix.batterycanary.monitor.feature.CompositeMonitors;
import com.tencent.matrix.batterycanary.stats.BatteryRecord;
import com.tencent.matrix.batterycanary.utils.ThreadSafeReference;

import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.Map;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.recyclerview.widget.RecyclerView;

import static com.tencent.matrix.batterycanary.stats.BatteryRecord.ReportRecord.EXTRA_APP_FOREGROUND;
import static com.tencent.matrix.batterycanary.stats.BatteryRecord.ReportRecord.EXTRA_JIFFY_OVERHEAT;

/**
 * @author Kaede
 * @since 2021/12/10
 */
public class BatteryStatsAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder> {

    public static final int VIEW_TYPE_HEADER = 0;
    public static final int VIEW_TYPE_EVENT_DUMP = 1;
    public static final int VIEW_TYPE_EVENT_LEVEL_1 = 2;
    public static final int VIEW_TYPE_EVENT_LEVEL_2 = 3;
    public static final int VIEW_TYPE_NO_DATA = 4;
    public static final int VIEW_TYPE_EVENT_SIMPLE = 5;
    public static final int VIEW_TYPE_EVENT_BATTERY = 6;

    protected final List<Item> dataList = new ArrayList<>();

    public List<Item> getDataList() {
        return dataList;
    }

    @NonNull
    @Override
    public RecyclerView.ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        switch (viewType) {
            case VIEW_TYPE_HEADER:
                return new ViewHolder.HeaderHolder(LayoutInflater.from(parent.getContext()).inflate(R.layout.stats_item_header, parent, false));
            case VIEW_TYPE_NO_DATA:
                return new ViewHolder.NoDataHolder(LayoutInflater.from(parent.getContext()).inflate(R.layout.stats_item_no_data, parent, false));
            case VIEW_TYPE_EVENT_DUMP:
                return new ViewHolder.EventDumpHolder(LayoutInflater.from(parent.getContext()).inflate(R.layout.stats_item_event_dump, parent, false));
            case VIEW_TYPE_EVENT_SIMPLE:
                return new ViewHolder.EventSimpleHolder(LayoutInflater.from(parent.getContext()).inflate(R.layout.stats_item_event_simple, parent, false));
            case VIEW_TYPE_EVENT_BATTERY:
                return new ViewHolder.EventBatteryHolder(LayoutInflater.from(parent.getContext()).inflate(R.layout.stats_item_event_battery, parent, false));
            case VIEW_TYPE_EVENT_LEVEL_1:
                return new ViewHolder.EventLevel1Holder(LayoutInflater.from(parent.getContext()).inflate(R.layout.stats_item_event_1, parent, false));
            case VIEW_TYPE_EVENT_LEVEL_2:
                return new ViewHolder.EventLevel2Holder(LayoutInflater.from(parent.getContext()).inflate(R.layout.stats_item_event_2, parent, false));
        }
        throw new IllegalStateException("Unknown view type: " + viewType);
    }

    @Override
    public void onBindViewHolder(@NonNull RecyclerView.ViewHolder holder, int position) {
        //noinspection unchecked
        ((ViewHolder<Item>) holder).bind(dataList.get(position));
    }

    @Override
    public int getItemCount() {
        return dataList.size();
    }

    @Override
    public int getItemViewType(int position) {
        return dataList.get(position).viewType();
    }

    public interface Item {
        int viewType();

        class HeaderItem implements Item {
            public String date;

            @Override
            public int viewType() {
                return VIEW_TYPE_HEADER;
            }
        }

        class NoDataItem implements Item {
            public String text;

            @Override
            public int viewType() {
                return VIEW_TYPE_NO_DATA;
            }
        }

        class EventDumpItem extends BatteryRecord.ReportRecord implements Item {
            public final ReportRecord record;
            public boolean expand = false;
            public String desc;

            public EventDumpItem(ReportRecord record) {
                this.record = record;
                this.millis = record.millis;
                this.id = record.id;
                this.event = record.event;
                this.extras = record.extras;
                this.scope = record.scope;
                this.windowMillis = record.windowMillis;
                this.threadInfoList = record.threadInfoList;
                this.entryList = record.entryList;
            }

            @Override
            public int viewType() {
                return VIEW_TYPE_EVENT_DUMP;
            }
        }

        class EventSimpleItem extends BatteryRecord.EventStatRecord implements Item {
            public final EventStatRecord record;

            public EventSimpleItem(EventStatRecord record) {
                this.millis = record.millis;
                this.record = record;
                this.id = record.id;
                this.event = record.event;
            }

            @Override
            public int viewType() {
                return VIEW_TYPE_EVENT_SIMPLE;
            }
        }

        class EventBatteryItem extends BatteryRecord.EventStatRecord implements Item {
            public final EventStatRecord record;

            public EventBatteryItem(EventStatRecord record) {
                this.millis = record.millis;
                this.record = record;
                this.id = record.id;
                this.event = record.event;
            }

            @Override
            public int viewType() {
                return VIEW_TYPE_EVENT_BATTERY;
            }
        }

        @SuppressLint("ParcelCreator")
        class EventLevel1Item extends BatteryRecord implements Item {
            public String text;

            public EventLevel1Item(BatteryRecord record) {
                this.millis = record.millis;
            }

            @Override
            public int viewType() {
                return VIEW_TYPE_EVENT_LEVEL_1;
            }
        }

        @SuppressLint("ParcelCreator")
        class EventLevel2Item extends BatteryRecord implements Item {
            public String text;

            public EventLevel2Item(BatteryRecord record) {
                this.millis = record.millis;
            }

            @Override
            public int viewType() {
                return VIEW_TYPE_EVENT_LEVEL_2;
            }
        }
    }


    public abstract static class ViewHolder<ITEM extends Item> extends RecyclerView.ViewHolder {
        public static final int COLOR_FG_MAIN = R.color.FG_0;
        public static final int COLOR_FG_SUB = R.color.FG_2;
        public static final int COLOR_FG_ALERT = R.color.Red_80_CARE;

        protected static final ThreadSafeReference<DateFormat> sTimeFormatRef = new ThreadSafeReference<DateFormat>() {
            @NonNull
            @Override
            public DateFormat onCreate() {
                return new SimpleDateFormat("HH:mm", Locale.getDefault());
            }
        };

        @SuppressWarnings("NotNullFieldNotInitialized")
        @NonNull
        protected ITEM mItem;

        public ViewHolder(@NonNull View itemView) {
            super(itemView);
        }

        public void bind(ITEM item) {
        }


        public static class HeaderHolder extends ViewHolder<Item.HeaderItem> {
            private final TextView mTitleTv;

            public HeaderHolder(@NonNull View itemView) {
                super(itemView);
                mTitleTv = itemView.findViewById(R.id.tv_title);
            }

            @Override
            public void bind(Item.HeaderItem item) {
                mItem = item;
                mTitleTv.setText(item.date);
            }
        }

        public static class NoDataHolder extends ViewHolder<Item.NoDataItem> {
            private final TextView mTitleTv;

            public NoDataHolder(@NonNull View itemView) {
                super(itemView);
                mTitleTv = itemView.findViewById(R.id.tv_title);
            }

            @Override
            public void bind(Item.NoDataItem item) {
                mItem = item;
                if (!TextUtils.isEmpty(item.text)) {
                    mTitleTv.setText(item.text);
                }
            }
        }

        public static class EventDumpHolder extends ViewHolder<Item.EventDumpItem> {

            private final TextView mTimeTv;
            private final TextView mTitleTv;
            private final TextView mTitleSub1;
            private final TextView mTitleSub2;
            private final TextView mMoreTv;
            private final ImageView mIndicatorIv;
            private final View mExpandView;

            private final View mHeaderEntryView;
            private final TextView mHeaderLeftTv;
            private final TextView mHeaderRightTv;
            private final TextView mHeaderDescTv;

            private final View mEntryViewThread;
            private final View mEntryView1;
            private final View mEntryView2;
            private final View mEntryView3;
            private final View mEntryView4;

            public EventDumpHolder(@NonNull View itemView) {
                super(itemView);
                mTimeTv = itemView.findViewById(R.id.tv_time);
                mTitleTv = itemView.findViewById(R.id.tv_title);
                mTitleSub1 = itemView.findViewById(R.id.tv_title_sub_1);
                mTitleSub2 = itemView.findViewById(R.id.tv_title_sub_2);
                mMoreTv = itemView.findViewById(R.id.tv_more);
                mIndicatorIv = itemView.findViewById(R.id.iv_indicator);
                mExpandView = itemView.findViewById(R.id.layout_expand);

                mHeaderEntryView = itemView.findViewById(R.id.layout_expand_entry_header);
                mHeaderLeftTv = mHeaderEntryView.findViewById(R.id.tv_header_left);
                mHeaderRightTv = mHeaderEntryView.findViewById(R.id.tv_header_right);
                mHeaderDescTv = mHeaderEntryView.findViewById(R.id.tv_desc);

                mEntryViewThread = itemView.findViewById(R.id.layout_expand_entry_thread);
                mEntryView1 = itemView.findViewById(R.id.layout_expand_entry_1);
                mEntryView2 = itemView.findViewById(R.id.layout_expand_entry_2);
                mEntryView3 = itemView.findViewById(R.id.layout_expand_entry_3);
                mEntryView4 = itemView.findViewById(R.id.layout_expand_entry_4);

                View.OnClickListener onClickListener = new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        mItem.expand = !mItem.expand;
                        updateView(mItem);
                    }
                };
                itemView.findViewById(R.id.layout_title).setOnClickListener(onClickListener);
                itemView.findViewById(R.id.layout_right).setOnClickListener(onClickListener);

                mEntryViewThread.setOnClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        long endMillis = mItem.record.millis;
                        StringBuilder sb = new StringBuilder();
                        long bgnMillis = mItem.record.millis - mItem.record.windowMillis;
                        SimpleDateFormat dateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.getDefault());

                        sb.append("线程异常: ").append(mItem.record.getBoolean(EXTRA_JIFFY_OVERHEAT, false))
                                .append("\n统计时长: ").append(Math.max((mItem.record.windowMillis) / (60 * 1000L), 1)).append("min")
                                .append("\n时间窗口: ").append(dateFormat.format(new Date(bgnMillis))).append(" ~ ").append(dateFormat.format(new Date(endMillis)));

                        for (int i = 0; i < mItem.record.threadInfoList.size(); i++) {
                            BatteryRecord.ReportRecord.ThreadInfo threadInfo = mItem.record.threadInfoList.get(i);
                            if (threadInfo != null) {
                                String status = threadInfo.stat;
                                switch (threadInfo.stat) {
                                    case "R":
                                        status = "Running";
                                        break;
                                    case "S":
                                        status = "Sleep";
                                        break;
                                    case "D":
                                        status = "Dead";
                                        break;
                                    default:
                                        break;
                                }
                                sb.append("\n\n线程 TOP ").append(i + 1).append(":").append(threadInfo.name)
                                        .append("\ntid: ").append(threadInfo.tid)
                                        .append("\n状态: ").append(status)
                                        .append("\nJiffy 开销: ").append(threadInfo.jiffies).append(", ").append(threadInfo.jiffies / Math.max(mItem.windowMillis / (60 * 1000L), 1)).append("/min")
                                        .append("\n运行时间: ").append(Math.max((threadInfo.jiffies * 10L) / (60 * 1000L), 1)).append("min").append(", 占整体统计时间 ").append(String.format(Locale.US, "%s", (threadInfo.jiffies * 10L * 100) / mItem.windowMillis)).append("%")
                                        .append("\nStackTrace: \n").append(threadInfo.extraInfo.get(BatteryRecord.ReportRecord.EXTRA_THREAD_STACK));
                            }
                        }

                        View layout = LayoutInflater.from(v.getContext()).inflate(R.layout.stats_battery_report, null);
                        TextView tv = layout.findViewById(R.id.tv_report);
                        tv.setText(sb.toString());
                        AlertDialog dialog = new AlertDialog.Builder(v.getContext())
                                .setTitle("线程详细信息")
                                .setPositiveButton("确定", null)
                                .setCancelable(true)
                                .setView(layout)
                                .create();
                        dialog.show();
                    }
                });
            }

            @Override
            public void bind(final Item.EventDumpItem item) {
                mItem = item;
                resetView();
                updateView(item);
            }

            private void resetView() {
                mExpandView.setVisibility(View.GONE);
                mHeaderEntryView.setVisibility(View.VISIBLE);
                mEntryViewThread.setVisibility(View.GONE);
                mEntryView1.setVisibility(View.GONE);
                mEntryView2.setVisibility(View.GONE);
                mEntryView3.setVisibility(View.GONE);
                mEntryView4.setVisibility(View.GONE);
            }

            @SuppressLint({"SetTextI18n", "CutPasteId"})
            private void updateView(Item.EventDumpItem item) {
                // Basic
                String title = "", desc = "";
                switch (TextUtils.isEmpty(item.record.scope) ? "" : item.record.scope) {
                    case CompositeMonitors.SCOPE_CANARY:
                        if (item.record.getBoolean(EXTRA_APP_FOREGROUND, false)) {
                            title += "前台 Polling 监控";
                            desc = "App 在前台时, 周期性地执行电量统计 (具体周期见时长)";
                        } else {
                            title += "待机功耗监控";
                            desc = "App 进入后台并持续一段时间后 (待机), 再次切换到前台时执行一次电量统计。";
                        }
                        break;
                    case CompositeMonitors.SCOPE_INTERNAL:
                        title += "Matrix 内部监控";
                        desc = "Matrix 自身电量开销的监控, 避免电量监控框架自身导致的耗电问题";
                        break;
                    case CompositeMonitors.SCOPE_OVERHEAT:
                        title += "Runnable 任务监控";
                        desc = "ThreadPool 等需要执行大量零碎 Runnable 的专项电量统计。";
                        break;
                    default:
                        title += ": " + item.record.scope;
                        desc = "缺乏描述";
                        break;
                }

                mTimeTv.setText(sTimeFormatRef.safeGet().format(new Date(item.millis)));
                mMoreTv.setRotation(item.expand ? 180 : 0);
                mExpandView.setVisibility(item.expand ? View.VISIBLE : View.GONE);
                mTitleTv.setText(title);
                mTitleSub1.setText(sTimeFormatRef.safeGet().format(new Date(item.millis - item.windowMillis)) + " ~ " + sTimeFormatRef.safeGet().format(new Date(item.millis)));
                if (item.record.isOverHeat()) {
                    mIndicatorIv.setImageLevel(4);
                    mTitleSub2.setText("#OVERHEAT");
                } else {
                    mIndicatorIv.setImageLevel(2);
                    mTitleSub2.setText("正常");
                }
                if (!item.expand) {
                    return;
                }

                // Header
                mHeaderLeftTv.setText("模式: " + item.scope);
                mHeaderRightTv.setText("时长: " + Math.max(1, (item.windowMillis) / (60 * 1000L)) + "min");
                mHeaderDescTv.setText(TextUtils.isEmpty(item.desc) ? desc : item.desc);

                // Thread Entry
                mEntryViewThread.setVisibility(!item.threadInfoList.isEmpty() ? View.VISIBLE : View.GONE);
                if (!item.threadInfoList.isEmpty()) {
                    boolean overHeat = item.record.getBoolean(EXTRA_JIFFY_OVERHEAT, false);
                    TextView tvTitle = mEntryViewThread.findViewById(R.id.tv_header_left);
                    tvTitle.setTextColor(tvTitle.getResources().getColor(overHeat ? COLOR_FG_ALERT : COLOR_FG_SUB));

                    LinearLayout entryGroup = mEntryViewThread.findViewById(R.id.layout_entry_group);
                    int reusableCount = entryGroup.getChildCount();
                    for (int i = 0; i < reusableCount; i++) {
                        entryGroup.getChildAt(i).setVisibility(View.GONE);
                    }
                    for (int i = 0; i < 5; i++) {
                        if (i < item.threadInfoList.size()) {
                            Item.EventDumpItem.ThreadInfo threadInfo = item.threadInfoList.get(i);
                            View entryItemView;
                            if (i < reusableCount) {
                                // 1. reuse existing entry view
                                entryItemView = entryGroup.getChildAt(i);
                            } else {
                                // 2. add new entry view
                                entryItemView = LayoutInflater.from(entryGroup.getContext()).inflate(R.layout.stats_item_event_dump_entry_subentry, entryGroup, false);
                                LinearLayout.LayoutParams layoutParams = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
                                layoutParams.topMargin = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 5, entryGroup.getContext().getResources().getDisplayMetrics());
                                entryGroup.addView(entryItemView, layoutParams);
                            }
                            // 3. update content
                            entryItemView.setVisibility(View.VISIBLE);
                            TextView left = entryItemView.findViewById(R.id.tv_key);
                            TextView right = entryItemView.findViewById(R.id.tv_value);
                            left.setVisibility(threadInfo != null ? View.VISIBLE : View.GONE);
                            right.setVisibility(threadInfo != null ? View.VISIBLE : View.GONE);

                            if (threadInfo != null) {
                                left.setText(threadInfo.name);
                                right.setText(threadInfo.tid + " / " + Math.max(1, (threadInfo.jiffies * 10) / (60 * 1000L)) + "min / " + threadInfo.stat);
                            }
                        }
                    }
                }

                // Entry 1 - Entry 4:
                // 1. entry section
                for (int i = 1; i <= 4; i++) {
                    final View entryView;
                    switch (i) {
                        case 1:
                            entryView = mEntryView1;
                            break;
                        case 2:
                            entryView = mEntryView2;
                            break;
                        case 3:
                            entryView = mEntryView3;
                            break;
                        case 4:
                            entryView = mEntryView4;
                            break;
                        default:
                            throw new IndexOutOfBoundsException("entryList section out of bound: " + i);
                    }

                    Item.EventDumpItem.EntryInfo entryInfo = i <= item.entryList.size() ? item.entryList.get(i - 1) : null;
                    entryView.setVisibility(entryInfo != null ? View.VISIBLE : View.GONE);
                    if (entryInfo != null) {
                        TextView left = entryView.findViewById(R.id.tv_header_left);
                        left.setText(entryInfo.name);

                        // 2. entry list
                        LinearLayout entryGroup = entryView.findViewById(R.id.layout_entry_group);
                        int reusableCount = entryGroup.getChildCount();
                        for (int j = 0; j < reusableCount; j++) {
                            entryGroup.getChildAt(j).setVisibility(View.GONE);
                        }

                        int entryIdx = 0, entryLimit = 6;
                        for (Map.Entry<String, String> entry : entryInfo.entries.entrySet()) {
                            entryIdx++;
                            if (entryIdx > entryLimit) {
                                break;
                            }
                            View entryItemView;
                            if (entryIdx < reusableCount) {
                                // 1. reuse existing entry view
                                entryItemView = entryGroup.getChildAt(entryIdx);
                            } else {
                                // 2. add new entry view
                                entryItemView = LayoutInflater.from(entryGroup.getContext()).inflate(R.layout.stats_item_event_dump_entry_subentry, entryGroup, false);
                                LinearLayout.LayoutParams layoutParams = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
                                layoutParams.topMargin = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 5, entryGroup.getContext().getResources().getDisplayMetrics());
                                entryGroup.addView(entryItemView, layoutParams);
                            }

                            entryItemView.setVisibility(View.VISIBLE);
                            TextView keyTv = entryItemView.findViewById(R.id.tv_key);
                            TextView valTv = entryItemView.findViewById(R.id.tv_value);
                            keyTv.setText(entry.getKey());
                            valTv.setText(entry.getValue());
                        }
                    }
                }
            }
        }

        public static class EventLevel1Holder extends ViewHolder<Item.EventLevel1Item> {

            private final TextView mTimeTv;
            private final TextView mTitleTv;

            public EventLevel1Holder(@NonNull View itemView) {
                super(itemView);
                mTimeTv = itemView.findViewById(R.id.tv_time);
                mTitleTv = itemView.findViewById(R.id.tv_title);
            }

            @Override
            public void bind(Item.EventLevel1Item item) {
                mItem = item;
                mTimeTv.setText(sTimeFormatRef.safeGet().format(new Date(item.millis)));
                mTitleTv.setText(item.text);
            }
        }

        public static class EventLevel2Holder extends ViewHolder<Item.EventLevel2Item> {

            private final TextView mTimeTv;
            private final TextView mTitleTv;

            public EventLevel2Holder(@NonNull View itemView) {
                super(itemView);
                mTimeTv = itemView.findViewById(R.id.tv_time);
                mTitleTv = itemView.findViewById(R.id.tv_title);
            }

            @Override
            public void bind(Item.EventLevel2Item item) {
                mItem = item;
                mTimeTv.setText(sTimeFormatRef.safeGet().format(new Date(item.millis)));
                mTitleTv.setText(item.text);
            }
        }

        public static class EventSimpleHolder extends ViewHolder<Item.EventSimpleItem> implements View.OnClickListener {

            private final TextView mTimeTv;
            private final TextView mTitleTv;

            public EventSimpleHolder(@NonNull View itemView) {
                super(itemView);
                mTimeTv = itemView.findViewById(R.id.tv_time);
                mTitleTv = itemView.findViewById(R.id.tv_title);
                itemView.findViewById(R.id.layout_title).setOnClickListener(this);
            }

            @Override
            public void bind(Item.EventSimpleItem item) {
                mItem = item;
                mTimeTv.setText(sTimeFormatRef.safeGet().format(new Date(item.millis)));
                mTitleTv.setText(item.event);
            }

            @Override
            public void onClick(View v) {
                View layout = LayoutInflater.from(v.getContext()).inflate(R.layout.stats_battery_report, null);
                TextView tv = layout.findViewById(R.id.tv_report);
                tv.setText(getDetailInfo());
                AlertDialog dialog = new AlertDialog.Builder(v.getContext())
                        .setTitle(mItem.event)
                        .setPositiveButton("确定", null)
                        .setCancelable(true)
                        .setView(layout)
                        .create();
                dialog.show();
            }

            protected String getDetailInfo() {
                return mItem.record.extras.toString();
            }
        }

        public static class EventBatteryHolder extends ViewHolder<Item.EventBatteryItem> {

            private final TextView mTimeTv;
            private final ImageView mIndicatorIv;
            private final TextView mTitleTv;

            public EventBatteryHolder(@NonNull View itemView) {
                super(itemView);
                mTimeTv = itemView.findViewById(R.id.tv_time);
                mIndicatorIv = itemView.findViewById(R.id.iv_indicator);
                mTitleTv = itemView.findViewById(R.id.tv_title);
            }

            @SuppressLint("SetTextI18n")
            @Override
            public void bind(Item.EventBatteryItem item) {
                mItem = item;
                mTimeTv.setText(sTimeFormatRef.safeGet().format(new Date(item.millis)));
                mTitleTv.setText(item.event);

                mIndicatorIv.setImageLevel(1);
                if (item.record.extras.containsKey("battery-low")) {
                    boolean lowBattery = item.record.getBoolean("battery-low", false);
                    mIndicatorIv.setImageLevel(lowBattery ? 4 : 2);
                    long pct = item.record.getDigit("battery-pct", -1);
                    mTitleTv.setText((lowBattery ? "电量低" : "电量恢复") + ((pct > 0 ? " (" + pct + "%)" : "")));
                    return;
                }
                if (item.record.extras.containsKey("battery-temp")) {
                    long temp = item.record.getDigit("battery-temp", -1);
                    if (temp != -1) {
                        mIndicatorIv.setImageLevel(3);
                    }
                    long pct = item.record.getDigit("battery-pct", -1);
                    mTitleTv.setText("电池温度: " + (temp > 0 ? temp / 10f : "/") + "°C" + ((pct > 0 ? " (" + pct + "%)" : "")));
                    return;
                }
                if (item.record.extras.containsKey("battery-pct")) {
                    long pct = item.record.getDigit("battery-pct", -1);
                    mTitleTv.setText("电量变化: " + (pct > 0 ? pct : "/") + "%");
                }
            }
        }
    }
}
