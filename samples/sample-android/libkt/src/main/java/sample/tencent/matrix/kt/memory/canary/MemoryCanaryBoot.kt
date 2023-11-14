package sample.tencent.matrix.kt.memory.canary

import android.app.Application
import com.tencent.matrix.memory.canary.MemoryCanaryConfig
import com.tencent.matrix.memory.canary.monitor.AppBgSumPssMonitorConfig
import sample.tencent.matrix.kt.memory.canary.monitor.BackgroundMemoryMonitorBoot

/**
 * Created by Yves on 2021/11/5
 */
class MemoryCanaryBoot {
    companion object {
        private const val TAG = "MicroMsg.MemoryCanaryBoot"

        @JvmStatic
        fun configure(app: Application): MemoryCanaryConfig {
            return MemoryCanaryConfig(
                appBgSumPssMonitorConfigs = arrayOf(
                    BackgroundMemoryMonitorBoot.appStagedBgMemoryMonitorConfig,
                    BackgroundMemoryMonitorBoot.appDeepBgMemoryMonitorConfig
                ),
                processBgMemoryMonitorConfigs = arrayOf(
                    BackgroundMemoryMonitorBoot.procStagedBgMemoryMonitorConfig,
                    BackgroundMemoryMonitorBoot.procDeepBgMemoryMonitorConfig,

                    BackgroundMemoryMonitorBoot.procStagedBgMemoryMonitorConfig2,
                    BackgroundMemoryMonitorBoot.procStagedBgMemoryMonitorConfig3,
                    BackgroundMemoryMonitorBoot.procStagedBgMemoryMonitorConfig4,
                    BackgroundMemoryMonitorBoot.procStagedBgMemoryMonitorConfig5,
                )
            )
        }
    }
}