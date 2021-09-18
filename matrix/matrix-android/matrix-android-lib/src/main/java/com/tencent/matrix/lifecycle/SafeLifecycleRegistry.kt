package com.tencent.matrix.lifecycle

import android.annotation.SuppressLint
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry

/**
 * Created by Yves on 2021/9/18
 */
class SafeLifecycleRegistry(lifecycleOwner: LifecycleOwner) : Lifecycle() {

    @SuppressLint("VisibleForTests")
    private val delegate = LifecycleRegistry.createUnsafe(lifecycleOwner)

    @Synchronized
    override fun addObserver(observer: LifecycleObserver) = delegate.addObserver(observer)


    @Synchronized
    override fun removeObserver(observer: LifecycleObserver) = delegate.removeObserver(observer)


    @Synchronized
    override fun getCurrentState(): State = delegate.currentState


    @Synchronized
    fun setCurrentState(state: State) {
        delegate.currentState = state
    }

    @Synchronized
    @Deprecated("super deprecated", ReplaceWith("setCurrentState(state)"))
    fun markState(state: State) = delegate.markState(state)


    @Synchronized
    fun handleLifecycleEvent(event: Event) = delegate.handleLifecycleEvent(event)


    @Synchronized
    fun getObserverCount(): Int = delegate.observerCount

}