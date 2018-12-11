package com.tencent.matrix.plugin.extension;


/**
 * Created by williamjin on 17/12/26.
 */

class MatrixDelUnusedResConfiguration {

    boolean enable
    String variant
    boolean needSign
    boolean shrinkArsc
    String apksignerPath
    Set<String> unusedResources
    Set<String> ignoreResources

    MatrixDelUnusedResConfiguration() {
        enable = false
        variant = ""
        needSign = false
        shrinkArsc = false
        apksignerPath = ""
        unusedResources = new HashSet<>()
        ignoreResources = new HashSet<>()
    }

    @Override
    String toString() {
        """| enable = ${enable}
           | variant = ${variant}
           | needSign = ${needSign}
           | shrinkArsc = ${shrinkArsc}
           | apkSignerPath = ${apksignerPath}
           | unusedResources = ${unusedResources}
           | ignoreResources = ${ignoreResources}
        """.stripMargin()
    }
}
