package com.tencent.matrix.util

import android.annotation.SuppressLint
import android.app.ActivityManager
import android.os.Build

/**
 * Created by Yves on 2021/12/2
 */
@SuppressLint("NewApi")
fun ActivityManager.RecentTaskInfo.contentToString(): String {
    return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
        this.toString()
    } else {
        try {
            "RecentTaskInfo{id=" + id +
                    " persistentId=" + persistentId +
                    " baseIntent=" + baseIntent + " baseActivity=" + baseActivity +
                    " topActivity=" + topActivity + " origActivity=" + origActivity +
                    " numActivities=" + numActivities +
                    "}"
        } catch (e: Throwable) {
            this.toString()
        }
    }
}