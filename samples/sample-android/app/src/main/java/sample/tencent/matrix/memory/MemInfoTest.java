package sample.tencent.matrix.memory;

import com.tencent.matrix.util.MemInfo;
import com.tencent.matrix.util.MatrixLog;

import java.util.Arrays;

/**
 * Created by Yves on 2021/10/29
 */
public class MemInfoTest {
    private static final String TAG = "Matrix.sample.MemInfoTest";
    public static void test() {
        MemInfo info = new MemInfo();
        MatrixLog.d(TAG, "new %s", info.toJson());

        info = MemInfo.getCurrentProcessMemInfo();
        MatrixLog.d(TAG, "getCurrentProcessMemInfo %s", info);

        info = MemInfo.getCurrentProcessFullMemInfo();
        MatrixLog.d(TAG, "getCurrentProcessFullMemInfo %s", info.toJson());

        info = MemInfo.getCurrentProcessMemInfoWithPss();
        MatrixLog.d(TAG, "getCurrentProcessMemInfoWithPss %s", info);

        info = MemInfo.getCurrentProcessMemInfoWithAmsPss();
        MatrixLog.d(TAG, "getCurrentProcessMemInfoWithAmsPss %s", info);

        MemInfo[] infos = MemInfo.getAllProcessPss();
        MatrixLog.d(TAG, "getAllProcessPss %s", Arrays.toString(infos));
    }
}
