/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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