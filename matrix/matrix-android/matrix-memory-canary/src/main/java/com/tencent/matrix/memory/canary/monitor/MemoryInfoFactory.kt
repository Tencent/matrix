package com.tencent.matrix.memory.canary.monitor

import android.app.ActivityManager
import android.content.Context
import android.os.Build
import android.os.Debug
import android.os.Process
import android.text.TextUtils
import com.tencent.matrix.Matrix
import com.tencent.matrix.memory.canary.BuildConfig
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
object MemoryInfoFactory {
    init {
        if (!Matrix.isInstalled()) {
            throw IllegalStateException("Matrix NOT installed yet!!!")
        }
    }

    internal val activityManager =
        Matrix.with().application.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager

    internal val memClass = activityManager.memoryClass
    internal val largeMemClass = activityManager.largeMemoryClass

    val allProcessMemInfo: Array<MemInfo>
        get() = MemInfo.getAllProcessPss()

    val allProcessPssSum: Int
        get() {
            return allProcessMemInfo.sumBy { it.amsPssInfo?.totalPss ?: 0 }
        }

    val currentMemInfo: MemInfo
        get() = MemInfo.getCurrentProcessMemInfo()
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
    var totalPss: Int = -1,
    var pssJava: Int = -1,
    var pssNative: Int = -1,
    var pssGraphic: Int = -1,
    var pssSystem: Int = -1,
    var pssSwap: Int = -1,
    var pssCode: Int = -1,
    var pssStack: Int = -1,
    var pssPrivateOther: Int = -1
) {
    override fun toString(): String {
        return "totalPss=$totalPss K,\tJava=$pssJava K,\tNative=$pssNative K,\tGraphic=$pssGraphic K,\tSystem=$pssSystem K,\tSwap=$pssSwap K,\tCode=$pssCode K,\tStack=$pssStack K,\tPrivateOther=$pssPrivateOther K"
    }

    companion object {

        fun getFromDebug(): PssInfo {
            val dbgInfo = Debug.MemoryInfo()
            Debug.getMemoryInfo(dbgInfo)
            return get(dbgInfo)
        }

        fun getFromAms(): PssInfo {
            val amsInfo =
                MemoryInfoFactory.activityManager.getProcessMemoryInfo(arrayOf(Process.myPid()).toIntArray())
            return amsInfo.firstOrNull()?.run {
                get(this)
            } ?: run {
                PssInfo()
            }
        }

        @JvmStatic
        fun get(memoryInfo: Debug.MemoryInfo): PssInfo {
            return PssInfo().also {
                it.totalPss = memoryInfo.totalPss

                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                    memoryInfo.memoryStats.apply {

                        fun Map<String, String>.getInt(key: String) = get(key)?.toInt() ?: -1

                        it.pssJava = getInt("summary.java-heap")
                        it.pssNative = getInt("summary.native-heap")
                        it.pssCode = getInt("summary.code")
                        it.pssStack = getInt("summary.stack")
                        it.pssGraphic = getInt("summary.graphics")
                        it.pssPrivateOther = getInt("summary.private-other")
                        it.pssSystem = getInt("summary.system")
                        it.pssSwap = getInt("summary.total-swap")
                    }
                } else {
                    memoryInfo.apply {
                        it.pssJava = dalvikPrivateDirty
                        it.pssNative = nativePrivateDirty
                        it.pssSystem = totalPss - totalPrivateClean - totalPrivateDirty
                    }
                }
            }
        }
    }
}

