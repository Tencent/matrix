package com.tencent.matrix.memory.canary.lifecycle.owners

import com.tencent.matrix.lifecycle.MultiProcessLifecycleOwner
import com.tencent.matrix.memory.canary.lifecycle.LifecycleDelegateStatefulOwner.Companion.toStateOwner
import com.tencent.matrix.memory.canary.lifecycle.MultiSourceStatefulOwner
import com.tencent.matrix.memory.canary.lifecycle.ReduceOperators


/**
 * 进程前台判断条件：Activity 在前台、有 ForegroundService、有浮窗
 *
 * Created by Yves on 2021/9/18
 */
object CombinedProcessForegroundStatefulOwner : MultiSourceStatefulOwner(
    ReduceOperators.OR,
    MultiProcessLifecycleOwner.get().toStateOwner()
)