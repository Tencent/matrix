package com.tencent.matrix.plugin.extension

/**
 * Created by zhangshaowen on 17/6/16.
 */
class MatrixTraceExtension {
    String customDexTransformName
    boolean enable
    String baseMethodMapFile
    String blackListFile

    MatrixTraceExtension() {
        customDexTransformName = ""
        enable = true
        baseMethodMapFile = ""
        blackListFile = ""
    }

    @Override
    String toString() {
        """|customDexTransformName = ${customDexTransformName}
           | enable = ${enable}
           | baseMethodMapFile = ${baseMethodMapFile}
           | blackListFile = ${blackListFile}
        """.stripMargin()
    }
}