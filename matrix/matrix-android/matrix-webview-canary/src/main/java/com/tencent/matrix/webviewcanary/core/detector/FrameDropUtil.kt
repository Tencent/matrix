package com.tencent.matrix.webviewcanary.core.detector

enum class DropStatus {
    DROPPED_FROZEN,
    DROPPED_HIGH,
    DROPPED_MIDDLE,
    DROPPED_NORMAL,
    NO_DROP_FOR_120,
    NO_DROP_FOR_90,
    NO_DROP_FOR_60
}

const val CONTENT_KEY_HOST = "HOST"
const val CONTENT_KEY_TOTAL_COST_COUNT = "TOTAL_COST_COUNT"