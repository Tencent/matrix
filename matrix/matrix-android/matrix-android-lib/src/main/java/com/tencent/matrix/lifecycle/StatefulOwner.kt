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

fun IStatefulOwner.reverse(): IStatefulOwner = object : IStatefulOwner by this {
    override fun active() = !this@reverse.active()
}

interface IStateObserver {
    fun on()
    fun off()
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

private open class ObserverWrapper(val observer: IStateObserver, val switchOwner: StatefulOwner) {
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
        switchOwner.removeObserver(observer)
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

open class StatefulOwner : IStatefulOwner {

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
            state.dispatch?.invoke(observer)
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
            state.dispatch?.invoke(observer)
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
        observerMap.forEach {
            state.dispatch?.invoke(it.key)
        }
    }

    @Synchronized
    protected fun turnOff() {
        if (state == State.OFF) {
            return
        }
        state = State.OFF
        observerMap.forEach {
            state.dispatch?.invoke(it.key)
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
    private val source: LifecycleOwner
) : StatefulOwner(), LifecycleObserver {

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
    vararg statefulOwners: IStatefulOwner
) : StatefulOwner(), IStateObserver, IMultiSourceOwner {

    private val sourceOwners = ConcurrentLinkedQueue<IStatefulOwner>()

    init {
        statefulOwners.forEach {
            register(it)
        }
    }

    private fun register(owner: IStatefulOwner) {
        owner.let {
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