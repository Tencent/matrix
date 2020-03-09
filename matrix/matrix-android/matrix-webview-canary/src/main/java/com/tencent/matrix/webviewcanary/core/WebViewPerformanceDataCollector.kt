package com.tencent.matrix.webviewcanary.core

import com.tencent.matrix.report.IssuePublisher
import com.tencent.matrix.webviewcanary.core.detector.WebViewFrameDropDetector

class WebViewPerformanceDataCollector(private val mIssueDetectListener: IssuePublisher.OnIssueDetectListener, private val mConfig: Config) :
        IWebViewPerformanceCollector {

    private val mFrameDropDetector by lazy {
        WebViewFrameDropDetector(
                mIssueDetectListener,
                mConfig
        )
    }

    override fun reportFrameCost(
            host: String,
            deviceRefreshRate : Int,
            fps : Double,
            totalCostCount: Int,
            noDropFrameCountFor120Hz: Int,
            noDropFrameCountFor90Hz: Int,
            noDropFrameCountFor60Hz: Int,
            dropFrameCostSequence: DoubleArray
    ) {
        mFrameDropDetector.onFrameCostDataReceived(
                host,
                deviceRefreshRate,
                fps,
                totalCostCount,
                noDropFrameCountFor120Hz,
                noDropFrameCountFor90Hz,
                noDropFrameCountFor60Hz,
                dropFrameCostSequence
        )
    }
}