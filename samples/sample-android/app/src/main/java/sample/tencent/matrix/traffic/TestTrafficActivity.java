package sample.tencent.matrix.traffic;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import com.tencent.matrix.plugin.Plugin;
import com.tencent.matrix.plugin.PluginListener;
import com.tencent.matrix.report.Issue;
import com.tencent.matrix.traffic.TrafficConfig;
import com.tencent.matrix.traffic.TrafficPlugin;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.HashMap;
import java.util.Map;

import sample.tencent.matrix.R;

public class TestTrafficActivity extends Activity {

    TextView totalTrafficTextView;
    TextView threadTextView;
    TextView stackTextView;
    TrafficPlugin trafficPlugin;
    boolean downloading = false;
    long totalTraffic = 0;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_traffic);
        initTraffic();
        totalTrafficTextView = findViewById(R.id.total_traffic);
        threadTextView = findViewById(R.id.traffic_thread);
        stackTextView = findViewById(R.id.thread_stack);
        ((Button)findViewById(R.id.test_traffic)).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                new Thread(new Runnable() {
                    @Override
                    public void run() {
                        downloading = true;
                        startTrafficCollector();
                        downloadAPkByHttpUrlConnection("https://dldir1.qq.com/weixin/android/weixin8018android2060_arm64.apk");
                        downloading = false;
                    }
                }, "Download-Thread").start();
            }
        });
    }

    void startTrafficCollector() {
        new Thread(new Runnable() {
            @Override
            public void run() {
                while (downloading) {
                    HashMap<String, String> trafficInfo = trafficPlugin.getTrafficInfoMap(TrafficPlugin.TYPE_GET_TRAFFIC_RX);
                    Map<String, String> stackTraceMap = trafficPlugin.getStackTraceMap();
                    for (Map.Entry<String, String> entry : trafficInfo.entrySet()) {
                        final String threadName = entry.getKey();
                        long traffic = Long.parseLong(entry.getValue());
                        final String stack = stackTraceMap.get(threadName);
                        totalTraffic += traffic;

                        runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                threadTextView.setText("TotalTraffic : " + totalTraffic + " Bytes");
                                totalTrafficTextView.setText("Thread Name : " + threadName);
                                stackTextView.setText("stack : " + stack);
                            }
                        });
                        trafficPlugin.clearTrafficInfo();
                    }

                    try {
                        Thread.sleep(1000);
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                }
            }
        }).start();
    }

    void initTraffic() {

        TrafficConfig trafficConfig = new TrafficConfig(true, true, true);
        //trafficConfig.addIgnoreSoFile("libmmkv.so");// whitelist
        trafficPlugin = new TrafficPlugin(trafficConfig);
        trafficPlugin.init(getApplication(), new PluginListener() {
            @Override
            public void onInit(Plugin plugin) {

            }

            @Override
            public void onStart(Plugin plugin) {

            }

            @Override
            public void onStop(Plugin plugin) {

            }

            @Override
            public void onDestroy(Plugin plugin) {

            }

            @Override
            public void onReportIssue(Issue issue) {

            }
        });

        trafficPlugin.start();
    }

    public void downloadAPkByHttpUrlConnection(String urlString) {
        try {
            URL url=new URL(urlString);
            HttpURLConnection connection= (HttpURLConnection) url.openConnection();
            connection.setRequestMethod("GET");
            connection.setConnectTimeout(5000);
            connection.connect();
            InputStream inputStream = connection.getInputStream();
            File file = new File(getFilesDir(), "test.apk");
            FileOutputStream fileOutputStream = new FileOutputStream(file);
            byte[] data=new byte[1024];
            int length;
            while ((length=inputStream.read(data)) != -1){
                fileOutputStream.write(data,0 , length);
            }
            inputStream.close();
            fileOutputStream.close();
            connection.disconnect();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}