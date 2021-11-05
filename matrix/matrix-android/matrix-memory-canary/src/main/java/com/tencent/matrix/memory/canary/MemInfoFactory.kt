package com.tencent.matrix.memory.canary

import android.app.ActivityManager
import android.app.Application
import android.content.Context
import android.os.Build
import android.os.Debug
import android.os.Process
import android.text.TextUtils
import com.tencent.matrix.Matrix
import com.tencent.matrix.lifecycle.owners.ActivityRecorder
import com.tencent.matrix.lifecycle.owners.CombinedProcessForegroundOwner
import com.tencent.matrix.lifecycle.supervisor.ProcessSupervisor
import com.tencent.matrix.util.MatrixLog
import com.tencent.matrix.util.MatrixUtil
import junit.framework.Assert
import java.io.File
import java.util.*
import java.util.regex.Pattern
import kotlin.collections.ArrayList

private const val TAG = "Matrix.MemoryInfoFactory"

/**
 * Created by Yves on 2021/9/22
 */
object MemInfoFactory {
    @Volatile
    @JvmStatic
    private var manualInitialized = false

    @Volatile
    @JvmStatic
    private var application: Application? = null

    @JvmStatic
    fun init(app: Application) {
        application = app
        manualInitialized = true
    }

    init {
        if (!manualInitialized && !Matrix.isInstalled()) {
            throw IllegalStateException("Matrix is NOT installed or MemoryInfoFactory is not initialized!!!")
        }
    }

    val activityManager = (if (manualInitialized) application else Matrix.with().application)
        ?.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager

    val memClass = activityManager.memoryClass
    val largeMemClass = activityManager.largeMemoryClass
}

data class ProcessInfo(
    val pid: Int = Process.myPid(),
    val processName: String = MatrixUtil.getProcessName(Matrix.with().application),
    val activity: String = ActivityRecorder.currentActivity,
    val isProcessFg: Boolean = CombinedProcessForegroundOwner.active(),
    val isAppFg: Boolean = ProcessSupervisor.isAppForeground
) {
    override fun toString(): String {
        return "Name=$processName, Activity=$activity, AppForeground=$isAppFg, ProcessForeground=$isProcessFg"
    }
}

data class PssInfo(
    var totalPssK: Int = -1,
    var pssJavaK: Int = -1,
    var pssNativeK: Int = -1,
    var pssGraphicK: Int = -1,
    var pssSystemK: Int = -1,
    var pssSwapK: Int = -1,
    var pssCodeK: Int = -1,
    var pssStackK: Int = -1,
    var pssPrivateOtherK: Int = -1
) {
    override fun toString(): String {
        return "totalPss=$totalPssK K,\tJava=$pssJavaK K,\tNative=$pssNativeK K,\tGraphic=$pssGraphicK K,\tSystem=$pssSystemK K,\tSwap=$pssSwapK K,\tCode=$pssCodeK K,\tStack=$pssStackK K,\tPrivateOther=$pssPrivateOtherK K"
    }

    companion object {

        fun getFromDebug(): PssInfo {
            val dbgInfo = Debug.MemoryInfo()
            Debug.getMemoryInfo(dbgInfo)
            return get(dbgInfo)
        }

        fun getFromAms(): PssInfo {
            val amsInfo =
                MemInfoFactory.activityManager.getProcessMemoryInfo(arrayOf(Process.myPid()).toIntArray())
            return amsInfo.firstOrNull()?.run {
                get(this)
            } ?: run {
                PssInfo()
            }
        }

        @JvmStatic
        fun get(memoryInfo: Debug.MemoryInfo): PssInfo {
            return PssInfo().also {
                it.totalPssK = memoryInfo.totalPss

                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                    memoryInfo.memoryStats.apply {

                        fun Map<String, String>.getInt(key: String) = get(key)?.toInt() ?: -1

                        it.pssJavaK = getInt("summary.java-heap")
                        it.pssNativeK = getInt("summary.native-heap")
                        it.pssCodeK = getInt("summary.code")
                        it.pssStackK = getInt("summary.stack")
                        it.pssGraphicK = getInt("summary.graphics")
                        it.pssPrivateOtherK = getInt("summary.private-other")
                        it.pssSystemK = getInt("summary.system")
                        it.pssSwapK = getInt("summary.total-swap")
                    }
                } else {
                    memoryInfo.apply {
                        it.pssJavaK = dalvikPrivateDirty
                        it.pssNativeK = nativePrivateDirty
                        it.pssSystemK = totalPss - totalPrivateClean - totalPrivateDirty
                    }
                }
            }
        }
    }
}

