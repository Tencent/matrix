package com.tencent.matrix.memory.canary.lifecycle.owners

import com.tencent.matrix.memory.canary.lifecycle.ImmutableMultiSourceStatefulOwner
import com.tencent.matrix.memory.canary.lifecycle.ReduceOperators

/**
 * 「深后台」定义：进程出于后台并且没有 Activity
 *
 * notice Events:
 * [androidx.lifecycle.Lifecycle.Event.ON_START] : 进入深后台状态
 * [androidx.lifecycle.Lifecycle.Event.ON_STOP] : 退出深后台状态
 *
 * Created by Yves on 2021/9/24
 */
// TODO: 2021/10/11 move
object DeepBackgroundStatefulOwner : ImmutableMultiSourceStatefulOwner(
    ReduceOperators.NONE,
    ActivityRecord,
    CombinedProcessForegroundStatefulOwner
)