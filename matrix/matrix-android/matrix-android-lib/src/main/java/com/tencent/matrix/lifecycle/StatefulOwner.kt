package com.tencent.matrix.lifecycle

import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.OnLifecycleEvent
import com.tencent.matrix.lifecycle.LifecycleDelegateStatefulOwner.Companion.toStateOwner
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.ConcurrentLinkedQueue

interface IStateful {
    fun active(): Boolean
}

fun IStatefulOwner.reverse(): IStatefulOwner = object : IStatefulOwner {
    override fun active() = !this@reverse.active()

    inner class ReverseObserverWrapper(val origin: IStateObserver) : IStateObserver,
        ISerialObserver {
        override fun on() = origin.off()
        override fun off() = origin.on()
        override fun toString() = origin.toString()
        override fun hashCode() = origin.hashCode()
        override fun equals(other: Any?): Boolean {
            return if (other is ReverseObserverWrapper) {
                origin == other.origin
            } else {
                false
            }
        }

        override fun serial(): Boolean {
            return if (origin is ISerialObserver) {
                origin.serial()
            } else {
                false
            }
        }
    }

    private fun IStateObserver.wrap(): ReverseObserverWrapper = ReverseObserverWrapper(this)

    override fun observeForever(observer: IStateObserver) =
        this@reverse.observeForever(observer.wrap())

    override fun observeWithLifecycle(lifecycleOwner: LifecycleOwner, observer: IStateObserver) =
        this@reverse.observeWithLifecycle(lifecycleOwner, observer.wrap())

    override fun removeObserver(observer: IStateObserver) =
        this@reverse.removeObserver(observer.wrap())
}

fun IStatefulOwner.shadow() = shadow(false)

internal fun IStatefulOwner.shadow(serial: Boolean): IStatefulOwner = object : StatefulOwner(serial) {
    init {
        this@shadow.observeForever(object : IStateObserver, ISerialObserver {
            override fun on() = turnOn()
            override fun off() = turnOff()
            override fun serial() = serial
        })
    }
}

interface IStateObserver {
    fun on()
    fun off()
}

internal interface ISerialObserver : IStateObserver {
    fun serial() = true
}

interface IStateObservable {
    fun observeForever(observer: IStateObserver)
    fun observeWithLifecycle(lifecycleOwner: LifecycleOwner, observer: IStateObserver)
    fun removeObserver(observer: IStateObserver)
}

interface IStatefulOwner : IStateful, IStateObservable

interface IMultiSourceOwner {
    fun addSourceOwner(owner: StatefulOwner)
    fun removeSourceOwner(owner: StatefulOwner)
}

private open class ObserverWrapper(val observer: IStateObserver, val statefulOwner: StatefulOwner) {
    open fun isAttachedTo(owner: LifecycleOwner?) = false
}

private class AutoReleaseObserverWrapper constructor(
    val lifecycleOwner: LifecycleOwner,
    targetObservable: StatefulOwner,
    observer: IStateObserver
) : ObserverWrapper(observer, targetObservable), LifecycleObserver {

    init {
        lifecycleOwner.lifecycle.addObserver(this)
    }

    override fun isAttachedTo(owner: LifecycleOwner?) = lifecycleOwner == owner

    @OnLifecycleEvent(Lifecycle.Event.ON_DESTROY)
    fun release() {
        statefulOwner.removeObserver(observer)
    }
}

private fun ObserverWrapper.checkLifecycle(lifecycleOwner: LifecycleOwner?): Boolean {
    val broken = if (lifecycleOwner != null) {
        isAttachedTo(lifecycleOwner)
    } else {
        this is AutoReleaseObserverWrapper
    }

    if (broken) {
        throw IllegalArgumentException(
            "Cannot add the same observer with different lifecycles"
        )
    }

    return false
}

private enum class State(val dispatch: ((observer: IStateObserver) -> Unit)?) {
    INIT(null),
    ON({ observer -> observer.on() }),
    OFF({ observer -> observer.off() });
}

open class StatefulOwner(val async: Boolean = true) : IStatefulOwner {

    @Volatile
    private var state = State.INIT

    private val observerMap = ConcurrentHashMap<IStateObserver, ObserverWrapper>()

    override fun active() = state == State.ON

    /**
     * Observe the [StatefulOwner] forever util you remove the observer.
     * You can add observer at any time, even before the initialization of Matrix
     */
    @Synchronized
    override fun observeForever(observer: IStateObserver) {
        val wrapper = observerMap[observer]
        if (wrapper != null) {
            wrapper.checkLifecycle(null)
        } else {
            observerMap[observer] = ObserverWrapper(observer, this)
            if (async) {
                state.dispatch?.invokeAsync(observer)
            } else {
                state.dispatch?.invoke(observer)
            }
        }
    }

    /**
     * Observer the [StatefulOwner] with lifecycle. It is useful for observers from Activities
     *
     * When lifecycle owner destroyed, the [StatefulOwner] would remove the observer automatically
     * thus avoid leaking the lifecycle owner
     *
     * You can add observer at any time, even before the initialization of Matrix
     */
    @Synchronized
    override fun observeWithLifecycle(lifecycleOwner: LifecycleOwner, observer: IStateObserver) {
        val wrapper = observerMap[observer]
        if (wrapper != null) {
            wrapper.checkLifecycle(lifecycleOwner)
        } else {
            observerMap[observer] = AutoReleaseObserverWrapper(lifecycleOwner, this, observer)
            if (async) {
                state.dispatch?.invokeAsync(observer)
            } else {
                state.dispatch?.invoke(observer)
            }
        }
    }

