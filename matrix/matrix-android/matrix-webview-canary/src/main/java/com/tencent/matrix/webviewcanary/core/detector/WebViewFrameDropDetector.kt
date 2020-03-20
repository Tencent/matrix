package com.tencent.matrix.webviewcanary.core.detector

import com.tencent.matrix.report.Issue
import com.tencent.matrix.report.IssuePublisher
import com.tencent.matrix.webviewcanary.core.Config
import com.tencent.matrix.webviewcanary.core.IssueType
import org.json.JSONObject

class WebViewFrameDropDetector(
        private val mListener: OnIssueDetectListener,
        private val mConfig: Config
) :
        IssuePublisher(mListener) {

    fun onFrameCostDataReceived(
            host: String,
            deviceRefreshRate: Int,
            fps: Double,
            totalCostCount: Int,
            noDropFrameCountFor120Hz: Int,
            noDropFrameCountFor90Hz: Int,
            noDropFrameCountFor60Hz: Int,
            dropFrameCostSequence: DoubleArray
    ) {
        analysis(
                host, deviceRefreshRate, fps,
                totalCostCount,
                noDropFrameCountFor120Hz,
                noDropFrameCountFor90Hz,
                noDropFrameCountFor60Hz,
                dropFrameCostSequence
        )
    }

    private fun analysis(
            host: String,
            deviceRefreshRate: Int,
            fps: Double,
            totalCostCount: Int,
            noDropFrameCountFor120Hz: Int,
            noDropFrameCountFor90Hz: Int,
            noDropFrameCountFor60Hz: Int,
            dropFrameCostSequence: DoubleArray
    ) {
        val contentObject = JSONObject()
        contentObject.put(CONTENT_KEY_HOST, host)
        contentObject.put(CONTENT_KEY_TOTAL_COST_COUNT, totalCostCount)
        contentObject.put(CONTENT_KEY_FPS, fps)
        contentObject.put(CONTENT_KEY_DEVICE_REFRESH_RATE, deviceRefreshRate)

        contentObject.put(DropStatus.NO_DROP_FOR_120.name, noDropFrameCountFor120Hz)
        contentObject.put(DropStatus.NO_DROP_FOR_90.name, noDropFrameCountFor90Hz)
        contentObject.put(DropStatus.NO_DROP_FOR_60.name, noDropFrameCountFor60Hz)

        var frameDropCountNormal = 0
        var frameDropCountMiddle = 0
        var frameDropCountHigh = 0
        var frameDropCountFrozen = 0

        dropFrameCostSequence.forEach {
            when {
                it > mConfig.frameDropFrozenThreshold -> frameDropCountFrozen++
                it > mConfig.frameDropHighThreshold -> frameDropCountHigh++
                it > mConfig.frameDropMiddleThreshold -> frameDropCountMiddle++
                else -> frameDropCountNormal++
            }
        }

        contentObject.put(DropStatus.DROPPED_NORMAL.name, frameDropCountNormal)
        contentObject.put(DropStatus.DROPPED_MIDDLE.name, frameDropCountNormal)
        contentObject.put(DropStatus.DROPPED_HIGH.name, frameDropCountHigh)
        contentObject.put(DropStatus.DROPPED_FROZEN.name, frameDropCountFrozen)

        val frameDropIssue = Issue(IssueType.ISSUE_FRAME_DROP)
        frameDropIssue.content = contentObject
        mListener.onDetectIssue(frameDropIssue)
    }

    private fun report(issue: Issue) {
        mListener.onDetectIssue(issue)
    }
}