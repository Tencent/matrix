package com.tencent.matrix.resource.dumper

import android.os.Process
import com.tencent.matrix.Matrix
import com.tencent.matrix.util.MatrixLog
import com.tencent.matrix.util.MatrixUtil
import com.tencent.matrix.util.safeApply
import java.io.File
import java.io.FileNotFoundException
import java.text.SimpleDateFormat
import java.util.*
import java.util.concurrent.TimeUnit

/**
 * manage hprof files with LRU
 */
object HprofFileManager {

    private const val TAG = "Matrix.HprofFileManager"

    private const val MAX_FILE_COUNT = 10
    private const val CLEAN_THRESHOLD = 12 * 1024 * 1024 * 1024L // 12G
    private const val MIN_FREE_SPACE = 1024 * 1024 * 1024L // 1G

    private val EXPIRED_TIME_MILLIS = TimeUnit.DAYS.toMillis(7)

    private val hprofStorageDir by lazy { File(Matrix.with().application.cacheDir.absolutePath + "/hprofs") }

    private val format by lazy { SimpleDateFormat("yyyy-MM-dd-HH-mm-ss", Locale.US) }

    fun checkExpiredFile() {
        safeApply(TAG) {
            val current = System.currentTimeMillis()
            if (hprofStorageDir.exists() && hprofStorageDir.isDirectory) {
                hprofStorageDir.listFiles { it: File ->
                    current - it.lastModified() > EXPIRED_TIME_MILLIS
                }?.forEach {
                    if (it.exists()) {
                        MatrixLog.i(TAG, "expired: ${it.absolutePath}")
                        it.delete()
                    }
                }
            }
        }
    }

    @Throws(FileNotFoundException::class)
    fun prepareHprofFile(prefix: String = "", deleteSoon: Boolean = false): File {
        hprofStorageDir.prepare(deleteSoon)
        return File(hprofStorageDir, getHprofFileName(prefix))
    }

    fun clearAll() {
        hprofStorageDir.deleteRecursively()
    }

    private fun File.prepare(deleteSoon: Boolean) {
        reserve()
        makeSureEnoughSpace(deleteSoon)
    }

    private fun File.reserve() {
        if (!exists() && (!mkdirs() || !canWrite())) {
            "fialed to create new hprof file since path: $absolutePath is not writable".let {
                MatrixLog.e(TAG, it)
                throw FileNotFoundException(it)
            }
        }
    }

    private fun File.makeSureEnoughSpace(deleteSoon: Boolean) {
        if (!isDirectory) {
            return
        }
        lru()
        if (freeSpace < CLEAN_THRESHOLD) {
            listFiles()?.forEach { it.delete() }
        }
        if (freeSpace < if (deleteSoon) MIN_FREE_SPACE else CLEAN_THRESHOLD) {
            throw FileNotFoundException("free space($freeSpace) less than $CLEAN_THRESHOLD, skip dump hprof")
        }
    }

    private fun File.lru() {
        if (!isDirectory) {
            return
        }
        val files = listFiles() ?: return
        files.sortBy { it.lastModified() }
        files.forEach {
            MatrixLog.d(TAG, "==> list sorted: ${it.absolutePath}, last mod = ${it.lastModified()}")
        }
        if (files.size >= MAX_FILE_COUNT) {
            files.take(files.size - MAX_FILE_COUNT + 1).forEach {
                it.delete()
            }
        }
    }

    private val processSuffix by lazy {
        if (MatrixUtil.isInMainProcess(Matrix.with().application)) {
            "Main"
        } else {
            val split =
                MatrixUtil.getProcessName(Matrix.with().application).split(":").toTypedArray()
            if (split.size > 1) {
                split[1]
            } else {
                "unknown"
            }
        }
    }

    private fun getHprofFileName(prefix: String) =
        "$prefix-${processSuffix}-${Process.myPid()}-${format.format(Calendar.getInstance().time)}.hprof"
}