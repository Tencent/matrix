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
    private val mReporterRegisterListenerList : MutableList<ReporterRegisterListener>

    init {
        mRegistry = ConcurrentHashMap()
        mReporterRegisterListenerList = ArrayList()
    }

    fun registerReporter(name : String, reporter: IWebViewPerformanceReporter){
        mRegistry[name] = reporter

        synchronized(this) {
            mReporterRegisterListenerList.forEach {
                it.onReporterRegister(name, reporter)
            }
        }
    }

    fun addReporterRegisterListener(listener: ReporterRegisterListener) {
        synchronized(this) {
            mReporterRegisterListenerList.add(listener)
        }
    }

    fun traversalReporter(func : (name : String, reporter : IWebViewPerformanceReporter) -> Unit) {
        val tempMap = HashMap(mRegistry)

        tempMap.forEach {
                (key, value ) -> func(key, value)
        }
    }

    interface ReporterRegisterListener {
        fun onReporterRegister(name: String, reporter: IWebViewPerformanceReporter)
    }
}