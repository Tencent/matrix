package com.tencent.matrix.memory.canary

import android.app.Application
import android.content.ContentProvider
import android.content.ContentValues
import android.database.Cursor
import android.net.Uri

/**
 * Created by Yves on 2021/9/28
 */
open class MemoryCanaryInitializer: ContentProvider() {
    companion object {
        @Volatile
        private var instance : MemoryCanaryInitializer? = null

        var application: Application? = null
            private set

        fun init(app: Application) {
            if (instance != null) {
                return
            }
            application = app
            instance = MemoryCanaryInitializer().also { it.onInit() }
        }
    }

    protected open fun onInit() {
    }

    override fun onCreate(): Boolean {
        if (instance != null) {
            return true
        }
        application = context?.applicationContext as Application
        instance = this.also { onInit() }
        return true
    }

    override fun query(
        p0: Uri,
        p1: Array<out String>?,
        p2: String?,
        p3: Array<out String>?,
        p4: String?
    ): Cursor? {
        return null
    }

    override fun getType(p0: Uri): String? {
        return null
    }

    override fun insert(p0: Uri, p1: ContentValues?): Uri? {
        return null
    }

    override fun delete(p0: Uri, p1: String?, p2: Array<out String>?): Int {
        return 0
    }

    override fun update(p0: Uri, p1: ContentValues?, p2: String?, p3: Array<out String>?): Int {
        return 0
    }
}