data class StatusInfo(
    var state: String = "default",
    var fdSize: Int = -1,
    var vmSize: Int = -1,
    var vmRss: Int = -1,
    var vmSwap: Int = -1,
    var threads: Int = -1
) {
    override fun toString(): String {
        return "State=$state,\tFDSize=$fdSize,\tVmSize=$vmSize K,\tVmRss=$vmRss K,\tVmSwap=$vmSwap K,\tThreads=$threads"
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
                    it.vmSize = getInt("VmSize")
                    it.vmRss = getInt("VmRSS")
                    it.vmSwap = getInt("VmSwap")
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
    val javaHeapUsedSize: Long = Runtime.getRuntime().totalMemory(),
    val javaHeapRecycledSize: Long = Runtime.getRuntime().freeMemory(),
    val javaHeapMaxSize: Long = Runtime.getRuntime().maxMemory(),
    val javaMemClass: Int = MemoryInfoFactory.memClass,
    val javaLargeMemClass: Int = MemoryInfoFactory.largeMemClass
) {
    override fun toString(): String {
        return "Used=$javaHeapUsedSize B,\tRecycled=$javaHeapRecycledSize B,\tMax=$javaHeapMaxSize B,\tMemClass:$javaMemClass M, LargeMemClass=$javaLargeMemClass M"
    }
}

data class NativeMemInfo(
    val nativeHeapSize: Long = Debug.getNativeHeapSize(),
    val nativeAllocatedSize: Long = Debug.getNativeHeapAllocatedSize(),
    val nativeRecycledSize: Long = Debug.getNativeHeapFreeSize()
) {
    override fun toString(): String {
        return "Used=$nativeAllocatedSize B,\tRecycled=$nativeRecycledSize B,\tHeapSize=$nativeHeapSize B"
    }
}

data class SystemInfo(
    var totalMem: Long = -1,
    var availMem: Long = -1,
    var lowMemory: Boolean = false,
    var threshold: Long = -1
) {
    override fun toString(): String {
        return "totalMem=$totalMem B,\tavailMem=$availMem B,\tlowMemory=$lowMemory B,\tthreshold=$threshold B"
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
                MemoryInfoFactory.activityManager.getRunningServices(
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
    var systemInfo: SystemInfo? = SystemInfo(),
    var amsPssInfo: PssInfo? = null,
    var debugPssInfo: PssInfo? = null,
    var fgServiceInfo: FgServiceInfo? = FgServiceInfo()
) {
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
            """.trimIndent() + "\n"
    }

    companion object {
        @JvmStatic
        fun getAllProcessPss(): Array<MemInfo> {
            val memInfoArray = prepareAllProcessInfo()
            val pidMemInfoArray =
                MemoryInfoFactory.activityManager.getProcessMemoryInfo(memInfoArray.toPidArray())

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
            return memInfoArray
        }

        @JvmStatic
        fun getCurrentProcessMemInfo(): MemInfo {
            return MemInfo()
        }

        @JvmStatic
        fun getCurrentProcessMemInfoWithPss(): MemInfo {
            return MemInfo(debugPssInfo = PssInfo.getFromDebug())
        }

        @JvmStatic
        fun getCurrentProcessMemInfoWithAmsPss(): MemInfo {
            return MemInfo(amsPssInfo = PssInfo.getFromAms())
        }

        @JvmStatic
        fun getCurrentProcessFullMemInfo(): MemInfo {
            return MemInfo(amsPssInfo = PssInfo.getFromAms(), debugPssInfo = PssInfo.getFromDebug())
        }

        private fun Array<MemInfo>.toPidArray(): IntArray {
            val pidArray = IntArray(size)
            forEachIndexed { i, info ->
                pidArray[i] = info.processInfo!!.pid // processInfo must be not null
            }
            return pidArray
        }

        private fun prepareAllProcessInfo(): Array<MemInfo> {
            val processInfoList = MemoryInfoFactory.activityManager.runningAppProcesses
            val memoryInfoList: MutableList<MemInfo> = ArrayList()

            if (processInfoList == null) {
                MatrixLog.e(TAG, "ERROR: activityManager.runningAppProcesses - no running process")
                return emptyArray()
            }

            MatrixLog.d(TAG, "processInfoList[$processInfoList]")

            val systemInfo = SystemInfo()
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