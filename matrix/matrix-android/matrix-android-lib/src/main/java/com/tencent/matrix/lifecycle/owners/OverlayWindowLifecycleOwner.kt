package com.tencent.matrix.lifecycle.owners

import android.annotation.SuppressLint
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import com.tencent.matrix.lifecycle.StatefulOwner
import com.tencent.matrix.util.*

private const val SDK_GUARD = 32

/**
 * Created by Yves on 2021/12/30
 */
@SuppressLint("PrivateApi")
object OverlayWindowLifecycleOwner : StatefulOwner() {

    private const val TAG = "Matrix.OverlayWindowLifecycleOwner"

    private val overlayViews = HashSet<Any>()
    private val mainHandler = Handler(Looper.getMainLooper())

    private var WindowManagerGlobal_mRoots: ArrayList<*>? = null

    @Volatile
    private var injected = false

    private val Field_ViewRootImpl_mView by lazy {
        safeLetOrNull(TAG) {
            ReflectFiled<View>(Class.forName("android.view.ViewRootImpl"), "mView")
        }
    }

    internal fun init(enable: Boolean) {
        if (!enable) {
            MatrixLog.i(TAG, "disabled")
            return
        }
        if (Build.VERSION.SDK_INT > SDK_GUARD) { // for safety
            MatrixLog.e(TAG, "NOT support for api-level ${Build.VERSION.SDK_INT} yet!!!")
            return
        }
        inject()
    }

    private fun ViewGroup.LayoutParams.isOverlayType(): Boolean {
        return this is WindowManager.LayoutParams && if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            this.type == WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY || this.type == WindowManager.LayoutParams.TYPE_PHONE
        } else {
            this.type == WindowManager.LayoutParams.TYPE_PHONE
        }
    }

    private val onViewRootChangedListener: ArrayListProxy.OnDataChangedListener by lazy {
        object : ArrayListProxy.OnDataChangedListener {
            override fun onAdded(o: Any) {
                // we post to main thread cause the View is NOT set to ViewRootImpl yet
                mainHandler.post {
                    val view = safeLetOrNull(TAG) {
                        Field_ViewRootImpl_mView?.get(o) as View
                    }
                    if (view?.layoutParams?.isOverlayType() == true) {
                        if (overlayViews.isEmpty()) {
                            turnOn()
                        }
                        overlayViews.add(o)
                    }
                }
            }

            override fun onRemoved(o: Any) {
                overlayViews.remove(o)
                if (overlayViews.isEmpty()) {
                    turnOff()
                }
            }
        }
    }

    @Suppress("LocalVariableName")
    private fun inject() = safeApply(TAG) {
        if (injected) {
            MatrixLog.e(TAG, "already injected")
            return@safeApply
        }
        val Clazz_WindowManagerGlobal = Class.forName("android.view.WindowManagerGlobal")

        val WindowManagerGlobal_instance =
            ReflectUtils.invoke<Any>(Clazz_WindowManagerGlobal, "getInstance", null)

        val WindowManagerGlobal_mLock =
            ReflectUtils.get<Any>(Clazz_WindowManagerGlobal, "mLock", WindowManagerGlobal_instance)

        synchronized(WindowManagerGlobal_mLock) {
            val origin = ReflectUtils.get<ArrayList<*>>(
                Clazz_WindowManagerGlobal,
                "mRoots",
                WindowManagerGlobal_instance
            )
            val proxy = ArrayListProxy(origin, onViewRootChangedListener)
            ReflectUtils.set(
                Clazz_WindowManagerGlobal,
                "mRoots",
                WindowManagerGlobal_instance,
                proxy
            )
            WindowManagerGlobal_mRoots = proxy
        }
        injected = true
    }

    @Volatile
    private var fallbacked = false

    private fun prepareWindowGlobal() {
        if (WindowManagerGlobal_mRoots == null) {
            if (fallbacked) {
                throw ClassNotFoundException("WindowManagerGlobal_mRoots not found")
            }
            MatrixLog.i(TAG, "monitor disabled, fallback init")
            fallbacked = true
            WindowManagerGlobal_mRoots = safeLetOrNull(TAG) {
                Class.forName("android.view.WindowManagerGlobal").let {
                    val instance = ReflectUtils.invoke<Any>(it, "getInstance", null)
                    ReflectUtils.get(it, "mRoots", instance)
                }
            }
        }
        if (WindowManagerGlobal_mRoots == null) {
            throw ClassNotFoundException("WindowManagerGlobal_mRoots not found")
        }
        if (Field_ViewRootImpl_mView == null) {
            throw ClassNotFoundException("Field_ViewRootImpl_mView not found")
        }
    }

    /**
     * including Activity window, for fallback checks
     */
    fun hasVisibleWindow() = safeLet(TAG, log = true, defVal = false) {
        prepareWindowGlobal()
        return@safeLet WindowManagerGlobal_mRoots!!.any {
            val v = Field_ViewRootImpl_mView!!.get(it)
            // windowVisibility is determined by app vibility
            // until the PRIVATE_FLAG_FORCE_DECOR_VIEW_VISIBILITY is set, which is blocked in Q
            return@any v != null && View.VISIBLE == v.visibility && View.VISIBLE == v.windowVisibility
        }
    }

    fun hasOverlayWindow() = if (injected) {
        active()
    } else safeLet(TAG, log = true, defVal = false) {
        prepareWindowGlobal()
        return@safeLet WindowManagerGlobal_mRoots!!.any {
            val v = Field_ViewRootImpl_mView!!.get(it)
            // windowVisibility is determined by app vibility
            // until the PRIVATE_FLAG_FORCE_DECOR_VIEW_VISIBILITY is set, which is blocked in Q
            return@any v != null
                    && v.layoutParams?.isOverlayType() == true
                    && View.VISIBLE == v.visibility
                    && View.VISIBLE == v.windowVisibility
        }
    }

    /**
     * requires enable the monitor
     */
    internal fun getAllViews() = safeLet(TAG, log = true, defVal = emptyList<View>()) {
        prepareWindowGlobal()
        return@safeLet WindowManagerGlobal_mRoots
            ?.map { Field_ViewRootImpl_mView!!.get(it) }
            ?.toList()
            ?: emptyList()
    }
}