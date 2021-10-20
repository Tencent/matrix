package com.tencent.matrix.memory.canary.monitor

import android.app.ActivityManager
import android.content.Context
import android.os.Build
import android.os.Debug
import android.os.Process
import android.text.TextUtils
import androidx.annotation.WorkerThread
import com.tencent.matrix.Matrix
import com.tencent.matrix.util.MatrixUtil
import com.tencent.matrix.memory.canary.lifecycle.owners.ActivityRecorder
import com.tencent.matrix.memory.canary.lifecycle.owners.CombinedProcessForegroundStatefulOwner
import com.tencent.matrix.memory.canary.lifecycle.supervisor.ProcessSupervisor
import com.tencent.matrix.util.MatrixLog
import java.io.File
import java.lang.IllegalStateException
import java.util.*
import java.util.regex.Pattern
import kotlin.collections.ArrayList
import kotlin.collections.HashMap
import com.tencent.matrix.memory.canary.BuildConfig
import junit.framework.Assert

/**
 * Created by Yves on 2021/9/22
 */
// FIXME: 2021/10/20 代码整理
object MemoryInfoFactory {

    private const val TAG = "Matrix.MemoryInfoFactory"

    init {
        if (!Matrix.isInstalled()) {
            throw IllegalStateException("Matrix NOT installed yet!!!")
        }
    }

    private val activityManager =
        Matrix.with().application.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager

    private val memClass = activityManager.memoryClass
    private val memLargeClass = activityManager.largeMemoryClass

    // TODO: 2021/10/13
    @WorkerThread
    fun dumpMemory(withStats: Boolean = false, withAllProcess: Boolean = ProcessSupervisor.isSupervisor) : MemInfo {
        val memInfo = MemInfo()

        if (withStats) {

        }

        // TODO: 2021/10/9

        return memInfo
    }

    private fun dumpDebugPss(memInfo: MemInfo) {
        val mi = Debug.MemoryInfo()
        Debug.getMemoryInfo(mi)
        memInfo.amsPss = mi.totalPss
    }

    @get:WorkerThread
    @get:JvmStatic
    val VmSize: Long
        get() {
            return procStatusStartsWith("VmSize")
        }

    @get:WorkerThread
    @get:JvmStatic
    val VmRSS: Long
        get() {
            return procStatusStartsWith("VmRSS")
        }

    @get:WorkerThread
    @get:JvmStatic
    val RssAnon: Long
        get() {
            return procStatusStartsWith("RssAnon")
        }

    @get:WorkerThread
    @get:JvmStatic
    val RssFile: Long
        get() {
            return procStatusStartsWith("RssFile")
        }

    @get:WorkerThread
    @get:JvmStatic
    val RssShmem: Long
        get() {
            return procStatusStartsWith("RssShmem")
        }

    private fun procStatusStartsWith(status: String): Long {

        val begin = if (BuildConfig.DEBUG) {
            System.currentTimeMillis()
        } else {
            0L
        }

        var res = -1L

        try {
            File("/proc/${Process.myPid()}/status").useLines { seq ->
                seq.first { str ->
                    str.startsWith(status)
                }.run {
                    val pattern = Pattern.compile("\\d+")
                    val matcher = pattern.matcher(this)
                    while (matcher.find()) {
                        res = matcher.group().toLong()
                        return@useLines
                    }
                }
            }
        } catch (ignore: Throwable) {
        }

        return res.also {
            if (BuildConfig.DEBUG) MatrixLog.d(
                TAG,
                "$status: $it | cost: ${System.currentTimeMillis() - begin}"
            )
        }
    }

    @get:WorkerThread
    @get:JvmStatic
    val memoryStat: Map<String, String>
        get() {
            val mi = Debug.MemoryInfo()
            Debug.getMemoryInfo(mi)
            return mi.memoryStatToMap()
        }

