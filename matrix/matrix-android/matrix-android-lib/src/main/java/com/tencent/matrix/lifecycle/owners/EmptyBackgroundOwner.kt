package com.tencent.matrix.lifecycle.owners

import com.tencent.matrix.lifecycle.ImmutableMultiSourceStatefulOwner
import com.tencent.matrix.lifecycle.ReduceOperators

/**
 * State-ON:
 * 1. Without any foreground widgets subscribed by [CombinedProcessForegroundOwner]
 * 2. Activity stack is empty
 *
 * Created by Yves on 2021/10/27
 */
object EmptyBackgroundOwner: ImmutableMultiSourceStatefulOwner(
    ReduceOperators.NONE,
    ActivityRecorder,
    CombinedProcessForegroundOwner
)