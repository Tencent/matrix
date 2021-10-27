package com.tencent.matrix.lifecycle.owners

import android.app.Application
import android.content.ContentProvider
import android.content.ContentValues
import android.content.Context
import android.database.Cursor
import android.net.Uri
import androidx.annotation.NonNull
import com.tencent.matrix.lifecycle.owners.MultiProcessLifecycleInitializer.Companion.init

/**
 * You should init [com.tencent.matrix.Matrix] or call [init] manually before creating any Activity
 * Created by Yves on 2021/9/14
 */
class MultiProcessLifecycleInitializer : ContentProvider() {

    companion object {

        @Volatile
        private var inited = false

        @JvmStatic
        fun init(@NonNull context: Context) {
            if (inited) {
                return
            }
            inited = true
            MultiProcessLifecycleOwner.init(context)
            ActivityRecorder.init(context.applicationContext as Application)
        }
    }

    override fun onCreate(): Boolean {
        context?.let {
            inited = true
            MultiProcessLifecycleOwner.init(it)
            ActivityRecorder.init(it.applicationContext as Application)
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