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

package com.tencent.matrix.plugin.compat

import com.android.build.gradle.AppExtension
import com.tencent.matrix.javalib.util.Log
import com.tencent.matrix.plugin.trace.MatrixTraceInjection
import com.tencent.matrix.plugin.transform.MatrixTraceLegacyTransform
import com.tencent.matrix.trace.extension.ITraceSwitchListener
import com.tencent.matrix.trace.extension.MatrixTraceExtension
import org.gradle.api.Project

class MatrixTraceCompat : ITraceSwitchListener {

    companion object {
        const val TAG = "Matrix.TraceCompat"
    }

    var traceInjection: MatrixTraceInjection? = null

    init {
        if (VersionsCompat.greatThanOrEqual(AGPVersion.AGP_4_0_0)) {
            traceInjection = MatrixTraceInjection()
        }
    }

    override fun onTraceEnabled(enable: Boolean) {
        traceInjection?.onTraceEnabled(enable)
    }

    fun inject(appExtension: AppExtension, project: Project, extension: MatrixTraceExtension) {
        when {
            VersionsCompat.lessThan(AGPVersion.AGP_3_6_0) ->
                legacyInject(appExtension, project, extension)
            VersionsCompat.greatThanOrEqual(AGPVersion.AGP_4_0_0) -> {
                if ((project.extensions.extraProperties.get("matrix_trace_legacy") as? String?) == "true") {
                    legacyInject(appExtension, project, extension)
                } else {
                    traceInjection!!.inject(appExtension, project, extension)
                }
            }
            else -> Log.e(TAG, "Matrix does not support Android Gradle Plugin " +
                    "${VersionsCompat.androidGradlePluginVersion}!.")
        }
    }

    fun legacyInject(appExtension: AppExtension,
                     project: Project,
                     extension: MatrixTraceExtension) {

        project.afterEvaluate {

            if (!extension.isEnable) {
                return@afterEvaluate
            }

            appExtension.applicationVariants.all {
                MatrixTraceLegacyTransform.inject(extension, project, it)
            }
        }
    }
}
