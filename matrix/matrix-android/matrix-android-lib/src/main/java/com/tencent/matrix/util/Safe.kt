package com.tencent.matrix.util

/**
 * Created by Yves on 2021/11/15
 */

const val DEFAULT_TAG = "Matrix.Safe"

inline fun <T> T.safeApply(
    tag: String = DEFAULT_TAG,
    log: Boolean = true,
    msg: String = "",
    unsafe: T.() -> Unit
): T {
    try {
        unsafe()
    } catch (e: Throwable) {
        if (log) {
            MatrixLog.printErrStackTrace(tag, e, msg)
        }
    }
    return this
}

inline fun <T, R> T.safeLet(
    unsafe: (T) -> R,
    success: (R) -> Unit = {},
    failed: (Throwable) -> Unit = {}
) {
    try {
        success(unsafe(this))
    } catch (e: Throwable) {
        failed(e)
    }
}

inline fun <T, R> T.safeLet(
    tag: String = DEFAULT_TAG,
    log: Boolean = true,
    msg: String = "",
    defVal: R,
    unsafe: (T) -> R
): R {
    return try {
        unsafe(this)
    } catch (e: Throwable) {
        if (log) {
            MatrixLog.printErrStackTrace(tag, e, msg)
        }
        defVal
    }
}

inline fun <T, R> T.safeLetOrNull(
    tag: String = DEFAULT_TAG,
    log: Boolean = true,
    msg: String = "",
    unsafe: (T) -> R
): R? {
    return try {
        unsafe(this)
    } catch (e: Throwable) {
        if (log) {
            MatrixLog.printErrStackTrace(tag, e, msg)
        }
        null
    }
}