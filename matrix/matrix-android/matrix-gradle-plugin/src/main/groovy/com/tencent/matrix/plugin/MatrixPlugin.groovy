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

import com.tencent.matrix.javalib.util.Log
import com.tencent.matrix.javalib.util.Util
import com.tencent.matrix.plugin.extension.MatrixExtension
import com.tencent.matrix.plugin.extension.MatrixDelUnusedResConfiguration
import com.tencent.matrix.plugin.extension.MatrixTraceExtension
import com.tencent.matrix.plugin.task.RemoveUnusedResourcesTask
import org.gradle.api.GradleException
import org.gradle.api.Plugin
import org.gradle.api.Project

/**
 * Created by zhangshaowen on 17/6/16.
 */
class MatrixPlugin implements Plugin<Project> {
    private static final String TAG = "Matrix.MatrixPlugin"

    @Override
    void apply(Project project) {
        project.extensions.create("matrix", MatrixExtension)
        project.matrix.extensions.create("trace", MatrixTraceExtension)
        project.matrix.extensions.create("removeUnusedResources", MatrixDelUnusedResConfiguration)

        if (!project.plugins.hasPlugin('com.android.application')) {
            throw new GradleException('Matrix Plugin, Android Application plugin required')
        }

        project.afterEvaluate {
            def android = project.extensions.android
            def configuration = project.matrix
            android.applicationVariants.all { variant ->

                Log.i(TAG, "Trace enable is %s", configuration.trace.enable)
                if (configuration.trace.enable) {
                    String output = configuration.output
                    if (Util.isNullOrNil(output)) {
                        configuration.output = project.getBuildDir().getAbsolutePath() + File.separator + "matrix_output"
                        Log.i(TAG, "set matrix output file to " + configuration.output)
                    }
                    com.tencent.matrix.plugin.transform.MatrixTraceTransform.inject(project, variant)
                }

                if (configuration.removeUnusedResources.enable) {
                    if (Util.isNullOrNil(configuration.removeUnusedResources.variant) || variant.name.equalsIgnoreCase(configuration.removeUnusedResources.variant)) {
                        Log.i(TAG, "removeUnusedResources %s", configuration.removeUnusedResources);
                        RemoveUnusedResourcesTask removeUnusedResourcesTask = project.tasks.create("remove" + variant.name.capitalize()+ "UnusedResources", RemoveUnusedResourcesTask)
                        removeUnusedResourcesTask.inputs.property(RemoveUnusedResourcesTask.BUILD_VARIANT, variant.name)
                        project.tasks.add(removeUnusedResourcesTask)
                        removeUnusedResourcesTask.dependsOn variant.packageApplication
                        variant.assemble.dependsOn removeUnusedResourcesTask
                    }
                }

            }
        }
    }
}
