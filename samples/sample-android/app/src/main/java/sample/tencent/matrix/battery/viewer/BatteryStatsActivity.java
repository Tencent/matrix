package sample.tencent.matrix.battery.viewer;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import java.util.ArrayList;
import java.util.List;

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
        dataList.add(new BatteryStatsAdapter.EventDumpItem());
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
            @Override
            public int viewType() {
                return VIEW_TYPE_EVENT_DUMP;
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


        public abstract static class ViewHolder extends  RecyclerView.ViewHolder {
            public ViewHolder(@NonNull View itemView) {
                super(itemView);
            }

            public static class HeaderHolder extends ViewHolder {
                public HeaderHolder(@NonNull View itemView) {
                    super(itemView);
                }
            }

            public static class EventDumpHolder extends ViewHolder {
                public EventDumpHolder(@NonNull View itemView) {
                    super(itemView);
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