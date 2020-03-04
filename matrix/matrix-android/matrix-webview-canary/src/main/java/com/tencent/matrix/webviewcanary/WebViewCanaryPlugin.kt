package com.tencent.matrix.webviewcanary

import android.app.Application
import com.tencent.matrix.plugin.Plugin
import com.tencent.matrix.plugin.PluginListener
import com.tencent.matrix.report.Issue
import com.tencent.matrix.util.MatrixUtil
import com.tencent.matrix.webviewcanary.core.Config
import com.tencent.matrix.webviewcanary.core.CoreLogic
import com.tencent.matrix.webviewcanary.util.GlobalStorage

class WebViewCanaryPlugin(private val mConfig: Config = Config.EMPTY) : Plugin() {
    companion object {
        private const val TAG = "Matrix.WebViewPerformancePlugin"
    }

    private val mCore: CoreLogic = CoreLogic(this)

    override fun init(app: Application?, listener: PluginListener?) {
        super.init(app, listener)

        GlobalStorage.packageName = app?.packageName ?: ""
        GlobalStorage.processName = MatrixUtil.getProcessName(app)
    }

    override fun start() {
        super.start()
        mCore.start()
    }

    override fun stop() {
        super.stop()
        mCore.stop()
    }

    fun getConfig(): Config {
        return mConfig
    }

    override fun onDetectIssue(issue: Issue?) {
        super.onDetectIssue(issue)
    }
}