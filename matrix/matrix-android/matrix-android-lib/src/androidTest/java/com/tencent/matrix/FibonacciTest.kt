package com.tencent.matrix

import android.util.Log
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.math.min

/**
 * Created by Yves on 2021/11/25
 */
@RunWith(AndroidJUnit4::class)
class FibonacciTest {

    companion object {
        private const val TAG = "FibonacciTest"
    }

    class FibonacciInterval(private val maxVal: Long) {
        private var n = 34L // 10th
        private var m = 55L // 11th

        fun next(): Long {
            return min((n + m).also { n = m; m = it }, maxVal)
        }
    }

    @Test
    fun test() {
        val f = FibonacciInterval(60 * 1000L)

        for (i in 0..25) {
            Log.d(TAG, "${f.next()}")
        }
    }
}