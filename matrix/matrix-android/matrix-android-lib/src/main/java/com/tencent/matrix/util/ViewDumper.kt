package com.tencent.matrix.util

import android.view.View
import android.view.ViewGroup
import com.tencent.matrix.lifecycle.owners.MatrixProcessLifecycleOwner
import java.lang.StringBuilder

/**
 * Created by Yves on 2021/12/27
 */
object ViewDumper {

    @JvmStatic
    fun dump(): Array<String> = MatrixProcessLifecycleOwner.getViews().map {
        dumpInternal(it)
    }.toTypedArray()

    private fun dumpInternal(view: View, level: Int = 0): String {

        if (view is ViewGroup) {
            return dumpViewGroup(view, level)
        }

        return dumpView(view, level)
    }

    private fun dumpView(view: View, level: Int): String {
        val prefix = buildString {
            for (j in 0 until level) { // prefix
                append("-")
            }
        }
        return "$prefix${view.visibility}:${view.windowVisibility}:$view:[${view.x},${view.y},${view.width},${view.height}]:${view.context}\n"
    }

    private fun dumpViewGroup(viewGroup: ViewGroup, level: Int): String {
        val childCount = viewGroup.childCount
        val builder = StringBuilder()
        for (j in 0 until level) { // prefix
            builder.append("-")
        }
        builder.append("${viewGroup.visibility}:${viewGroup.windowVisibility}:$viewGroup:[${viewGroup.x},${viewGroup.y},${viewGroup.width},${viewGroup.height}]:${viewGroup.context}\n")
        for (i in 0 until childCount) {
            val child = viewGroup.getChildAt(i)
            builder.append(dumpInternal(child, level + 1))
        }
        return builder.toString()
    }
}