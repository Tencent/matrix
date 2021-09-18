package com.tencent.matrix.lifecycle

import android.content.ContentProvider
import android.content.ContentValues
import android.content.Context
import android.database.Cursor
import android.net.Uri
import androidx.annotation.NonNull
import androidx.annotation.RestrictTo
import com.tencent.matrix.util.MatrixLog
import com.tencent.matrix.util.MatrixUtil

/**
 * Created by Yves on 2021/9/14
 */
// TODO: 2021/9/18 不暴露 lifecycle，允许异步 addObserver，但需要保证线程安全，using handler ?
@RestrictTo(RestrictTo.Scope.LIBRARY_GROUP_PREFIX)
class MultiProcessLifecycleInitializer: ContentProvider() {

    companion object {
        @JvmStatic
        fun initForOtherProcesses(@NonNull context: Context) {
            if (MatrixUtil.isInMainProcess(context)) {
                MatrixLog.i(
                    TAG,
                    "main process lifecycle owner was initialized by onCreate"
                )
                return
            }
            MultiProcessLifecycleOwner.init(context)
        }
    }

    override fun onCreate(): Boolean {
        context?.let {
            MultiProcessLifecycleOwner.init(it)
        } ?: run {
            throw IllegalStateException("context is null !!!")
        }
        return true
    }

    override fun query(
        uri: Uri,
        projection: Array<out String>?,
        selection: String?,
        selectionArgs: Array<out String>?,
        sortOrder: String?
    ): Cursor? {
        return null
    }

    override fun getType(uri: Uri): String? {
        return null
    }

    override fun insert(uri: Uri, values: ContentValues?): Uri? {
        return null
    }

    override fun delete(uri: Uri, selection: String?, selectionArgs: Array<out String>?): Int {
        return 0
    }

    override fun update(
        uri: Uri,
        values: ContentValues?,
        selection: String?,
        selectionArgs: Array<out String>?
    ): Int {
        return 0
    }

}