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

package com.tencent.matrix.plugin.task

import com.android.build.api.transform.Status
import com.android.build.gradle.internal.tasks.DexArchiveBuilderTask
import com.android.builder.model.AndroidProject.FD_OUTPUTS
import com.google.common.base.Joiner
import com.tencent.matrix.javalib.util.Log
import com.tencent.matrix.plugin.compat.CreationConfig
import com.tencent.matrix.plugin.trace.MatrixTrace
import com.tencent.matrix.trace.extension.MatrixTraceExtension
import org.gradle.api.Action
import org.gradle.api.DefaultTask
import org.gradle.api.Project
import org.gradle.api.file.ConfigurableFileCollection
import org.gradle.api.file.FileType
import org.gradle.api.file.RegularFileProperty
import org.gradle.api.provider.Property
import org.gradle.api.tasks.*
import org.gradle.work.ChangeType
import org.gradle.work.Incremental
import org.gradle.work.InputChanges
import java.io.File
import java.util.concurrent.Callable
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.ExecutionException

abstract class MatrixTraceTask : DefaultTask() {
    companion object {
        private const val TAG: String = "Matrix.TraceTask"

        fun getTraceClassOut(project: Project, creationConfig: CreationConfig): String {

            val variantDirName = creationConfig.variant.dirName
            return Joiner.on(File.separatorChar).join(
                project.buildDir,
                FD_OUTPUTS,
                "traceClassOut",
                variantDirName)
        }
    }

    @get:Incremental
    @get:InputFiles
    abstract val classInputs: ConfigurableFileCollection

    @get:InputFile
    @get:Optional
    @get:PathSensitive(PathSensitivity.ABSOLUTE)
    abstract val baseMethodMapFile: RegularFileProperty

    @get:InputFile
    @get:Optional
    @get:PathSensitive(PathSensitivity.ABSOLUTE)
    abstract val blockListFile: RegularFileProperty

    @get:Input
    @get:Optional
    abstract val mappingDir: Property<String>

    @get:Input
    abstract val traceClassOutputDirectory: Property<String>

    @get:OutputFiles
    abstract val classOutputs: ConfigurableFileCollection

    @get:OutputFile
    @get:PathSensitive(PathSensitivity.ABSOLUTE)
    @get:Optional
    abstract val ignoreMethodMapFileOutput: RegularFileProperty

    @get:OutputFile
    @get:PathSensitive(PathSensitivity.ABSOLUTE)
    @get:Optional
    abstract val methodMapFileOutput: RegularFileProperty

    @get:Optional
    abstract val skipCheckClass: Property<Boolean>

    @TaskAction
    fun execute(inputChanges: InputChanges) {

        val incremental = inputChanges.isIncremental

        val changedFiles = ConcurrentHashMap<File, Status>()

        if (incremental) {
            inputChanges.getFileChanges(classInputs).forEach { change ->
                if (change.fileType == FileType.DIRECTORY) return@forEach

                changedFiles[change.file] = when {
                    change.changeType == ChangeType.REMOVED -> Status.REMOVED
                    change.changeType == ChangeType.MODIFIED -> Status.CHANGED
                    change.changeType == ChangeType.ADDED -> Status.ADDED
                    else -> Status.NOTCHANGED
                }
            }
        }

        val start = System.currentTimeMillis()
        try {

            val outputDirectory = File(traceClassOutputDirectory.get())
            MatrixTrace(
                    ignoreMethodMapFilePath = ignoreMethodMapFileOutput.asFile.get().absolutePath,
                    methodMapFilePath = methodMapFileOutput.asFile.get().absolutePath,
                    baseMethodMapPath = baseMethodMapFile.asFile.orNull?.absolutePath,
                    blockListFilePath = blockListFile.asFile.orNull?.absolutePath,
                    mappingDir = mappingDir.get(),
                    project = project
            ).doTransform(
                    classInputs = classInputs.files,
                    changedFiles = changedFiles,
                    isIncremental = incremental,
                    skipCheckClass = this.skipCheckClass.get(),
                    traceClassDirectoryOutput = outputDirectory,
                    inputToOutput = ConcurrentHashMap(),
                    legacyReplaceChangedFile = null,
                    legacyReplaceFile = null,
                    uniqueOutputName = false
            )

        } catch (e: ExecutionException) {
            e.printStackTrace()
        }
        val cost = System.currentTimeMillis() - start
        Log.i(TAG, " Insert matrix trace instrumentations cost time: %sms.", cost)
    }

    fun wired(creationConfig: CreationConfig, task: DexArchiveBuilderTask) {

        Log.i(TAG, "Wiring ${this.name} to task '${task.name}'.")

        task.dependsOn(this)

        // Wire minify task outputs to trace task inputs.
        classInputs.setFrom(task.mixedScopeClasses.files)

        val traceClassOut = getTraceClassOut(project, creationConfig)

        val outputs = task.mixedScopeClasses.files.map {
            File(traceClassOut, MatrixTrace.appendSuffix(it, "traced"))
        }

        classOutputs.from(outputs)

        // Wire trace task outputs to dex task inputs.
        task.mixedScopeClasses.setFrom(classOutputs)
    }

    class CreationAction(
            private val creationConfig: CreationConfig,
            private val extension: MatrixTraceExtension
    ) : Action<MatrixTraceTask>, BaseCreationAction<MatrixTraceTask>(creationConfig) {

        override val name = computeTaskName("matrix", "Trace")
        override val type = MatrixTraceTask::class.java

        override fun execute(task: MatrixTraceTask) {

            val project = creationConfig.project
            val variantDirName = creationConfig.variant.dirName

            val mappingOut = Joiner.on(File.separatorChar).join(
                    project.buildDir,
                    FD_OUTPUTS,
                    "mapping",
                    variantDirName)

            val traceClassOut = getTraceClassOut(project, creationConfig)

            // Input properties
            val baseMethodMapFile = File(extension.baseMethodMapFile)
            if (baseMethodMapFile.exists()) {
                task.baseMethodMapFile.set(baseMethodMapFile)
            }
            val blackListFile = File(extension.blackListFile)
            if (blackListFile.exists()) {
                task.blockListFile.set(blackListFile)
            }
            task.mappingDir.set(mappingOut)
            task.traceClassOutputDirectory.set(traceClassOut)
            task.skipCheckClass.set(extension.isSkipCheckClass)

            // Output properties
            task.ignoreMethodMapFileOutput.set(File("$mappingOut/ignoreMethodMapping.txt"))
            task.methodMapFileOutput.set(File("$mappingOut/methodMapping.txt"))
        }
    }

}