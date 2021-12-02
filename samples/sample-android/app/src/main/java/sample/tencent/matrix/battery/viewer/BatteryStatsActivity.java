package sample.tencent.matrix.battery.viewer;

import android.annotation.SuppressLint;
import android.os.Bundle;
import android.os.SystemClock;
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
                private final TextView mAEntryKey1;
                private final TextView mAEntryVal1;
                private final TextView mAEntryKey2;
                private final TextView mAEntryVal2;
                private final TextView mAEntryKey3;
                private final TextView mAEntryVal3;
                private final TextView mAEntryKey4;
                private final TextView mAEntryVal4;
                private final TextView mAEntryKey5;
                private final TextView mAEntryVal5;
                private final TextView mAEntryKey6;
                private final TextView mAEntryVal6;

                    private final View mBEntryView;
                private final TextView mBEntryLeftTv;
                private final TextView mBEntryKey1;
                private final TextView mBEntryVal1;
                private final TextView mBEntryKey2;
                private final TextView mBEntryVal2;
                private final TextView mBEntryKey3;
                private final TextView mBEntryVal3;
                private final TextView mBEntryKey4;
                private final TextView mBEntryVal4;
                private final TextView mBEntryKey5;
                private final TextView mBEntryVal5;
                private final TextView mBEntryKey6;
                private final TextView mBEntryVal6;

                    private final View mCEntryView;
                private final TextView mCEntryLeftTv;
                private final TextView mCEntryKey1;
                private final TextView mCEntryVal1;
                private final TextView mCEntryKey2;
                private final TextView mCEntryVal2;
                private final TextView mCEntryKey3;
                private final TextView mCEntryVal3;
                private final TextView mCEntryKey4;
                private final TextView mCEntryVal4;
                private final TextView mCEntryKey5;
                private final TextView mCEntryVal5;
                private final TextView mCEntryKey6;
                private final TextView mCEntryVal6;

                    private final View mDEntryView;
                private final TextView mDEntryLeftTv;
                private final TextView mDEntryKey1;
                private final TextView mDEntryVal1;
                private final TextView mDEntryKey2;
                private final TextView mDEntryVal2;
                private final TextView mDEntryKey3;
                private final TextView mDEntryVal3;
                private final TextView mDEntryKey4;
                private final TextView mDEntryVal4;
                private final TextView mDEntryKey5;
                private final TextView mDEntryVal5;
                private final TextView mDEntryKey6;
                private final TextView mDEntryVal6;


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
                    mAEntryKey1 = mAEntryView.findViewById(R.id.tv_key_1);
                    mAEntryVal1 = mAEntryView.findViewById(R.id.tv_value_1);
                    mAEntryKey2 = mAEntryView.findViewById(R.id.tv_key_2);
                    mAEntryVal2 = mAEntryView.findViewById(R.id.tv_value_2);
                    mAEntryKey3 = mAEntryView.findViewById(R.id.tv_key_3);
                    mAEntryVal3 = mAEntryView.findViewById(R.id.tv_value_3);
                    mAEntryKey4 = mAEntryView.findViewById(R.id.tv_key_4);
                    mAEntryVal4 = mAEntryView.findViewById(R.id.tv_value_4);
                    mAEntryKey5 = mAEntryView.findViewById(R.id.tv_key_5);
                    mAEntryVal5 = mAEntryView.findViewById(R.id.tv_value_5);
                    mAEntryKey6 = mAEntryView.findViewById(R.id.tv_key_6);
                    mAEntryVal6 = mAEntryView.findViewById(R.id.tv_value_6);

                    mBEntryView = itemView.findViewById(R.id.layout_expand_entry_2);
                    mBEntryLeftTv = mBEntryView.findViewById(R.id.tv_header_left);
                    mBEntryKey1   = mBEntryView.findViewById(R.id.tv_key_1);
                    mBEntryVal1   = mBEntryView.findViewById(R.id.tv_value_1);
                    mBEntryKey2   = mBEntryView.findViewById(R.id.tv_key_2);
                    mBEntryVal2   = mBEntryView.findViewById(R.id.tv_value_2);
                    mBEntryKey3   = mBEntryView.findViewById(R.id.tv_key_3);
                    mBEntryVal3   = mBEntryView.findViewById(R.id.tv_value_3);
                    mBEntryKey4   = mBEntryView.findViewById(R.id.tv_key_4);
                    mBEntryVal4   = mBEntryView.findViewById(R.id.tv_value_4);
                    mBEntryKey5   = mBEntryView.findViewById(R.id.tv_key_5);
                    mBEntryVal5   = mBEntryView.findViewById(R.id.tv_value_5);
                    mBEntryKey6   = mBEntryView.findViewById(R.id.tv_key_6);
                    mBEntryVal6   = mBEntryView.findViewById(R.id.tv_value_6);

                    mCEntryView = itemView.findViewById(R.id.layout_expand_entry_3);
                    mCEntryLeftTv = mCEntryView.findViewById(R.id.tv_header_left);
                    mCEntryKey1   = mCEntryView.findViewById(R.id.tv_key_1);
                    mCEntryVal1   = mCEntryView.findViewById(R.id.tv_value_1);
                    mCEntryKey2   = mCEntryView.findViewById(R.id.tv_key_2);
                    mCEntryVal2   = mCEntryView.findViewById(R.id.tv_value_2);
                    mCEntryKey3   = mCEntryView.findViewById(R.id.tv_key_3);
                    mCEntryVal3   = mCEntryView.findViewById(R.id.tv_value_3);
                    mCEntryKey4   = mCEntryView.findViewById(R.id.tv_key_4);
                    mCEntryVal4   = mCEntryView.findViewById(R.id.tv_value_4);
                    mCEntryKey5   = mCEntryView.findViewById(R.id.tv_key_5);
                    mCEntryVal5   = mCEntryView.findViewById(R.id.tv_value_5);
                    mCEntryKey6   = mCEntryView.findViewById(R.id.tv_key_6);
                    mCEntryVal6   = mCEntryView.findViewById(R.id.tv_value_6);

                    mDEntryView = itemView.findViewById(R.id.layout_expand_entry_4);
                    mDEntryLeftTv = mDEntryView.findViewById(R.id.tv_header_left);
                    mDEntryKey1   = mDEntryView.findViewById(R.id.tv_key_1);
                    mDEntryVal1   = mDEntryView.findViewById(R.id.tv_value_1);
                    mDEntryKey2   = mDEntryView.findViewById(R.id.tv_key_2);
                    mDEntryVal2   = mDEntryView.findViewById(R.id.tv_value_2);
                    mDEntryKey3   = mDEntryView.findViewById(R.id.tv_key_3);
                    mDEntryVal3   = mDEntryView.findViewById(R.id.tv_value_3);
                    mDEntryKey4   = mDEntryView.findViewById(R.id.tv_key_4);
                    mDEntryVal4   = mDEntryView.findViewById(R.id.tv_value_4);
                    mDEntryKey5   = mDEntryView.findViewById(R.id.tv_key_5);
                    mDEntryVal5   = mDEntryView.findViewById(R.id.tv_value_5);
                    mDEntryKey6   = mDEntryView.findViewById(R.id.tv_key_6);
                    mDEntryVal6   = mDEntryView.findViewById(R.id.tv_value_6);
                }

                @Override
                public void bind(final EventDumpItem item) {
                    resetView();
                    updateView(item);
                    itemView.setOnClickListener(new View.OnClickListener() {
                        @Override
                        public void onClick(View v) {
                            item.expand = !item.expand;
                            updateView(item);
                        }
                    });
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

                    // Thread Entry
                    mThreadEntryView.setVisibility(!item.threadInfoList.isEmpty() ? View.VISIBLE : View.GONE);
                    if (!item.threadInfoList.isEmpty()) {
                        for (int i = 1; i < 6; i++) {
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

                    mAEntryView.setVisibility(item.expand ? View.VISIBLE : View.GONE);
                    mBEntryView.setVisibility(item.expand ? View.VISIBLE : View.GONE);
                    mCEntryView.setVisibility(item.expand ? View.VISIBLE : View.GONE);
                    mDEntryView.setVisibility(item.expand ? View.VISIBLE : View.GONE);
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