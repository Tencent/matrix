package com.tencent.matrix.lifecycle.owners

import com.tencent.matrix.lifecycle.MultiSourceStatefulOwner
import com.tencent.matrix.lifecycle.ReduceOperators


/**
 * State-ON which means Foreground state:
 * with started Activity、ForegroundServices or floating Views added to Window
 *
 * Note: Monitors for ForegroundServices and floating Views are implemented by wechat
 * and attach to this with [addSourceOwner].
 *
 * Created by Yves on 2021/9/18
 */
object CombinedProcessForegroundOwner : MultiSourceStatefulOwner(
    ReduceOperators.OR,
    MatrixProcessLifecycleOwner.startedStateOwner
)