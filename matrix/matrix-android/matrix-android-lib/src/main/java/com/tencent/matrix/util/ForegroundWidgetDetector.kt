package com.tencent.matrix.util

import android.annotation.SuppressLint
import android.app.ActivityManager
import android.content.Context
import android.os.Process
import android.view.View
import com.tencent.matrix.Matrix
import java.util.*
import kotlin.collections.ArrayList

/**
 * Created by Yves on 2021/11/23
 */
@SuppressLint("PrivateApi", "DiscouragedPrivateApi")
object ForegroundWidgetDetector {
    private const val TAG = "Matrix.ForegroundWidgetDetector"

    private val am: ActivityManager by lazy {
        Matrix.with().application.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
    }

    @JvmStatic
    fun hasForegroundService(): Boolean {
        return am.getRunningServices(Int.MAX_VALUE)
            .filter {
                it.uid == Process.myUid() && it.pid == Process.myPid()
            }.any {
                it.foreground
            }
    }

    private val WindowManagerGlobal_mRoots by lazy {
        safeLetOrNull(TAG) {
            Class.forName("android.view.WindowManagerGlobal").let {
                val instance = ReflectUtils.invoke<Any>(it, "getInstance", null)
                ReflectUtils.get<ArrayList<*>>(it, "mRoots", instance)
            }
        }
    }

    private val field_ViewRootImpl_mView by lazy {
        safeLetOrNull(TAG) {
            ReflectFiled<View>(Class.forName("android.view.ViewRootImpl"), "mView")
        }
    }

    @JvmStatic
    @SuppressLint("PrivateApi", "DiscouragedPrivateApi")
    fun hasFloatingView() = safeLet(TAG, log = true, defVal = false) {
        if (WindowManagerGlobal_mRoots == null) {
            MatrixLog.e(TAG, "WindowManagerGlobal_mRoots not found")
            return@safeLet false
        }
        if (field_ViewRootImpl_mView == null) {
            MatrixLog.e(TAG, "field_ViewRootImpl_mView not found")
            return@safeLet false
        }
        return@safeLet WindowManagerGlobal_mRoots!!.any {
            View.VISIBLE == field_ViewRootImpl_mView!!.get(it)?.visibility
        }
    }
}