data class StatusInfo(
    var state: String = "default",
    var fdSize: Int = -1,
    var vmSizeK: Int = -1,
    var vmRssK: Int = -1,
    var vmSwapK: Int = -1,
    var threads: Int = -1
) {
    override fun toString(): String {
        return "State=$state,\tFDSize=$fdSize,\tVmSize=$vmSizeK K,\tVmRss=$vmRssK K,\tVmSwap=$vmSwapK K,\tThreads=$threads"
    }

    companion object {
        @JvmStatic
        fun get(pid: Int = Process.myPid()): StatusInfo {
            return StatusInfo().also {
                convertProcStatus(pid).apply {
                    fun Map<String, String>.getString(key: String) = get(key) ?: "unknown"

                    fun Map<String, String>.getInt(key: String): Int {
                        getString(key).let {
                            val matcher = Pattern.compile("\\d+").matcher(it)
                            while (matcher.find()) {
                                return matcher.group().toInt()
                            }
                        }
                        return -2
                    }

                    it.state = getString("State").trimIndent()
                    it.fdSize = getInt("FDSize")
                    it.vmSizeK = getInt("VmSize")
                    it.vmRssK = getInt("VmRSS")
                    it.vmSwapK = getInt("VmSwap")
                    it.threads = getInt("Threads")
                }
            }
        }

        private fun convertProcStatus(pid: Int): Map<String, String> {
            val begin = if (BuildConfig.DEBUG) {
                System.currentTimeMillis()
            } else {
                0L
            }

            try {
                File("/proc/${pid}/status").useLines { seq ->
                    return seq.flatMap {
                        val split = it.split(":")
                        if (split.size == 2) {
                            return@flatMap sequenceOf(split[0] to split[1])
                        } else {
                            MatrixLog.e(TAG, "ERROR : $it")
                            return@flatMap emptySequence()
                        }
                    }.toMap().also {
                        if (BuildConfig.DEBUG) {
                            MatrixLog.d(
                                TAG,
                                "convertProcStatus cost ${System.currentTimeMillis() - begin}"
                            )
                        }
                    }
                }
            } catch (ignore: Throwable) {
            }

            return emptyMap()
        }
    }
}

data class JavaMemInfo(
    val heapSizeByte: Long = Runtime.getRuntime().totalMemory(),
    val recycledByte: Long = Runtime.getRuntime().freeMemory(),
    val usedByte: Long = heapSizeByte - recycledByte,
    val maxByte: Long = Runtime.getRuntime().maxMemory(),
    val memClass: Int = MemInfoFactory.memClass,
    val largeMemClass: Int = MemInfoFactory.largeMemClass
) {
    override fun toString(): String {
        return "Used=$usedByte B,\tRecycled=$recycledByte B,\tHeapSize=$heapSizeByte B,\tMax=$maxByte B,\tMemClass:$memClass M, LargeMemClass=$largeMemClass M"
    }
}

data class NativeMemInfo(
    val heapSizeByte: Long = Debug.getNativeHeapSize(),
    val recycledByte: Long = Debug.getNativeHeapFreeSize(),
    val usedByte: Long = Debug.getNativeHeapAllocatedSize()
) {
    override fun toString(): String {
        return "Used=$usedByte B,\tRecycled=$recycledByte B,\tHeapSize=$heapSizeByte B"
    }
}

data class SystemInfo(
    var totalMemByte: Long = -1,
    var availMemByte: Long = -1,
    var lowMemory: Boolean = false,
    var thresholdByte: Long = -1
) {
    companion object {
        fun get(): SystemInfo {
            val info = ActivityManager.MemoryInfo()
            MemInfoFactory.activityManager.getMemoryInfo(info)
            return SystemInfo(
                totalMemByte = info.totalMem,
                availMemByte = info.availMem,
                lowMemory = info.lowMemory,
                thresholdByte = info.threshold
            )
        }
    }

    override fun toString(): String {
        return "totalMem=$totalMemByte B,\tavailMem=$availMemByte B,\tlowMemory=$lowMemory,\tthreshold=$thresholdByte B"
    }
}

data class FgServiceInfo(val fgServices: List<String> = getRunningForegroundServices()) {
    override fun toString(): String {
        return fgServices.toTypedArray().contentToString()
    }

    companion object {
        private fun getRunningForegroundServices(): List<String> {
            val fgServices = ArrayList<String>()
            val runningServiceInfoList: List<ActivityManager.RunningServiceInfo> =
                MemInfoFactory.activityManager.getRunningServices(
                    Int.MAX_VALUE
                )
            for (serviceInfo in runningServiceInfoList) {
                if (serviceInfo.uid != Process.myUid()) {
                    continue
                }
                if (serviceInfo.pid != Process.myPid()) {
                    continue
                }
                if (serviceInfo.foreground) {
                    fgServices.add(serviceInfo.service.className)
                }
            }
            return fgServices
        }
    }
}

