package com.tencent.matrix.batterycanary.stats.ui;

import android.annotation.SuppressLint;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import com.tencent.matrix.batterycanary.R;
import com.tencent.matrix.batterycanary.stats.BatteryRecord;

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

    final List<Item> dataList = new ArrayList<>();

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
        // Item item = dataList.get(position);
        // if (item instanceof HeaderItem) {
        //     return VIEW_TYPE_HEADER;
        // } else if (item instanceof ForegroundEventItem) {
        //     return VIEW_TYPE_EVENT_FOREGROUND;
        // } else {
        //     throw new IllegalStateException("Unknown view type: " + item);
        // }
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
            protected final ReportRecord record;
            public boolean expand = false;
            public String desc;

            public EventDumpItem(ReportRecord record) {
                this.millis = record.millis;
                this.record = record;
                this.id = record.id;
                this.event = record.event;
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
        static DateFormat sTimeFormat = new SimpleDateFormat("HH:mm", Locale.getDefault());

        @SuppressWarnings("NotNullFieldNotInitialized")
        @NonNull
        ITEM mItem;

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
            private final TextView mMoreTv;
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
                mMoreTv = itemView.findViewById(R.id.tv_more);
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
                        BatteryRecord.ReportRecord.ThreadInfo threadInfo = mItem.record.threadInfoList.get(0);
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

                            long endMillis = mItem.record.millis;
                            long bgnMillis = mItem.record.millis - mItem.record.windowMillis;
                            SimpleDateFormat dateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.getDefault());

                            StringBuilder sb = new StringBuilder("线程名: ").append(threadInfo.name)
                                    .append("\ntid: ").append(threadInfo.tid)
                                    .append("\n状态: ").append(status)
                                    .append("\nJiffy 开销: ").append(threadInfo.jiffies).append(", ").append(threadInfo.jiffies / Math.max(mItem.windowMillis / (60 * 1000L), 1)).append("/min")
                                    .append("\n运行时间: ").append(Math.max((threadInfo.jiffies * 10L) / (60 * 1000L), 1)).append("min").append(", 占整体统计时间 ").append(String.format(Locale.US, "%s", (threadInfo.jiffies * 10L * 100) / mItem.windowMillis)).append("%")
                                    .append("\n统计时间: ").append(Math.max((mItem.record.windowMillis) / (60 * 1000L), 1)).append("min").append(", ").append(dateFormat.format(new Date(bgnMillis))).append(" ~ ").append(dateFormat.format(new Date(endMillis)))
                                    .append("\n\nStackTrace: \n").append(threadInfo.extraInfo.get(BatteryRecord.ReportRecord.EXTRA_THREAD_STACK));

                            View layout = LayoutInflater.from(v.getContext()).inflate(R.layout.stats_battery_report, null);
                            TextView tv = layout.findViewById(R.id.tv_report);
                            tv.setText(sb.toString());
                            AlertDialog dialog = new AlertDialog.Builder(v.getContext())
                                    .setTitle("线程异常信息")
                                    .setPositiveButton("确定", null)
                                    .setCancelable(true)
                                    .setView(layout)
                                    .create();
                            dialog.show();
                        }
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
                mTimeTv.setText(sTimeFormat.format(new Date(item.millis)));
                mMoreTv.setText(item.expand ? "▼" : "▲");
                mExpandView.setVisibility(item.expand ? View.VISIBLE : View.GONE);
                if (!item.expand) {
                    return;
                }

                // Header
                mHeaderLeftTv.setText("模式: " + item.scope);
                mHeaderRightTv.setText("时长: " + Math.max(1, (item.windowMillis) / (60 * 1000L)) + "min");
                mHeaderDescTv.setText(item.desc);

                // Thread Entry
                mEntryViewThread.setVisibility(!item.threadInfoList.isEmpty() ? View.VISIBLE : View.GONE);
                if (!item.threadInfoList.isEmpty()) {
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
                mTimeTv.setText(sTimeFormat.format(new Date(item.millis)));
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
                mTimeTv.setText(sTimeFormat.format(new Date(item.millis)));
                mTitleTv.setText(item.text);
            }
        }
    }
}
