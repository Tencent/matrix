package com.tencent.matrix.lifecycle

import android.os.Handler
import com.tencent.matrix.util.MatrixHandlerThread

/**
 * Created by Yves on 2022/1/11
 */
internal object MatrixLifecycleThread {
    val handler by lazy(LazyThreadSafetyMode.SYNCHRONIZED) {
        Handler(MatrixHandlerThread.getNewHandlerThread("matrix_li", Thread.NORM_PRIORITY).looper)
    }
}