data class MemInfo(
    var processInfo: ProcessInfo? = ProcessInfo(),
    var statusInfo: StatusInfo? = StatusInfo.get(),
    var javaMemInfo: JavaMemInfo? = JavaMemInfo(),
    var nativeMemInfo: NativeMemInfo? = NativeMemInfo(),
    var systemInfo: SystemInfo? = SystemInfo.get(),
    var amsPssInfo: PssInfo? = null,
    var debugPssInfo: PssInfo? = null,
    var fgServiceInfo: FgServiceInfo? = FgServiceInfo()
) {
    var cost = 0L
    override fun toString(): String {
        return "\n" + """
                >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> MemInfo <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
                | Process   : $processInfo
                | Status    : $statusInfo
                | SystemInfo: $systemInfo
                | Java      : $javaMemInfo
                | Native    : $nativeMemInfo
                | Dbg-Pss   : $debugPssInfo
                | AMS-Pss   : $amsPssInfo
                | FgService : $fgServiceInfo
                >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
            """.trimIndent() + "\n".run { if (cost <= 0) this else "$this| cost : $cost" }
    }

    companion object {
        @JvmStatic
        fun getAllProcessPss(): Array<MemInfo> {
            val begin = System.currentTimeMillis()
            val memInfoArray = prepareAllProcessInfo()
            val pidMemInfoArray =
                MemInfoFactory.activityManager.getProcessMemoryInfo(memInfoArray.toPidArray())

            if (pidMemInfoArray != null) {
                if (BuildConfig.DEBUG) {
                    Assert.assertEquals(memInfoArray.size, pidMemInfoArray.size)
                }
                for (i in memInfoArray.indices) {
                    pidMemInfoArray[i].totalPss
                    memInfoArray[i].apply {
                        pidMemInfoArray[i].apply {
                            amsPssInfo = PssInfo.get(this)
                        }
                    }
                }
            }
            MatrixLog.i(TAG, "getAllProcessPss cost: ${System.currentTimeMillis() - begin}")
            return memInfoArray
        }

        @JvmStatic
        fun getCurrentProcessMemInfo(): MemInfo {
            val begin = System.currentTimeMillis()
            return MemInfo().also { it.cost = System.currentTimeMillis() - begin }
        }

        @JvmStatic
        fun getCurrentProcessMemInfoWithPss(): MemInfo {
            val begin = System.currentTimeMillis()
            return MemInfo(debugPssInfo = PssInfo.getFromDebug()).also {
                it.cost = System.currentTimeMillis() - begin
            }
        }

        @JvmStatic
        fun getCurrentProcessMemInfoWithAmsPss(): MemInfo {
            val begin = System.currentTimeMillis()
            return MemInfo(amsPssInfo = PssInfo.getFromAms()).also {
                it.cost = System.currentTimeMillis() - begin
            }
        }

        @JvmStatic
        fun getCurrentProcessFullMemInfo(): MemInfo {
            val begin = System.currentTimeMillis()
            return MemInfo(
                amsPssInfo = PssInfo.getFromAms(),
                debugPssInfo = PssInfo.getFromDebug()
            ).also { it.cost = System.currentTimeMillis() - begin }
        }

        private fun Array<MemInfo>.toPidArray(): IntArray {
            val pidArray = IntArray(size)
            forEachIndexed { i, info ->
                pidArray[i] = info.processInfo!!.pid // processInfo must be not null
            }
            return pidArray
        }

        private fun prepareAllProcessInfo(): Array<MemInfo> {
            val processInfoList = MemInfoFactory.activityManager.runningAppProcesses
            val memoryInfoList: MutableList<MemInfo> = ArrayList()

            if (processInfoList == null) {
                MatrixLog.e(TAG, "ERROR: activityManager.runningAppProcesses - no running process")
                return emptyArray()
            }

            MatrixLog.d(TAG, "processInfoList[$processInfoList]")

            val systemInfo = SystemInfo.get()
            for (i in processInfoList.indices) {
                val processInfo = processInfoList[i]
                val pkgName = Matrix.with().application.packageName
                if (Process.myUid() != processInfo.uid
                    || TextUtils.isEmpty(processInfo.processName)
                    || !processInfo.processName.startsWith(pkgName)
                ) {
                    MatrixLog.e(
                        TAG,
                        "info with uid [${processInfo.uid}] & process name [${processInfo.processName}] is not current app [${Process.myUid()}][${pkgName}]",
                    )
                    continue
                }

                memoryInfoList.add(
                    MemInfo(
                        processInfo = ProcessInfo(
                            processInfo.pid,
                            processInfo.processName,
                        ),
                        statusInfo = StatusInfo.get(processInfo.pid),
                        javaMemInfo = null,
                        nativeMemInfo = null,
                        systemInfo = systemInfo,
                    )
                )
            }
            return memoryInfoList.toTypedArray()
        }
    }
}