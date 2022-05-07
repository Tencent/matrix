package com.tencent.matrix

import android.util.Log
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.tencent.matrix.lifecycle.IStateObserver
import com.tencent.matrix.lifecycle.StatefulOwner
import com.tencent.matrix.lifecycle.reverse
import com.tencent.matrix.lifecycle.shadow
import org.junit.runner.RunWith
import org.junit.Test

/**
 * Created by Yves on 2022/1/7
 */
@RunWith(AndroidJUnit4::class)
class ShadowStatefulOwnerTest {

    companion object {
        private const val TAG = "ShadowStatefulOwnerTest"
    }

    @Test
    fun test() {
        val origin = object : StatefulOwner() {
            fun on() = turnOn()
            fun off() = turnOff()
        }


        val shadow = origin.shadow()
        val reverse = origin.reverse()

        origin.observeForever(object :IStateObserver {
            override fun on() {
                Log.d(TAG, "origin on")
//                origin.removeObserver(this)
            }

            override fun off() {
                Log.d(TAG, "origin off")
//                origin.removeObserver(this)
            }
        })

        shadow.observeForever(object :IStateObserver {
            override fun on() {
                Log.d(TAG, "shadow on")
//                shadow.removeObserver(this)
            }

            override fun off() {
                Log.d(TAG, "shadow off")
//                shadow.removeObserver(this)
            }
        })


        reverse.observeForever(object :IStateObserver {
            override fun on() {
                Log.d(TAG, "reverse on")
                reverse.removeObserver(this)
            }

            override fun off() {
                Log.d(TAG, "reverse off")
                reverse.removeObserver(this)
            }
        })

        origin.on()
        origin.off()

        Log.d(TAG, "============")

        origin.on()
        origin.off()

        Log.d(TAG, "done")


    }
}