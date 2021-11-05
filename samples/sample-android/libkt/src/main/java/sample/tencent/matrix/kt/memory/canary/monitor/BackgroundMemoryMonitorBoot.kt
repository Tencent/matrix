package sample.tencent.matrix.kt.memory.canary.monitor

import com.tencent.matrix.memory.canary.BuildConfig
import com.tencent.matrix.memory.canary.MemInfo
import com.tencent.matrix.memory.canary.monitor.BackgroundMemoryMonitorConfig
import com.tencent.matrix.util.MatrixLog

/**
 * Created by Yves on 2021/11/5
 */
object BackgroundMemoryMonitorBoot {

    private const val TAG = "Matrix.sample.BackgroundMemoryMonitor"

    private val baseActivities = arrayOf(
        "sample.tencent.matrix.MainActivity",
        "sample.tencent.matrix.SplashActivity",
    )

    init {
        if (BuildConfig.DEBUG) {
            baseActivities.forEach { Class.forName(it) }
        }
    }

    private val reporter: (MemInfo, Boolean) -> Unit = { memInfo, isBusy ->
        if (isBusy) {
            MatrixLog.d(TAG, "report on busy: $memInfo")
        } else {
            MatrixLog.d(TAG, "report on free: $memInfo")
        }
    }

    internal val config = BackgroundMemoryMonitorConfig(
        reportCallback = reporter,
        baseActivities = baseActivities,
        javaThresholdByte = 10 * 1000L,
        nativeThresholdByte = 10 * 1024L,
    )
}