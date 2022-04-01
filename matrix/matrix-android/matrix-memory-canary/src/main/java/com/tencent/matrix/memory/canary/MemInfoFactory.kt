package com.tencent.matrix.memory.canary

import android.app.ActivityManager
import android.app.Application
import android.content.Context
import android.os.Build
import android.os.Debug
import android.os.Process
import android.text.TextUtils
import com.tencent.matrix.Matrix
import com.tencent.matrix.lifecycle.owners.ProcessUILifecycleOwner
import com.tencent.matrix.lifecycle.owners.ProcessUIStartedStateOwner
import com.tencent.matrix.lifecycle.supervisor.ProcessSupervisor
import com.tencent.matrix.util.MatrixLog
import com.tencent.matrix.util.MatrixUtil
import com.tencent.matrix.util.safeApply
import com.tencent.matrix.util.safeLet

import org.json.JSONObject
import java.io.File
import java.lang.StringBuilder
import java.util.*
import java.util.regex.Matcher
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
    val name: String = MatrixUtil.getProcessName(Matrix.with().application),
    val activity: String = ProcessUILifecycleOwner.recentScene.substringAfterLast('.'),
    val isProcessFg: Boolean = ProcessUIStartedStateOwner.active(),
    val isAppFg: Boolean = ProcessSupervisor.isAppUIForeground
) {
    override fun toString(): String {
        return String.format(
            "%-21s\t%-21s %-21s %-21s",
            name,
            "Activity=$activity",
            "AppForeground=$isAppFg",
            "ProcessForeground=$isProcessFg"
        )
    }

    fun toJson() = safeLet(tag = TAG, defVal = JSONObject()) {
        JSONObject().apply {
            put("pid", pid)
            put("name", name)
            put("activity", activity)
            put("isProcessFg", isProcessFg)
            put("isAppFg", isAppFg)
        }
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
        return String.format(
            "%-21s %-21s %-21s %-21s %-21s %-21s %-21s %-21s %-21s",
            "totalPss=$totalPssK K",
            "Java=$pssJavaK K",
            "Native=$pssNativeK K",
            "Graphic=$pssGraphicK K",
            "System=$pssSystemK K",
            "Swap=$pssSwapK K",
            "Code=$pssCodeK K",
            "Stack=$pssStackK K",
            "PrivateOther=$pssPrivateOtherK K"
        )
    }

    fun toJson() = safeLet(tag = TAG, defVal = JSONObject()) {
        JSONObject().apply {
            put("total", totalPssK)
            put("java", pssJavaK)
            put("native", pssNativeK)
            put("graphic", pssGraphicK)
            put("system", pssSystemK)
            put("swap", pssSwapK)
            put("code", pssCodeK)
            put("stack", pssStackK)
            put("other", pssPrivateOtherK)
        }
    }

    companion object {

        fun getFromDebug(): PssInfo {
            val dbgInfo = Debug.MemoryInfo()
            Debug.getMemoryInfo(dbgInfo)
            return get(dbgInfo)
        }

        fun getFromAms(): PssInfo {
            val mi =
                MemInfoFactory.activityManager
                    .getProcessMemoryInfo(arrayOf(Process.myPid()).toIntArray())
                    .firstOrNull()
            return if (mi != null) {
                get(mi)
            } else {
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
    val state: String = "default",
    val fdSize: Int = -1,
    val vmSizeK: Int = -1,
    val vmRssK: Int = -1,
    val vmSwapK: Int = -1,
    val threads: Int = -1,
    val oomAdj: Int = -1,
    val oomScoreAdj: Int = -1
) {
    override fun toString(): String {
        return String.format(
            "%-21s %-21s %-21s %-21s %-21s %-21s %-21s %-21s",
            "State=$state",
            "FDSize=$fdSize",
            "VmSize=$vmSizeK K",
            "VmRss=$vmRssK K",
            "VmSwap=$vmSwapK K",
            "Threads=$threads",
            "oom_adj=$oomAdj",
            "oom_score_adj=$oomScoreAdj"
        )
    }

    fun toJson() = safeLet(tag = TAG, defVal = JSONObject()) {
        JSONObject().apply {
            put("state", state)
            put("vmSize", vmSizeK)
            put("vmRss", vmRssK)
            put("vmSwap", vmSwapK)
            put("threads", threads)
            put("fdSize", fdSize)
            put("oom_adj", oomAdj)
            put("oom_score_adj", oomScoreAdj)
        }
    }

    companion object {
        @JvmStatic
        fun get(pid: Int = Process.myPid()): StatusInfo {
            return convertProcStatus(pid).run {
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

                StatusInfo(
                    state = getString("State").trimIndent(),
                    fdSize = getInt("FDSize"),
                    vmSizeK = getInt("VmSize"),
                    vmRssK = getInt("VmRSS"),
                    vmSwapK = getInt("VmSwap"),
                    threads = getInt("Threads"),
                    oomAdj = getOomAdj(pid),
                    oomScoreAdj = getOomScoreAdj(pid)
                )
            }
        }

        private fun getOomAdj(pid: Int): Int = safeLet(TAG, defVal = Int.MAX_VALUE) {
            File("/proc/$pid/oom_adj").useLines {
                it.first().toInt()
            }
        }

        private fun getOomScoreAdj(pid: Int): Int = safeLet(TAG, defVal = Int.MAX_VALUE) {
            File("/proc/$pid/oom_score_adj").useLines {
                it.first().toInt()
            }
        }

        private fun convertProcStatus(pid: Int): Map<String, String> {
            safeApply(TAG) {
                File("/proc/${pid}/status").useLines { seq ->
                    return seq.flatMap {
                        val split = it.split(":")
                        if (split.size == 2) {
                            return@flatMap sequenceOf(split[0] to split[1])
                        } else {
                            MatrixLog.e(TAG, "ERROR : $it")
                            return@flatMap emptySequence()
                        }
                    }.toMap()
                }
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
        return String.format(
            "%-21s %-21s %-21s %-21s %-21s %-21s",
            "Used=$usedByte B",
            "Recycled=$recycledByte B",
            "HeapSize=$heapSizeByte B",
            "Max=$maxByte B",
            "MemClass:$memClass M",
            "LargeMemClass=$largeMemClass M"
        )
    }

    fun toJson() = safeLet(tag = TAG, defVal = JSONObject()) {
        JSONObject().apply {
            put("used", usedByte)
            put("recycled", recycledByte)
            put("heapSize", heapSizeByte)
            put("max", maxByte)
            put("memClass", memClass)
            put("largeMemClass", largeMemClass)
        }
    }
}

data class NativeMemInfo(
    val heapSizeByte: Long = Debug.getNativeHeapSize(),
    val recycledByte: Long = Debug.getNativeHeapFreeSize(),
    val usedByte: Long = Debug.getNativeHeapAllocatedSize()
) {
    override fun toString(): String {
        return String.format(
            "%-21s %-21s %-21s",
            "Used=$usedByte B",
            "Recycled=$recycledByte B",
            "HeapSize=$heapSizeByte B"
        )
    }

    fun toJson() = safeLet(tag = TAG, defVal = JSONObject()) {
        JSONObject().apply {
            put("used", usedByte)
            put("recycled", recycledByte)
            put("heapSize", heapSizeByte)
        }
    }
}

data class SystemInfo(
    val totalMemByte: Long = -1,
    val availMemByte: Long = -1,
    val lowMemory: Boolean = false,
    val thresholdByte: Long = -1
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
        return String.format(
            "%-21s %-21s %-21s %-21s",
            "totalMem=$totalMemByte B",
            "availMem=$availMemByte B",
            "lowMemory=$lowMemory",
            "threshold=$thresholdByte B"
        )
    }

    fun toJson() = safeLet(tag = TAG, defVal = JSONObject()) {
        JSONObject().apply {
            put("totalMemByte", totalMemByte)
            put("availMemByte", availMemByte)
            put("lowMem", lowMemory)
            put("threshold", thresholdByte)
        }
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
                safeLet(TAG, true, defVal = emptyList()) {
                    MemInfoFactory.activityManager.getRunningServices(Int.MAX_VALUE)
                }
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
                |>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> MemInfo <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
                |> Process   : $processInfo
                |> Status    : $statusInfo
                |> SystemInfo: $systemInfo
                |> Java      : $javaMemInfo
                |> Native    : $nativeMemInfo
                |> Dbg-Pss   : $debugPssInfo
                |> AMS-Pss   : $amsPssInfo
                |> FgService : $fgServiceInfo
                |>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
            """.trimIndent() + "\n".run { if (cost <= 0) this else "$this| cost : $cost" }
    }

    fun toJson() = safeLet(tag = TAG, defVal = JSONObject()) {
        JSONObject().apply {
            processInfo?.let { put("processInfo", it.toJson()) }
            statusInfo?.let { put("statusInfo", it.toJson()) }
            javaMemInfo?.let { put("javaMemInfo", it.toJson()) }
            nativeMemInfo?.let { put("nativeMemInfo", it.toJson()) }
            systemInfo?.let { put("systemInfo", it.toJson()) }
            amsPssInfo?.let { put("amsPssInfo", it.toJson()) }
            debugPssInfo?.let { put("debugPssInfo", it.toJson()) }
        }
    }

    companion object {
        @JvmStatic
        fun getAllProcessPss(): Array<MemInfo> {
            val begin = System.currentTimeMillis()
            val memInfoArray = prepareAllProcessInfo()
            val pidMemInfoArray =
                MemInfoFactory.activityManager.getProcessMemoryInfo(memInfoArray.toPidArray())

            if (pidMemInfoArray != null) {
                for (i in memInfoArray.indices) {
                    if (pidMemInfoArray[i] == null) {
                        memInfoArray[i].amsPssInfo = PssInfo(0, 0, 0, 0, 0, 0, 0, 0, 0)
                        continue
                    }
                    memInfoArray[i].amsPssInfo = PssInfo.get(pidMemInfoArray[i])
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
                        statusInfo = null,
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

data class SmapsItem(
    var name: String? = null,
    var permission: String? = null,
    var count: Long = 0,
    var vmSize: Long = 0,
    var rss: Long = 0,
    var pss: Long = 0,
    var sharedClean: Long = 0,
    var sharedDirty: Long = 0,
    var privateClean: Long = 0,
    var privateDirty: Long = 0,
    var swapPss: Long = 0
)

data class MergedSmapsInfo(
    val list: List<SmapsItem>? = null
) {
    override fun toString(): String {
        val sb = StringBuilder()
        sb.append("\n")
        sb.append(
            String.format(
                FORMAT,
                "PSS",
                "RSS",
                "SIZE",
                "SWAP_PSS",
                "SH_C",
                "SH_D",
                "PRI_C",
                "PRI_D",
                "COUNT",
                "PERM",
                "NAME"
            )
        ).append("\n")
        sb.append(
            String.format(
                FORMAT,
                "----",
                "----",
                "----",
                "----",
                "----",
                "----",
                "----",
                "----",
                "----",
                "----",
                "----"
            )
        ).append("\n")
        for ((name, permission, count, vmSize, rss, pss, sharedClean, sharedDirty, privateClean, privateDirty, swapPss) in list!!) {
            sb.append(
                String.format(
                    FORMAT,
                    pss,
                    rss,
                    vmSize,
                    swapPss,
                    sharedClean,
                    sharedDirty,
                    privateClean,
                    privateDirty,
                    count,
                    permission,
                    name
                )
            ).append("\n")
        }
        sb.append(
            String.format(
                FORMAT,
                "----",
                "----",
                "----",
                "----",
                "----",
                "----",
                "----",
                "----",
                "----",
                "----",
                "----"
            )
        )
        sb.append("\n")

        return sb.toString()
    }

    companion object {

        // PSS RSS SIZE SWAP_PSS SH_C SH_D PRI_C PRI_D COUNT PERM NAME
        const val FORMAT = "%8s %8s %8s %8s %8s %8s %8s %8s %8s %8s     %s"

        @JvmStatic
        fun get(pid: Int = Process.myPid()): MergedSmapsInfo {
            return MergedSmapsInfo(mergeSmaps(pid))
        }

        private fun mergeSmaps(pid: Int): ArrayList<SmapsItem> {
            val pattern =
                Pattern.compile("^[0-9a-f]+-[0-9a-f]+\\s+([rwxps-]{4})\\s+\\d+\\s+\\d+:\\d+\\s+\\d+\\s+(.*)$")

            val merged: HashMap<String, SmapsItem> = HashMap<String, SmapsItem>()
            var currentInfo: SmapsItem? = null

            safeApply(TAG) {
                File("/proc/${pid}/smaps").reader().forEachLine { line ->
                    currentInfo?.let {
                        var found = true
                        when {
                            line.startsWith("Size:") -> {
                                val sizes = line.substring("Size:".length).trim { it <= ' ' }
                                    .split(" ").toTypedArray()
                                currentInfo!!.vmSize += sizes[0].toLong()
                            }
                            line.startsWith("Rss:") -> {
                                val sizes =
                                    line.substring("Rss:".length).trim { it <= ' ' }.split(" ")
                                        .toTypedArray()
                                currentInfo!!.rss += sizes[0].toLong()
                            }
                            line.startsWith("Pss:") -> {
                                val sizes =
                                    line.substring("Pss:".length).trim { it <= ' ' }.split(" ")
                                        .toTypedArray()
                                currentInfo!!.pss += sizes[0].toLong()
                            }
                            line.startsWith("Shared_Clean:") -> {
                                val sizes =
                                    line.substring("Shared_Clean:".length).trim { it <= ' ' }
                                        .split(" ").toTypedArray()
                                currentInfo!!.sharedClean += sizes[0].toLong()
                            }
                            line.startsWith("Shared_Dirty:") -> {
                                val sizes =
                                    line.substring("Shared_Dirty:".length).trim { it <= ' ' }
                                        .split(" ").toTypedArray()
                                currentInfo!!.sharedDirty += sizes[0].toLong()
                            }
                            line.startsWith("Private_Clean:") -> {
                                val sizes =
                                    line.substring("Private_Clean:".length).trim { it <= ' ' }
                                        .split(" ").toTypedArray()
                                currentInfo!!.privateClean += sizes[0].toLong()
                            }
                            line.startsWith("Private_Dirty:") -> {
                                val sizes =
                                    line.substring("Private_Dirty:".length).trim { it <= ' ' }
                                        .split(" ").toTypedArray()
                                currentInfo!!.privateDirty += sizes[0].toLong()
                            }
                            line.startsWith("SwapPss:") -> {
                                val sizes = line.substring("SwapPss:".length).trim { it <= ' ' }
                                    .split(" ").toTypedArray()
                                currentInfo!!.swapPss += sizes[0].toLong()
                            }
                            else -> {
                                found = false
                            }
                        }
                        if (found) return@forEachLine
                    }

                    val matcher: Matcher = pattern.matcher(line)
                    if (matcher.find()) {
                        val permission = matcher.group(1)
                        val name = matcher.group(2)
                        currentInfo = merged["$permission|$name"]
                        if (currentInfo == null) {
                            currentInfo = SmapsItem()
                            currentInfo!!.let {
                                merged["$permission|$name"] = it
                                it.permission = permission
                                it.name = name
                            }
                        }
                        currentInfo!!.count++
                    }
                }
            }

            val list: ArrayList<SmapsItem> = ArrayList(merged.values)

            list.sortWith { o1, o2 -> ((o2.pss - o1.pss).toInt()) }

            return list
        }
    }
}