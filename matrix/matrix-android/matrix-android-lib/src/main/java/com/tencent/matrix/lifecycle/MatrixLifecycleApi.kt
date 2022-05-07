package com.tencent.matrix.lifecycle

import androidx.lifecycle.LifecycleOwner

@Deprecated("")
abstract class IMatrixLifecycleCallback {
    internal var stateObserver: IStateObserver? = null
    abstract fun onForeground()
    abstract fun onBackground()
}

abstract class IMatrixForegroundCallback {
    internal var stateObserver: IStateObserver? = null
    abstract fun onEnterForeground()

    /**
     * NOTICE: do NOT always mean background!!!, depending on the observed owner
     */
    abstract fun onExitForeground()
}

abstract class IMatrixBackgroundCallback {
    internal var stateObserver: IStateObserver? = null
    abstract fun onEnterBackground()

    /**
     * NOTICE: do NOT always mean foreground!!!, depending on the observed owner
     */
    abstract fun onExitBackground()
}

interface IBackgroundStatefulOwner : IStatefulOwner {
    fun isBackground() = active()

    fun addLifecycleCallback(callback: IMatrixBackgroundCallback) =
        observeForever(object : IStateObserver {
            override fun on() = callback.onEnterBackground()
            override fun off() = callback.onExitBackground()
        }.also { callback.stateObserver = it })

    fun addLifecycleCallback(lifecycleOwner: LifecycleOwner, callback: IMatrixBackgroundCallback) =
        observeWithLifecycle(lifecycleOwner, object : IStateObserver {
            override fun on() = callback.onEnterBackground()
            override fun off() = callback.onExitBackground()
        }.also { callback.stateObserver = it })

    fun removeLifecycleCallback(callback: IMatrixBackgroundCallback) {
        callback.stateObserver?.let { removeObserver(it) }
    }

    @Deprecated("")
    fun addLifecycleCallback(callback: IMatrixLifecycleCallback) =
        observeForever(object : IStateObserver {
            override fun on() = callback.onBackground()
            override fun off() = callback.onForeground()
        }.also { callback.stateObserver = it })

    @Deprecated("")
    fun addLifecycleCallback(lifecycleOwner: LifecycleOwner, callback: IMatrixLifecycleCallback) =
        observeWithLifecycle(lifecycleOwner, object : IStateObserver {
            override fun on() = callback.onBackground()
            override fun off() = callback.onForeground()
        }.also { callback.stateObserver = it })

    @Deprecated("")
    fun removeLifecycleCallback(callback: IMatrixLifecycleCallback) {
        callback.stateObserver?.let { removeObserver(it) }
    }
}

interface IForegroundStatefulOwner : IStatefulOwner {
    fun isForeground() = active()

    fun addLifecycleCallback(callback: IMatrixForegroundCallback) =
        observeForever(object : IStateObserver {
            override fun on() = callback.onEnterForeground()
            override fun off() = callback.onExitForeground()
        }.also { callback.stateObserver = it })

    fun addLifecycleCallback(lifecycleOwner: LifecycleOwner, callback: IMatrixForegroundCallback) =
        observeWithLifecycle(lifecycleOwner, object : IStateObserver {
            override fun on() = callback.onEnterForeground()
            override fun off() = callback.onExitForeground()
        }.also { callback.stateObserver = it })

    fun removeLifecycleCallback(callback: IMatrixForegroundCallback) {
        callback.stateObserver?.let { removeObserver(it) }
    }

    @Deprecated("")
    fun addLifecycleCallback(callback: IMatrixLifecycleCallback) =
        observeForever(object : IStateObserver {
            override fun on() = callback.onForeground()
            override fun off() = callback.onBackground()
        }.also { callback.stateObserver = it })

    @Deprecated("")
    fun addLifecycleCallback(lifecycleOwner: LifecycleOwner, callback: IMatrixLifecycleCallback) =
        observeWithLifecycle(lifecycleOwner, object : IStateObserver {
            override fun on() = callback.onForeground()
            override fun off() = callback.onBackground()
        }.also { callback.stateObserver = it })

    @Deprecated("")
    fun removeLifecycleCallback(callback: IMatrixLifecycleCallback) {
        callback.stateObserver?.let { removeObserver(it) }
    }
}