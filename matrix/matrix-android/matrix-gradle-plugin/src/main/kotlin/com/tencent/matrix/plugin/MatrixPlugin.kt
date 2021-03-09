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

package com.tencent.matrix.plugin

import com.android.build.gradle.AppExtension
import com.tencent.matrix.javalib.util.Log
import com.tencent.matrix.plugin.extension.MatrixExtension
import com.tencent.matrix.plugin.extension.MatrixRemoveUnusedResExtension
import com.tencent.matrix.plugin.task.MatrixTasksManager
import com.tencent.matrix.trace.extension.MatrixTraceExtension
import org.gradle.api.GradleException
import org.gradle.api.Plugin
import org.gradle.api.Project
import org.gradle.api.plugins.ExtensionAware

class MatrixPlugin : Plugin<Project> {
    companion object {
        const val TAG = "Matrix.Plugin"
    }

    override fun apply(project: Project) {

        val matrix = project.extensions.create("matrix", MatrixExtension::class.java)
        val traceExtension = (matrix as ExtensionAware).extensions.create("trace", MatrixTraceExtension::class.java)
        val removeUnusedResourcesExtension = matrix.extensions.create("removeUnusedResources", MatrixRemoveUnusedResExtension::class.java)

        if (!project.plugins.hasPlugin("com.android.application")) {
            throw GradleException("Matrix Plugin, Android Application plugin required.")
        }

        project.afterEvaluate {
            Log.setLogLevel(matrix.logLevel)
        }

        MatrixTasksManager().createMatrixTasks(
                project.extensions.getByName("android") as AppExtension,
                project,
                traceExtension,
                removeUnusedResourcesExtension
        )
    }
}
