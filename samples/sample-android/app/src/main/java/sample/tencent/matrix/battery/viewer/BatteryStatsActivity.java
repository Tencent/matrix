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
            public String scope;
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
            @SuppressWarnings("NotNullFieldNotInitialized")
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
                    mEntryViewThread.setVisibility(View.GONE);
                    mEntryView1.setVisibility(View.GONE);
                    mEntryView2.setVisibility(View.GONE);
                    mEntryView3.setVisibility(View.GONE);
                    mEntryView4.setVisibility(View.GONE);
                }

                @SuppressLint({"SetTextI18n", "CutPasteId"})
                private void updateView(EventDumpItem item) {
                    mMoreTv.setText(item.expand ? "▼" : "▲");
                    mExpandView.setVisibility(item.expand ? View.VISIBLE : View.GONE);
                    if (!item.expand) {
                        return;
                    }

                    // Header
                    mHeaderLeftTv.setText("模式: " + item.scope);
                    mHeaderRightTv.setText("时长: " + Math.max(1, (item.windowMillis * 10) / (60 * 1000L)) + "min");
                    mHeaderDescTv.setText(item.desc);

                    // Thread Entry
                    mEntryViewThread.setVisibility(!item.threadInfoList.isEmpty() ? View.VISIBLE : View.GONE);
                    if (!item.threadInfoList.isEmpty()) {
                        Function<Integer, View> findEntryItemView = new Function<Integer, View>() {
                            @Override
                            public View apply(Integer idx) {
                                switch (idx) {
                                    case 1:
                                        return mEntryViewThread.findViewById(R.id.layout_entry_1);
                                    case 2:
                                        return mEntryViewThread.findViewById(R.id.layout_entry_2);
                                    case 3:
                                        return mEntryViewThread.findViewById(R.id.layout_entry_3);
                                    case 4:
                                        return mEntryViewThread.findViewById(R.id.layout_entry_4);
                                    case 5:
                                        return mEntryViewThread.findViewById(R.id.layout_entry_5);
                                    default:
                                        throw new IndexOutOfBoundsException("threadInfo out of bound: " + idx);
                                }
                            }
                        };
                        for (int i = 1; i <= 5; i++) {
                            View entryItemView = findEntryItemView.apply(i);
                            TextView left = entryItemView.findViewById(R.id.tv_key);
                            TextView right = entryItemView.findViewById(R.id.tv_value);
                            EventDumpItem.ThreadInfo threadInfo = i <= item.threadInfoList.size() ?  item.threadInfoList.get(i - 1) : null;
                            left.setVisibility(threadInfo != null ? View.VISIBLE : View.GONE);
                            right.setVisibility(threadInfo != null ? View.VISIBLE : View.GONE);
                            if (threadInfo != null) {
                                left.setText(threadInfo.name);
                                right.setText(threadInfo.tid + " / " + Math.max(1, (threadInfo.jiffies * 10) / (60 * 1000L)) + "min / " + threadInfo.stat);
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