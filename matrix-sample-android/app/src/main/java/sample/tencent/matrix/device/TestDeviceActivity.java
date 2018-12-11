package sample.tencent.matrix.device;

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.ActivityManager;
import android.content.ComponentCallbacks;
import android.content.ComponentCallbacks2;
import android.content.Context;
import android.content.res.Configuration;
import android.os.Build;
import android.os.Bundle;
import android.os.Debug;
import android.support.annotation.Nullable;
import android.view.View;

import com.tencent.matrix.memorycanary.util.DebugMemoryInfoUtil;
import com.tencent.matrix.util.DeviceUtil;
import com.tencent.matrix.util.MatrixLog;
import com.tencent.matrix.memorycanary.IFragmentActivity;

import org.json.JSONObject;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

import sample.tencent.matrix.R;

public class TestDeviceActivity extends Activity implements IFragmentActivity {
    private static final String TAG = "Matrix.TestDeviceActivit";
    private List<byte[]> mTest = new ArrayList<>();

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.device);
        MatrixLog.i(TAG, "onCreate");
        getApplicationContext().registerComponentCallbacks(mComponentCallback);
        getApplicationContext().registerComponentCallbacks(mMemoryCallback);
    }

    private final ComponentCallbacks2 mComponentCallback = new ComponentCallbacks2() {
        @Override
        public void onTrimMemory(final int i) {
            switch (i) {
                case TRIM_MEMORY_RUNNING_CRITICAL:
                    break;
                case TRIM_MEMORY_MODERATE:
                    break;
                case TRIM_MEMORY_COMPLETE:
                    break;
                default:
                    return;
            }

            ActivityManager.MemoryInfo memInfo = new ActivityManager.MemoryInfo();
            ActivityManager am = (ActivityManager) getApplicationContext().getSystemService(Context.ACTIVITY_SERVICE);
            am.getMemoryInfo(memInfo);

            MatrixLog.w(TAG, "MemoryCanary, flag:" + i);
            long avai = DeviceUtil.getAvailMemory(getApplicationContext());
            long free = DeviceUtil.getMemFree();
            long total = DeviceUtil.getTotalMemory(getApplicationContext()) / 1024;
            Debug.MemoryInfo memoryInfo = DeviceUtil.getAppMemory(getApplicationContext());
            long pss = DebugMemoryInfoUtil.getMemoryStat("summary.total-pss", memoryInfo);
            long thre = memInfo.threshold / 1024;
            MatrixLog.w(TAG, "MemoryCanary onTrim total:" + total + ", avai:" + avai + ", free:" + free + ", pss:" + pss + ", thre:" + thre
                + ", isLow:" + memInfo.lowMemory);
        }

        @Override
        public void onConfigurationChanged(Configuration configuration) {

        }

        @Override
        public void onLowMemory() {
            ActivityManager.MemoryInfo memInfo = new ActivityManager.MemoryInfo();
            ActivityManager am = (ActivityManager) getApplicationContext().getSystemService(Context.ACTIVITY_SERVICE);
            am.getMemoryInfo(memInfo);

            long avai = DeviceUtil.getAvailMemory(getApplicationContext());
            long free = DeviceUtil.getMemFree();
            long total = DeviceUtil.getTotalMemory(getApplicationContext()) / 1024;
            Debug.MemoryInfo memoryInfo = DeviceUtil.getAppMemory(getApplicationContext());
            long pss = DebugMemoryInfoUtil.getMemoryStat("summary.total-pss", memoryInfo);
            long thre = memInfo.threshold / 1024;
            MatrixLog.w(TAG, "MemoryCanary onTrimLow total:" + total + ", avai:" + avai + ", free:" + free + ", pss:" + pss + ", thre:" + thre
                    + ", isLow:" + memInfo.lowMemory);
        }
    };

    private final ComponentCallbacks mMemoryCallback = new ComponentCallbacks() {
        @Override
        public void onConfigurationChanged(Configuration configuration) {

        }

        @Override
        public void onLowMemory() {
            ActivityManager.MemoryInfo memInfo = new ActivityManager.MemoryInfo();
            ActivityManager am = (ActivityManager) getApplicationContext().getSystemService(Context.ACTIVITY_SERVICE);
            am.getMemoryInfo(memInfo);

            long avai = DeviceUtil.getAvailMemory(getApplicationContext());
            long free = DeviceUtil.getMemFree();
            long total = DeviceUtil.getTotalMemory(getApplicationContext()) / 1024;
            Debug.MemoryInfo memoryInfo = DeviceUtil.getAppMemory(getApplicationContext());
            long pss = DebugMemoryInfoUtil.getMemoryStat("summary.total-pss", memoryInfo);
            long thre = memInfo.threshold / 1024;
            MatrixLog.w(TAG, "MemoryCanary onLowMemory total:" + total + ", avai:" + avai + ", free:" + free + ", pss:" + pss + ", thre:" + thre
                    + ", isLow:" + memInfo.lowMemory);
        }
    };

    @TargetApi(Build.VERSION_CODES.M)
    public void onClick(View v) {
        if (v.getId() == R.id.total) {
            int dalvik = (int)DeviceUtil.getDalvikHeap();
            Runtime runtime = Runtime.getRuntime();
            MatrixLog.i(TAG, "MemoryCanary dalvik:" + dalvik + ", limit:" + runtime.maxMemory()/1024);

            long start = System.currentTimeMillis();
            long nh = Debug.getNativeHeapSize() / 1024;
            long nh2 = Debug.getNativeHeapAllocatedSize()/ 1024;
            long nh3 = Debug.getNativeHeapFreeSize()/1024;
            MatrixLog.i(TAG, "MemoryCanary nh:" + nh + ", n alloc:" + nh2 + ", n free:" + nh3 + ", cost:" + (System.currentTimeMillis() - start));

            long avai = DeviceUtil.getAvailMemory(getApplicationContext());
            long free = DeviceUtil.getMemFree();
            long total = DeviceUtil.getTotalMemory(getApplicationContext()) / 1024;
            Debug.MemoryInfo memoryInfo = DeviceUtil.getAppMemory(getApplicationContext());
            long pss = DebugMemoryInfoUtil.getMemoryStat("summary.total-pss", memoryInfo);
            long thre = DeviceUtil.getLowMemoryThresold(getApplicationContext()) / 1024;
            MatrixLog.w(TAG, "MemoryCanary total:" + total + ", avai:" + avai + ", free:" + free + ", pss:" + pss + ", thre:" + thre);

        } else if (v.getId() == R.id.total_mem) {
            DeviceUtil.getTotalMemory(getApplicationContext());
        } else if (v.getId() == R.id.app_cpu) {
            DeviceUtil.getAppCpuRate();
        } else if (v.getId() == R.id.app_mem) {
            DeviceUtil.getAppMemory(getApplicationContext());
        } else if(v.getId() == R.id.add_mem) {
            byte[] dump = new byte[10*1024*1024];
            byte c = 'c';
            for(int i = 0 ; i < 10*1024*1024; ) {
                dump[i] = c;
                c++;
                i += 1024 * 100;
            }
            mTest.add(dump);
            MatrixLog.i(TAG, "MemoryCanary done");
        }
    }

    @Override
    public int hashCodeOfCurrentFragment() {
        return getClass().hashCode();
    }

    @Override
    public String fragmentName() {
        return getClass().getSimpleName();
    }

}
