package com.tencent.matrix.lifecycle.owners

import com.tencent.matrix.lifecycle.LifecycleDelegateStatefulOwner.Companion.toStateOwner
import com.tencent.matrix.lifecycle.MultiSourceStatefulOwner
import com.tencent.matrix.lifecycle.ReduceOperators


/**
 * State-ON which means Foreground state:
 * with started Activity、ForegroundServices or floating Views added to Window
 *
 * Created by Yves on 2021/9/18
 */
object CombinedProcessForegroundOwner : MultiSourceStatefulOwner(
    ReduceOperators.OR,
    MultiProcessLifecycleOwner.startedStateOwner
)