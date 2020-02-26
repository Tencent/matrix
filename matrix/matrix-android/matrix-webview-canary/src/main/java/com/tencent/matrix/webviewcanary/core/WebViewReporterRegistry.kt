package com.tencent.matrix.webviewcanary.core

import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.ConcurrentMap

class WebViewReporterRegistry {
    companion object {
        val INSTANCE : WebViewReporterRegistry by lazy {
            WebViewReporterRegistry()
        }
    }

    private val mRegistry : ConcurrentMap<String, IWebViewPerformanceReporter>

    init {
        mRegistry = ConcurrentHashMap()
    }

    fun registerReporter (name : String, reporter: IWebViewPerformanceReporter){
        mRegistry[name] = reporter
    }

    fun traversalReporter(func : (name : String, reporter : IWebViewPerformanceReporter) -> Unit) {
        val tempMap = HashMap(mRegistry)

        tempMap.forEach {
                (key, value ) -> func(key, value)
        }
    }
}