    private fun Debug.MemoryInfo.memoryStatToMap(): Map<String, String> {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            this.memoryStats
        } else {
            mapOf(
                "summary.java-heap" to this.dalvikPrivateDirty.toString(),
                "summary.native-heap" to this.nativePrivateDirty.toString(),
                "summary.code" to "-1",
                "summary.stack" to "-1",
                "summary.graphics" to "-1",
                "summary.private-other" to "-1",
                "summary.system" to ((this.totalPss - this.totalPrivateClean - this.totalPrivateDirty)).toString(),
                "summary.total-pss" to this.totalPss.toString(),
                "summary.total-swap" to "-1"
            )
        }
    }

    @get:WorkerThread
    @get:JvmStatic
    val memoryStatFromAMS: Map<String, String>
        get() {
            val pidMemInfoArray: Array<Debug.MemoryInfo>? = activityManager.getProcessMemoryInfo(
                intArrayOf(
                    Process.myPid()
                )
            )

            return pidMemInfoArray
                ?.takeIf { it.size == 1 }
                ?.first()?.memoryStatToMap()
                ?: emptyMap()

        }

    private fun Array<MemInfo>.toPidArray(): IntArray {
        val pidArray = IntArray(size)
        forEachIndexed { i, info ->
            pidArray[i] = info.processInfo.pid
        }
        return pidArray
    }

    private fun prepareAllProcessInfo(): Array<MemInfo> {
        val processInfoList = activityManager.runningAppProcesses
        val memoryInfoList: MutableList<MemInfo> = ArrayList()

        for (i in processInfoList.indices) {
            val processInfo = processInfoList[i]
            val pkgName = Matrix.with().application.packageName
            if (Process.myUid() != processInfo.uid
                || TextUtils.isEmpty(processInfo.processName)
                || !processInfo.processName.startsWith(pkgName)
            ) {
                MatrixLog.e(
                    TAG,
                    "info with uid [%s] & process name [%s] is not current app [%s][%s]",
                    processInfoList[i].uid,
                    processInfoList[i].processName,
                    Process.myUid(),
                    pkgName
                )
                continue
            }

            memoryInfoList.add(
                MemInfo(
                    ProcessInfo(
                        processInfo.pid,
                        processInfo.processName,
                    )
                )
            )
        }
        return memoryInfoList.toTypedArray()
    }


    val allProcessMemInfo: Array<MemInfo>
        get() {
            val memInfoArray = prepareAllProcessInfo()
            val pidMemInfoArray = activityManager.getProcessMemoryInfo(memInfoArray.toPidArray())

            if (pidMemInfoArray != null) {
                if (BuildConfig.DEBUG) {
                    Assert.assertEquals(memInfoArray.size, pidMemInfoArray.size)
                }
                for (i in memInfoArray.indices) {
                    pidMemInfoArray[i].totalPss
                    memInfoArray[i].apply {
                        pidMemInfoArray[i].apply {
                            amsPss = totalPss
                            memoryStatsFromAms = memoryStatToMap()
                        }
                    }
                }
            }
            return memInfoArray
        }

    val sumPss: Int
        get() {
            return allProcessMemInfo.sumBy { it.amsPss }
        }
}

data class ProcessInfo(
    val pid: Int = Process.myPid(),
    val processName: String = MatrixUtil.getProcessName(Matrix.with().application),
    val activity: String = ActivityRecorder.currentActivity,
    val isProcessFg: Boolean = CombinedProcessForegroundStatefulOwner.active(),
    val isAppFg: Boolean = ProcessSupervisor.isAppForeground
) {
    override fun toString(): String {
        return "ProcessInfo: $processName    Activity: $activity    AppForeground: $isAppFg    ProcessForeground: $isProcessFg"
    }
}

fun Map<String, String>.getTotalPss() = get("summary.total-pss")

data class MemInfo(
    var processInfo: ProcessInfo = ProcessInfo(),
    var memoryStats: Map<String, String> = HashMap(),
    var memoryStatsFromAms: Map<String, String> = HashMap(),
    var vmSize: Int = -1,
    var amsPss: Int = -1,
    var debugPss: Int = -1
    // TODO
) {

    override fun toString(): String {
        // TODO: 2021/9/26
        return """
                >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> MemInfo <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
                | $processInfo
                | VmSize:
                | SystemMemoryInfo:
                | Java:
                | Native:
                | Stats:
                | AMSStats:
                | FgService:
            """.trimIndent()
    }
}