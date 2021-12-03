package sample.tencent.matrix.battery.viewer;

import android.annotation.SuppressLint;
import android.os.Bundle;
import android.os.SystemClock;
import android.util.ArrayMap;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.arch.core.util.Function;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import sample.tencent.matrix.R;

public class BatteryStatsActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_battery_stats);

        RecyclerView recyclerView = findViewById(R.id.rv_battery_stats);
        View pinnedHeader = findViewById(R.id.header_pinned);

        recyclerView.setLayoutManager(new LinearLayoutManager(this));
        BatteryStatsAdapter adapter = new BatteryStatsAdapter();
        recyclerView.setAdapter(adapter);


        // mocking
        List<BatteryStatsAdapter.Item> dataList = adapter.getDataList();
        dataList.add(new BatteryStatsAdapter.HeaderItem());

        BatteryStatsAdapter.EventDumpItem dumpItem = new BatteryStatsAdapter.EventDumpItem();
        dumpItem.threadInfoList = new ArrayList<>();
        for (int i = 0; i < 5; i++) {
            BatteryStatsAdapter.EventDumpItem.ThreadInfo threadInfo = new BatteryStatsAdapter.EventDumpItem.ThreadInfo();
            threadInfo.stat = "R" + i;
            threadInfo.tid = 10000 + i;
            threadInfo.name = "ThreadName_" + i;
            threadInfo.jiffies = SystemClock.currentThreadTimeMillis() / 10;
            dumpItem.threadInfoList.add(0, threadInfo);
        }
        dumpItem.entryList = new ArrayList<>();
        for (int i = 0; i < 4; i++) {
            BatteryStatsAdapter.EventDumpItem.EntryInfo entryInfo = new BatteryStatsAdapter.EventDumpItem.EntryInfo();
            entryInfo.name = "Entry Name " + i;
            entryInfo.entries = new ArrayMap<>();
            entryInfo.entries.put("Key 1", "Value 1");
            entryInfo.entries.put("Key 2", "Value 2");
            entryInfo.entries.put("Key 3", "Value 3");
            dumpItem.entryList.add(entryInfo);
        }
        dataList.add(dumpItem);

        dataList.add(new BatteryStatsAdapter.EventLevel1Item());
        for (int i = 0; i < 10; i++) {
            dataList.add(new BatteryStatsAdapter.EventLevel2Item());
        }
        dataList.add(new BatteryStatsAdapter.EventLevel1Item());
        dataList.add(new BatteryStatsAdapter.EventDumpItem());

        dataList.add(new BatteryStatsAdapter.HeaderItem());
        dataList.add(new BatteryStatsAdapter.EventLevel1Item());
        for (int i = 0; i < 50; i++) {
            dataList.add(new BatteryStatsAdapter.EventLevel2Item());
        }
        dataList.add(new BatteryStatsAdapter.EventLevel1Item());
        dataList.add(new BatteryStatsAdapter.EventDumpItem());
        adapter.notifyItemRangeChanged(0, dataList.size());
    }


    public static class BatteryStatsAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder> {

        public static final int VIEW_TYPE_HEADER = 0;
        public static final int VIEW_TYPE_EVENT_DUMP = 1;
        public static final int VIEW_TYPE_EVENT_LEVEL_1 = 2;
        public static final int VIEW_TYPE_EVENT_LEVEL_2 = 3;

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
            ((ViewHolder) holder).bind(dataList.get(position));
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

        public static abstract class Item {
            public long millis;
            public abstract int viewType();
        }

        public static class HeaderItem extends Item {
            @Override
            public int viewType() {
                return VIEW_TYPE_HEADER;
            }
        }

        public static class EventDumpItem extends Item {
            public long id;
            public boolean expand = false;
            public String mode;
            public String desc;
            public long windowMillis;
            public List<ThreadInfo> threadInfoList = Collections.emptyList();
            public List<EntryInfo> entryList = Collections.emptyList();

            @Override
            public int viewType() {
                return VIEW_TYPE_EVENT_DUMP;
            }

            public static class ThreadInfo {
                public int tid;
                public String name;
                public String stat;
                public long jiffies;
            }

            public static class EntryInfo {
                public String name;
                public String desc;
                public String stat;
                public Map<String, String> entries = Collections.emptyMap();
            }
        }

        public static class EventLevel1Item extends Item {
            @Override
            public int viewType() {
                return VIEW_TYPE_EVENT_LEVEL_1;
            }
        }

        public static class EventLevel2Item extends Item {
            @Override
            public int viewType() {
                return VIEW_TYPE_EVENT_LEVEL_2;
            }
        }


        public abstract static class ViewHolder<ITEM extends Item> extends  RecyclerView.ViewHolder {
            @NonNull ITEM mItem;
            public ViewHolder(@NonNull View itemView) {
                super(itemView);
            }
            public void bind(ITEM item) {}

            public static class HeaderHolder extends ViewHolder<HeaderItem> {
                public HeaderHolder(@NonNull View itemView) {
                    super(itemView);
                }
            }

            public static class EventDumpHolder extends ViewHolder<EventDumpItem> {

                private final View mExpandView;

                private final View mHeaderEntryView;
                private final TextView mHeaderLeftTv;
                private final TextView mHeaderRightTv;
                private final TextView mHeaderDescTv;

                private final View mThreadEntryView;
                private final TextView mThreadEntryLeftTv;
                private final TextView mThreadRightTv;
                private final TextView mThreadEntryKey1;
                private final TextView mThreadEntryVal1;
                private final TextView mThreadEntryKey2;
                private final TextView mThreadEntryVal2;
                private final TextView mThreadEntryKey3;
                private final TextView mThreadEntryVal3;
                private final TextView mThreadEntryKey4;
                private final TextView mThreadEntryVal4;
                private final TextView mThreadEntryKey5;
                private final TextView mThreadEntryVal5;

                    private final View mAEntryView;
                private final TextView mAEntryLeftTv;


                    private final View mBEntryView;
                private final TextView mBEntryLeftTv;


                    private final View mCEntryView;
                private final TextView mCEntryLeftTv;


                    private final View mDEntryView;
                private final TextView mDEntryLeftTv;



                public EventDumpHolder(@NonNull View itemView) {
                    super(itemView);
                    mExpandView = itemView.findViewById(R.id.layout_expand);

                    mHeaderEntryView = itemView.findViewById(R.id.layout_expand_entry_header);
                    mHeaderLeftTv = mHeaderEntryView.findViewById(R.id.tv_header_left);
                    mHeaderRightTv = mHeaderEntryView.findViewById(R.id.tv_header_right);
                    mHeaderDescTv = mHeaderEntryView.findViewById(R.id.tv_desc);

                    mThreadEntryView = itemView.findViewById(R.id.layout_expand_entry_thread);
                    mThreadEntryLeftTv = mThreadEntryView.findViewById(R.id.tv_header_left);
                    mThreadRightTv = mThreadEntryView.findViewById(R.id.tv_header_right);
                    mThreadEntryKey1 = mThreadEntryView.findViewById(R.id.tv_key_1);
                    mThreadEntryVal1 = mThreadEntryView.findViewById(R.id.tv_value_1);
                    mThreadEntryKey2 = mThreadEntryView.findViewById(R.id.tv_key_2);
                    mThreadEntryVal2 = mThreadEntryView.findViewById(R.id.tv_value_2);
                    mThreadEntryKey3 = mThreadEntryView.findViewById(R.id.tv_key_3);
                    mThreadEntryVal3 = mThreadEntryView.findViewById(R.id.tv_value_3);
                    mThreadEntryKey4 = mThreadEntryView.findViewById(R.id.tv_key_4);
                    mThreadEntryVal4 = mThreadEntryView.findViewById(R.id.tv_value_4);
                    mThreadEntryKey5 = mThreadEntryView.findViewById(R.id.tv_key_5);
                    mThreadEntryVal5 = mThreadEntryView.findViewById(R.id.tv_value_5);

                    mAEntryView = itemView.findViewById(R.id.layout_expand_entry_1);
                    mAEntryLeftTv =    mAEntryView.findViewById(R.id.tv_header_left);

                    mBEntryView = itemView.findViewById(R.id.layout_expand_entry_2);
                    mBEntryLeftTv = mBEntryView.findViewById(R.id.tv_header_left);


                    mCEntryView = itemView.findViewById(R.id.layout_expand_entry_3);
                    mCEntryLeftTv = mCEntryView.findViewById(R.id.tv_header_left);


                    mDEntryView = itemView.findViewById(R.id.layout_expand_entry_4);
                    mDEntryLeftTv = mDEntryView.findViewById(R.id.tv_header_left);

                    itemView.setOnClickListener(new View.OnClickListener() {
                        @Override
                        public void onClick(View v) {
                            mItem.expand = !mItem.expand;
                            updateView(mItem);
                        }
                    });
                }

                @Override
                public void bind(final EventDumpItem item) {
                    mItem = item;
                    resetView();
                    updateView(item);
                }

                private void resetView() {
                    mExpandView.setVisibility(View.GONE);
                    mHeaderEntryView.setVisibility(View.VISIBLE);
                    mThreadEntryView.setVisibility(View.GONE);

                    mAEntryView.setVisibility(View.GONE);
                    mAEntryLeftTv.setText("设备状态");

                    mBEntryView.setVisibility(View.GONE);
                    mBEntryLeftTv.setText("应用状态");

                    mCEntryView.setVisibility(View.GONE);
                    mCEntryLeftTv.setText("服务调用");
                }

                @SuppressLint("SetTextI18n")
                private void updateView(EventDumpItem item) {
                    mExpandView.setVisibility(item.expand ? View.VISIBLE : View.GONE);
                    if (!item.expand) {
                        return;
                    }

                    // Header
                    mHeaderLeftTv.setText("模式: " + item.mode);
                    mHeaderRightTv.setText("时长: " + Math.max(1, (item.windowMillis * 10) / (60 * 1000L)) + "min");
                    mHeaderDescTv.setText(item.desc);

                    // Thread Entry
                    mThreadEntryView.setVisibility(!item.threadInfoList.isEmpty() ? View.VISIBLE : View.GONE);
                    if (!item.threadInfoList.isEmpty()) {
                        for (int i = 1; i <= 5; i++) {
                            TextView left, right;
                            switch (i) {
                                case 1:
                                    left = mThreadEntryKey1;
                                    right = mThreadEntryVal1;
                                    break;
                                case 2:
                                    left = mThreadEntryKey2;
                                    right = mThreadEntryVal2;
                                    break;
                                case 3:
                                    left = mThreadEntryKey3;
                                    right = mThreadEntryVal3;
                                    break;
                                case 4:
                                    left = mThreadEntryKey4;
                                    right = mThreadEntryVal4;
                                    break;
                                case 5:
                                    left = mThreadEntryKey5;
                                    right = mThreadEntryVal5;
                                    break;
                                default:
                                    throw new IndexOutOfBoundsException("threadInfo out of bound: " + i);
                            }

                            EventDumpItem.ThreadInfo threadInfo = i <= item.threadInfoList.size() ?  item.threadInfoList.get(i - 1) : null;
                            left.setVisibility(threadInfo != null ? View.VISIBLE : View.GONE);
                            right.setVisibility(threadInfo != null ? View.VISIBLE : View.GONE);
                            if (threadInfo != null) {
                                left.setText(threadInfo.name);
                                right.setText(threadInfo.tid + " / " + Math.max(1, (threadInfo.jiffies * 10) / (60 * 1000L)) + "min / " + threadInfo.stat);
                            }
                        }
                    }

                    // Entry A - D:
                    // 1. entry section
                    for (int i = 1; i <= 4; i++) {
                        final View entryView;
                        switch (i) {
                            case 1:
                                entryView = mAEntryView;
                                break;
                            case 2:
                                entryView = mBEntryView;
                                break;
                            case 3:
                                entryView = mCEntryView;
                                break;
                            case 4:
                                entryView = mDEntryView;
                                break;
                            default:
                                throw new IndexOutOfBoundsException("entryList section out of bound: " + i);
                        }

                        EventDumpItem.EntryInfo entryInfo = i <= item.entryList.size() ?  item.entryList.get(i - 1) : null;
                        entryView.setVisibility(entryInfo != null ? View.VISIBLE : View.GONE);
                        if (entryInfo != null) {
                            TextView left = entryView.findViewById(R.id.tv_header_left);
                            left.setText(entryInfo.name);

                            // 2. entry list
                            Function<Integer, View> findEntryItemView = new Function<Integer, View>() {
                                @Override
                                public View apply(Integer idx) {
                                    switch (idx) {
                                        case 1:
                                            return entryView.findViewById(R.id.layout_entry_1);
                                        case 2:
                                            return entryView.findViewById(R.id.layout_entry_2);
                                        case 3:
                                            return entryView.findViewById(R.id.layout_entry_3);
                                        case 4:
                                            return entryView.findViewById(R.id.layout_entry_4);
                                        case 5:
                                            return entryView.findViewById(R.id.layout_entry_5);
                                        case 6:
                                            return entryView.findViewById(R.id.layout_entry_6);
                                        default:
                                            throw new IndexOutOfBoundsException("entryList key-value out of bound: " + idx);
                                    }
                                }
                            };

                            int entryIdx = 0, entryLimit = 6;
                            for (Map.Entry<String, String> entry : entryInfo.entries.entrySet()) {
                                entryIdx++;
                                if (entryIdx > entryLimit) {
                                    break;
                                }
                                View entryItemView = findEntryItemView.apply(entryIdx);
                                entryItemView.setVisibility(View.VISIBLE);
                                TextView keyTv = entryItemView.findViewById(R.id.tv_key);
                                TextView valTv = entryItemView.findViewById(R.id.tv_value);
                                keyTv.setText(entry.getKey());
                                valTv.setText(entry.getValue());
                            }

                            for (int j = entryIdx + 1; j <= entryLimit; j++) {
                                View entryItemView = findEntryItemView.apply(j);
                                entryItemView.setVisibility(View.GONE);
                            }
                        }
                    }
                }
            }

            public static class EventLevel1Holder extends ViewHolder {
                public EventLevel1Holder(@NonNull View itemView) {
                    super(itemView);
                }
            }
            public static class EventLevel2Holder extends ViewHolder {
                public EventLevel2Holder(@NonNull View itemView) {
                    super(itemView);
                }
            }
        }
    }

}