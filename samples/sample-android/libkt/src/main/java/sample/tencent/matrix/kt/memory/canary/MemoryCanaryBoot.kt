package sample.tencent.matrix.kt.memory.canary

import android.app.Application
import com.tencent.matrix.lifecycle.supervisor.SupervisorConfig
import com.tencent.matrix.memory.canary.MemoryCanaryConfig
import com.tencent.matrix.util.MatrixUtil
import sample.tencent.matrix.kt.memory.canary.monitor.BackgroundMemoryMonitorBoot
import sample.tencent.matrix.kt.memory.canary.monitor.SumPssMonitorBoot

/**
 * Created by Yves on 2021/11/5
 */
class MemoryCanaryBoot {
    companion object {
        private const val TAG = "MicroMsg.MemoryCanaryBoot"

        @JvmStatic
        fun configure(app: Application): MemoryCanaryConfig {
            return MemoryCanaryConfig(
                supervisorConfig = getSupervisorConfig(app),
                sumPssMonitorConfig = SumPssMonitorBoot.config,
                backgroundMemoryMonitorConfig = BackgroundMemoryMonitorBoot.config
            )
        }

        private fun getSupervisorConfig(app: Application): SupervisorConfig {
            return SupervisorConfig(MatrixUtil.getPackageName(app))
        }
    }
}