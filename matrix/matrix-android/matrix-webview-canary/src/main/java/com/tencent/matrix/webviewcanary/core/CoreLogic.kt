package com.tencent.matrix.webviewcanary.core

import com.tencent.matrix.report.Issue
import com.tencent.matrix.report.IssuePublisher.OnIssueDetectListener
import com.tencent.matrix.webviewcanary.WebViewCanaryPlugin

class CoreLogic(private val mPlugin: WebViewCanaryPlugin) : OnIssueDetectListener {
    @Volatile
    private var mStarted = false
    private val mCollector by lazy { WebViewPerformanceDataCollector(this, mPlugin.getConfig()) }

    init {
        WebViewReporterRegistry.INSTANCE.addReporterRegisterListener(object : WebViewReporterRegistry.ReporterRegisterListener {
            override fun onReporterRegister(name: String, reporter: IWebViewPerformanceReporter) {
                if (mStarted) {
                    reporter.startReportPerformance(REPORTER_TAG_FRAME_DROP, mCollector)
                }
            }
        })
    }

    fun start() {
        WebViewReporterRegistry.INSTANCE.traversalReporter { _, reporter ->
            reporter.startReportPerformance(REPORTER_TAG_FRAME_DROP, mCollector)
        }
        mStarted = true
    }

    fun stop() {
        WebViewReporterRegistry.INSTANCE.traversalReporter { _, reporter ->
            reporter.stopReportPerformance()
        }
        mStarted = false
    }

    override fun onDetectIssue(issue: Issue?) {
        if (issue == null) {
            return
        }

        if (issue.tag == null) {
            issue.tag = PLUGIN_TAG
        }

        mPlugin.onDetectIssue(issue)
    }
}