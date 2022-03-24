package com.tencent.matrix.lifecycle

import androidx.lifecycle.LifecycleOwner

abstract class IMatrixLifecycleCallback {
    internal var stateObserver: IStateObserver? = null
    abstract fun onForeground()
    abstract fun onBackground()
}

interface IBackgroundStatefulOwner : IStatefulOwner {
    fun isBackground() = active()

    fun addLifecycleCallback(callback: IMatrixLifecycleCallback) =
        observeForever(object : IStateObserver {
            override fun on() = callback.onBackground()
            override fun off() = callback.onForeground()
        }.also { callback.stateObserver = it })

    fun addLifecycleCallback(lifecycleOwner: LifecycleOwner, callback: IMatrixLifecycleCallback) =
        observeWithLifecycle(lifecycleOwner, object : IStateObserver {
            override fun on() = callback.onBackground()
            override fun off() = callback.onForeground()
        }.also { callback.stateObserver = it })

    fun removeLifecycleCallback(callback: IMatrixLifecycleCallback) {
        callback.stateObserver?.let { removeObserver(it) }
    }
}

interface IForegroundStatefulOwner : IStatefulOwner {
    fun isForeground() = active()

    fun addLifecycleCallback(callback: IMatrixLifecycleCallback) =
        observeForever(object : IStateObserver {
            override fun on() = callback.onForeground()
            override fun off() = callback.onBackground()
        }.also { callback.stateObserver = it })

    fun addLifecycleCallback(lifecycleOwner: LifecycleOwner, callback: IMatrixLifecycleCallback) =
        observeWithLifecycle(lifecycleOwner, object : IStateObserver {
            override fun on() = callback.onForeground()
            override fun off() = callback.onBackground()
        }.also { callback.stateObserver = it })

    fun removeLifecycleCallback(callback: IMatrixLifecycleCallback) {
        callback.stateObserver?.let { removeObserver(it) }
    }
}