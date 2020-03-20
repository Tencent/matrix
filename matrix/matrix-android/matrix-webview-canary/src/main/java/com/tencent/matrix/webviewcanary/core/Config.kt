package com.tencent.matrix.webviewcanary.core

class Config {
    companion object {
        val EMPTY = Config()
    }

    var frameDropMiddleThreshold = 50
    var frameDropHighThreshold = 100
    var frameDropFrozenThreshold = 1000
}