    @Synchronized
    override fun removeObserver(observer: IStateObserver) {
        observerMap.remove(observer)
    }

    @Synchronized
    protected fun turnOn() {
        if (state == State.ON) {
            return
        }
        state = State.ON
        dispatchStateChanged()
    }

    @Synchronized
    protected fun turnOff() {
        if (state == State.OFF) {
            return
        }
        state = State.OFF
        dispatchStateChanged()
    }

    private fun ((observer: IStateObserver) -> Unit).invokeAsync(observer: IStateObserver) {
        if (observer is ISerialObserver) {
            if (observer.serial()) {
                MatrixLifecycleThread.handler.post {
                    this.invoke(observer)
                }
                return
            }
        }
        MatrixLifecycleThread.executor.execute(object : Runnable {
            override fun run() {
                this@invokeAsync.invoke(observer)
            }

            override fun toString(): String {
                return "$observer"
            }
        })
    }

    private fun dispatchStateChanged() {
        if (async) {
            val s = state // copy current state
            observerMap.forEach {
                s.dispatch?.invokeAsync(it.key)
            }
        } else {
            observerMap.forEach {
                state.dispatch?.invoke(it.key)
            }
        }
    }
}

/**
 * A delegate class for converting lifecycle to StateOwner.
 *
 * @author aurorani
 * @since 2021/9/24
 */
class LifecycleDelegateStatefulOwner private constructor(
    private val source: LifecycleOwner,
    async: Boolean = true
) : StatefulOwner(async), LifecycleObserver {

    companion object {
        fun LifecycleOwner.toStateOwner(): LifecycleDelegateStatefulOwner =
            LifecycleDelegateStatefulOwner(this)
    }

    init {
        source.lifecycle.addObserver(this)
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_CREATE)
    fun onCreate() = turnOff()

    @OnLifecycleEvent(Lifecycle.Event.ON_START)
    fun onReceiveStart() = turnOn()

    @OnLifecycleEvent(Lifecycle.Event.ON_STOP)
    fun onReceiveStop() = turnOff()

    override fun toString() = source.toString()
}

/**
 * The base class for combining multiple source owner with [reduceOperator]
 *
 * @author aurorani
 * @since 2021/9/24
 */
open class MultiSourceStatefulOwner(
    private val reduceOperator: (statefuls: Collection<IStateful>) -> Boolean,
    vararg statefulOwners: IStatefulOwner,
) : StatefulOwner(true), IStateObserver, IMultiSourceOwner {

    private val sourceOwners = ConcurrentLinkedQueue<IStatefulOwner>()

    init {
        statefulOwners.forEach {
            register(it)
        }
    }

    private fun register(owner: IStatefulOwner) {
        owner.let {
            if (it is MultiSourceStatefulOwner) {
                throw IllegalArgumentException("NOT allow to add MultiSourceStatefulOwner as source, consider to add its shadow owner")
            }
            sourceOwners.add(it)
            it.observeForever(this)
        }
    }

    private fun unregister(owner: StatefulOwner) {
        owner.also {
            sourceOwners.remove(it)
            it.removeObserver(this)
            onStateChanged()
        }
    }

    override fun addSourceOwner(owner: StatefulOwner) = register(owner)

    open fun addSourceOwner(owner: LifecycleOwner): StatefulOwner =
        owner.toStateOwner().also { addSourceOwner(it) }

    override fun removeSourceOwner(owner: StatefulOwner) = unregister(owner)

    override fun on() = onStateChanged()

    override fun off() = onStateChanged()

    override fun active() = if (sourceOwners.isEmpty()) {
        super.active()
    } else {
        reduceOperator(sourceOwners).also {
            if (it) {
                turnOn()
            } else {
                turnOff()
            }
        }
    }

    /**
     * Callback function while state of any source owners is changed.
     */
    private fun onStateChanged() {
        if (reduceOperator(sourceOwners)) {
            turnOn()
        } else {
            turnOff()
        }
    }
}

/**
 * Immutable version of [MultiSourceStatefulOwner]
 *
 * @author aurorani
 * @since 2021/9/24
 */
open class ImmutableMultiSourceStatefulOwner(
    reduceOperator: (statefuls: Collection<IStateful>) -> Boolean,
    vararg args: IStatefulOwner
) : MultiSourceStatefulOwner(reduceOperator, *args) {
    final override fun addSourceOwner(owner: StatefulOwner) {
        throw UnsupportedOperationException()
    }

    final override fun addSourceOwner(owner: LifecycleOwner): StatefulOwner {
        throw UnsupportedOperationException()
    }

    final override fun removeSourceOwner(owner: StatefulOwner) {
        throw UnsupportedOperationException()
    }
}

class ReduceOperators {
    companion object {
        val OR = { statefuls: Collection<IStateful> ->
            statefuls.any { it.active() }
        }

        val AND = { statefuls: Collection<IStateful> ->
            statefuls.all { it.active() }
        }

        val NONE = { statefuls: Collection<IStateful> ->
            statefuls.none { it.active() }
        }
    }
}