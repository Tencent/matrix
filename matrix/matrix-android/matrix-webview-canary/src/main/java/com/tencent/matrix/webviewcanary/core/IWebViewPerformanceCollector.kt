package com.tencent.matrix.webviewcanary.core

interface IWebViewPerformanceCollector {
    fun reportFrameCost(
        host: String,
        totalCostCount: Int,
        noDropFrameCountFor120Hz: Int,
        noDropFrameCountFor90Hz: Int,
        noDropFrameCountFor60Hz: Int,
        dropFrameCostSequence: DoubleArray
    )
}