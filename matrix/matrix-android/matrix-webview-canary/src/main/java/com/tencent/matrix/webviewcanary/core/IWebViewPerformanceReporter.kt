package com.tencent.matrix.webviewcanary.core

interface IWebViewPerformanceReporter {
    fun startReportPerformance(tag : Int, collector : IWebViewPerformanceCollector)

    fun stopReportPerformance()


}