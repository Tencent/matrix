package com.tencent.matrix.lifecycle.supervisor

import android.content.Context
import android.os.*
import com.tencent.matrix.lifecycle.owners.ActivityRecorder
import com.tencent.matrix.util.MatrixUtil

/**
 * Created by Yves on 2021/11/11
 */
class ProcessToken : Parcelable {
    val binder: IBinder
    val pid: Int
    val name: String
    val busy: Boolean


    companion object {

        @JvmStatic
        fun current(context: Context) = ProcessToken(
            Process.myPid(),
            MatrixUtil.getProcessName(context),
            ActivityRecorder.busy
        )

        @JvmField
        val CREATOR: Parcelable.Creator<ProcessToken> = object : Parcelable.Creator<ProcessToken> {
            override fun createFromParcel(src: Parcel): ProcessToken {
                return ProcessToken(src)
            }

            override fun newArray(size: Int): Array<ProcessToken?> {
                return arrayOfNulls(size)
            }
        }
    }

    constructor(pid: Int, processName: String, busy: Boolean) {
        this.binder = Binder()
        this.pid = pid
        this.name = processName
        this.busy = busy
    }

    constructor(src: Parcel) {
        this.binder = src.readStrongBinder()
        this.pid = src.readInt()
        this.name = src.readString() ?: ""
        this.busy = src.readInt() != 0
    }

    override fun describeContents(): Int {
        return 0
    }

    override fun writeToParcel(dest: Parcel, flags: Int) {
        dest.writeStrongBinder(binder)
        dest.writeInt(pid)
        dest.writeString(name)
        dest.writeInt(if (busy) 1 else 0)
    }

    fun linkToDeath(recipient: IBinder.DeathRecipient) {
        binder.linkToDeath(recipient, 0)
    }

    override fun equals(other: Any?): Boolean {
        if (other == null) {
            return false
        }
        if (other !is ProcessToken) {
            return false
        }
        return name == other.name && pid == other.pid
    }

    override fun hashCode(): Int {
        var result = pid
        result = 31 * result + name.hashCode()
        return result
    }

    override fun toString(): String {
        return "ProcessToken(pid=$pid, name='$name', busy=$busy)"
    }
}