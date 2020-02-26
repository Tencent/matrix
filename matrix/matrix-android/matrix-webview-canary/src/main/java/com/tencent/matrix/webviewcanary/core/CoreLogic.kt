package com.tencent.matrix.webviewcanary.core

import com.tencent.matrix.report.Issue
import com.tencent.matrix.report.IssuePublisher.OnIssueDetectListener
import com.tencent.matrix.webviewcanary.WebViewCanaryPlugin

class CoreLogic(private val mPlugin: WebViewCanaryPlugin) : OnIssueDetectListener {
    private val mCollector by lazy { WebViewPerformanceDataCollector(this, mPlugin.getConfig()) }

    fun start() {
        WebViewReporterRegistry.INSTANCE.traversalReporter { _, reporter ->
            reporter.startReportPerformance(REPORTER_TAG_FRAME_DROP, mCollector)
        }
    }

    fun stop() {
        WebViewReporterRegistry.INSTANCE.traversalReporter { _, reporter ->
            reporter.stopReportPerformance()
        }
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