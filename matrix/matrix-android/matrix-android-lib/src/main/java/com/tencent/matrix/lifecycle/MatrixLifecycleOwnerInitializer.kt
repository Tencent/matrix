package com.tencent.matrix.lifecycle

import android.annotation.SuppressLint
import android.app.Application
import androidx.annotation.NonNull
import com.tencent.matrix.lifecycle.MatrixLifecycleOwnerInitializer.Companion.init
import com.tencent.matrix.lifecycle.owners.ForegroundServiceLifecycleOwner
import com.tencent.matrix.lifecycle.owners.OverlayWindowLifecycleOwner
import com.tencent.matrix.lifecycle.owners.ProcessUILifecycleOwner
import com.tencent.matrix.lifecycle.supervisor.SupervisorConfig
import com.tencent.matrix.util.MatrixLog
import com.tencent.matrix.util.safeLet

/**
 * All feature that would change the origin Matrix behavior is disabled by default.
 */
data class MatrixLifecycleConfig(
    val supervisorConfig: SupervisorConfig = SupervisorConfig(),
    /**
     * Injects Service#mActivityManager if true
     */
    val enableFgServiceMonitor: Boolean = false,
    /**
     * Injects WindowManagerGlobal#mRoots if true
     */
    val enableOverlayWindowMonitor: Boolean = false,

    val lifecycleThreadConfig: LifecycleThreadConfig = LifecycleThreadConfig(),

    val enableLifecycleLogger: Boolean = false
)

/**
 * You should init [com.tencent.matrix.Matrix] or call [init] manually before creating any Activity
 * Created by Yves on 2021/9/14
 */
class MatrixLifecycleOwnerInitializer {
    companion object {
        private const val TAG = "Matrix.ProcessLifecycleOwnerInit"

        @Volatile
        private var inited = false

        @JvmStatic
        fun init(
            @NonNull app: Application,
            config: MatrixLifecycleConfig
        ) {
            if (inited) {
                return
            }
            inited = true
            if (hasCreatedActivities()) {
                ("Matrix Warning: Matrix might be inited after launching first Activity, " +
                        "which would disable some features like ProcessLifecycleOwner, " +
                        "pls consider calling MultiProcessLifecycleInitializer#init manually " +
                        "or initializing matrix at Application#onCreate").let {
                    MatrixLog.e(TAG, it)
                }
                return
            }
            MatrixLifecycleThread.init(config.lifecycleThreadConfig)
            ProcessUILifecycleOwner.init(app)
            ForegroundServiceLifecycleOwner.init(app, config.enableFgServiceMonitor)
            OverlayWindowLifecycleOwner.init(config.enableOverlayWindowMonitor)
            MatrixLifecycleLogger.init(app, config.enableLifecycleLogger)
        }

        @SuppressLint("PrivateApi", "DiscouragedPrivateApi")
        @JvmStatic
        private fun hasCreatedActivities() = safeLet(tag = TAG, defVal = false) {
            val clazzActivityThread = Class.forName("android.app.ActivityThread")
            val objectActivityThread =
                clazzActivityThread.getMethod("currentActivityThread").invoke(null)
            val fieldMActivities = clazzActivityThread.getDeclaredField("mActivities")
            fieldMActivities.isAccessible = true
            val mActivities = fieldMActivities.get(objectActivityThread) as Map<*, *>?
            return mActivities != null && mActivities.isNotEmpty()
        }
    }
}