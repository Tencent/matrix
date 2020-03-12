package com.tencent.matrix.webviewcanary

import com.tencent.matrix.plugin.Plugin
import com.tencent.matrix.report.Issue
import com.tencent.matrix.webviewcanary.core.Config
import com.tencent.matrix.webviewcanary.core.CoreLogic

class WebViewCanaryPlugin(private val mConfig: Config = Config.EMPTY) : Plugin() {
    companion object {
        private const val TAG = "Matrix.WebViewPerformancePlugin"
    }

    private val mCore: CoreLogic = CoreLogic(this)

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