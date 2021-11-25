package com.tencent.matrix.util

import android.annotation.SuppressLint
import android.app.ActivityManager
import android.content.Context
import android.os.Process
import com.tencent.matrix.Matrix
import java.util.*

/**
 * Created by Yves on 2021/11/23
 */
object ForegroundWidgetDetector {
    private const val TAG = "Matrix.ForegroundWidgetDetector"

    @JvmStatic
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

    @JvmStatic
    @SuppressLint("PrivateApi", "DiscouragedPrivateApi")
    fun hasFloatingView() = safeLet(TAG, log = true, defVal = false) {
        val clazzWindowManagerGlobal = Class.forName("android.view.WindowManagerGlobal")
        val methodGetInstance = clazzWindowManagerGlobal.getDeclaredMethod("getInstance")
            .apply { isAccessible = true }
        val objGlobal = methodGetInstance.invoke(null)
        val fieldRoots = clazzWindowManagerGlobal.getDeclaredField("mRoots")
            .apply { isAccessible = true }
        val mRoots = fieldRoots.get(objGlobal) as ArrayList<*>
        val clazzViewRootImpl = Class.forName("android.view.ViewRootImpl")
        val fieldView = clazzViewRootImpl.getDeclaredField("mView")
        return@safeLet mRoots.filter { fieldView.get(it) != null }.size > 1
    }
}