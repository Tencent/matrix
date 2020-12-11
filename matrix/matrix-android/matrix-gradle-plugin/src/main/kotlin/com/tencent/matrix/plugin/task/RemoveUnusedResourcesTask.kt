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

import com.android.build.gradle.api.BaseVariant
import com.android.builder.model.SigningConfig
import com.tencent.matrix.javalib.util.Log
import com.tencent.matrix.javalib.util.Pair
import com.tencent.matrix.javalib.util.Util
import com.tencent.matrix.plugin.compat.AgpCompat
import com.tencent.matrix.plugin.compat.CreationConfig
import com.tencent.matrix.plugin.extension.MatrixRemoveUnusedResExtension
import com.tencent.matrix.shrinker.RemoveUnusedResourceHelper
import com.tencent.mm.arscutil.ArscUtil
import com.tencent.mm.arscutil.io.ArscReader
import com.tencent.mm.arscutil.io.ArscWriter
import org.gradle.api.Action
import org.gradle.api.DefaultTask
import org.gradle.api.GradleException
import org.gradle.api.tasks.TaskAction
import java.io.File
import java.util.zip.ZipFile
import java.util.zip.ZipOutputStream

abstract class RemoveUnusedResourcesTask : DefaultTask() {

    companion object {
        const val TAG = "Matrix.RemoveUnusedResourcesTask"
    }

    private lateinit var variant: BaseVariant

    private var needSign: Boolean = false

    private var shrinkArsc: Boolean = false

    private var apksigner: String = ""

    private var ignoreRes: Set<String> = HashSet()

    private var unusedResources: MutableSet<String> = HashSet()

    private val ignoreResources = HashSet<String>()

    @TaskAction
    fun removeResources() {

        val symbolDirName = AgpCompat.getIntermediatesSymbolDirName()
        val signingConfig = AgpCompat.getSigningConfig(variant)

        val rTxtPath = "${project.buildDir.absolutePath}/intermediates/${symbolDirName}/${variant.name}/R.txt"

        variant.outputs.forEach { output ->
            val unsignedApkPath = output.outputFile.absolutePath
            val startTime = System.currentTimeMillis()
            removeUnusedResources(unsignedApkPath, rTxtPath, signingConfig)
            Log.i(TAG, "cost time %f s", (System.currentTimeMillis() - startTime) / 1000.0f)
        }
    }

    class CreationAction(
            private val creationConfig: CreationConfig,
            private val removeUnusedResources: MatrixRemoveUnusedResExtension
    ) : Action<RemoveUnusedResourcesTask>, BaseCreationAction<RemoveUnusedResourcesTask>(creationConfig) {

        override val name = computeTaskName("remove", "UnusedResources")
        override val type = RemoveUnusedResourcesTask::class.java

        override fun execute(task: RemoveUnusedResourcesTask) {

            task.needSign = removeUnusedResources.needSign
            task.shrinkArsc = removeUnusedResources.shrinkArsc
            task.apksigner = removeUnusedResources.apksignerPath
            task.ignoreRes = removeUnusedResources.ignoreResources
            task.unusedResources = removeUnusedResources.unusedResources

            task.variant = creationConfig.variant
        }
    }

    private fun ignoreResource(name: String): Boolean {
        for (path in ignoreResources) {
            if (name.matches(path.toRegex())) {
                return true
            }
        }
        return false
    }

    private fun entryToResouceName(entry: String): String {
        var resourceName = ""
        if (!Util.isNullOrNil(entry)) {
            var typeName = entry.substring(4, entry.lastIndexOf('/'));
            var resName = entry.substring(entry.lastIndexOf('/') + 1, entry.indexOf('.'))
            if (!Util.isNullOrNil(typeName) && !Util.isNullOrNil(resName)) {
                val index = typeName.indexOf('-')
                if (index >= 0) {
                    typeName = typeName.substring(0, index)
                }
                resourceName = "R.$typeName.$resName"
            }
        }
        return resourceName
    }

    private fun removeUnusedResources(
            originalApk: String,
            rTxtFile: String,
            signingConfig: SigningConfig?) {

        // Check configurations
        if (needSign) {
            if (Util.isNullOrNil(apksigner)) {
                throw GradleException("need sign apk but apksigner not found!");
            } else if (!File(apksigner).exists()) {
                throw GradleException("need sign apk but apksigner $apksigner was not exist!");
            } else if (signingConfig == null) {
                throw GradleException("need sign apk but signingConfig not found!");
            }
        }

        var zipOutputStream: ZipOutputStream? = null

        try {
            try {
                val inputFile = File(originalApk)

                for (res in ignoreRes) {
                    ignoreResources.add(Util.globToRegexp(res))
                }

                val iterator = unusedResources.iterator()
                while (iterator.hasNext()) {
                    val res = iterator.next()
                    if (ignoreResource(res)) {
                        iterator.remove()
                        Log.i(TAG, "ignore unused resources %s", res)
                    }
                }
                Log.i(TAG, "unused resources count:%d", unusedResources.size)

                val outputApk = inputFile.parentFile.absolutePath + "/" + inputFile.name.substring(0, inputFile.name.indexOf('.')) + "_shrinked.apk"
                val outputFile = File(outputApk)
                if (outputFile.exists()) {
                    Log.w(TAG, "output apk file %s is already exists! It will be deleted anyway!", outputApk)
                    outputFile.delete()
                    outputFile.createNewFile()
                }

                val zipInputFile = ZipFile(inputFile)

                zipOutputStream = ZipOutputStream(outputFile.outputStream())

                val resourceMap = HashMap<String, Int>()
                val styleableMap = HashMap<String, Array<Pair<String, Int>>>()
                val resTxtFile = File(rTxtFile)
                RemoveUnusedResourceHelper.readResourceTxtFile(resTxtFile, resourceMap, styleableMap)

                val removeResources = HashMap<String, Int>()
                for (resName in unusedResources) {
                    if (!ignoreResource(resName)) {
                        val removed = resourceMap.remove(resName)
                        if (removed != null) {
                            removeResources[resName] = removed
                        }
                    }
                }

                for (zipEntry in zipInputFile.entries()) {
                    if (zipEntry.name.startsWith("res/")) {
                        val resourceName = entryToResouceName(zipEntry.name)
                        if (!Util.isNullOrNil(resourceName)) {
                            if (removeResources.containsKey(resourceName)) {
                                Log.i(TAG, "remove unused resource %s", resourceName)
                                continue
                            } else {
                                RemoveUnusedResourceHelper.addZipEntry(zipOutputStream, zipEntry, zipInputFile);
                            }
                        } else {
                            RemoveUnusedResourceHelper.addZipEntry(zipOutputStream, zipEntry, zipInputFile);
                        }
                    } else {
                        if (needSign && zipEntry.name.startsWith("META-INF/")) {
                            continue
                        } else {
                            if (shrinkArsc && zipEntry.name.equals("resources.arsc", true) && unusedResources.size > 0) {
                                val srcArscFile = File(inputFile.parentFile.absolutePath + "/resources.arsc");
                                val destArscFile = File(inputFile.parentFile.absolutePath + "/resources_shrinked.arsc")
                                if (srcArscFile.exists()) {
                                    srcArscFile.delete()
                                    srcArscFile.createNewFile()
                                }
                                RemoveUnusedResourceHelper.unzipEntry(zipInputFile, zipEntry, srcArscFile)

                                val reader = ArscReader(srcArscFile.absolutePath)
                                val resTable = reader.readResourceTable()
                                for (resName in removeResources.keys) {
                                    ArscUtil.removeResource(resTable, removeResources[resName]!!, resName)
                                }
                                val writer = ArscWriter(destArscFile.absolutePath)
                                writer.writeResTable(resTable)
                                Log.i(TAG, "Shrink resources.arsc size %f KB", (srcArscFile.length() - destArscFile.length()) / 1024.0)
                                RemoveUnusedResourceHelper.addZipEntry(zipOutputStream, zipEntry, destArscFile)
                            } else {
                                RemoveUnusedResourceHelper.addZipEntry(zipOutputStream, zipEntry, zipInputFile);
                            }
                        }
                    }
                }

                zipOutputStream.close()

                Log.i(TAG, "shrink apk size %f KB", (inputFile.length() - outputFile.length()) / 1024.0)
                if (needSign) {
                    Log.i(TAG, "Sign apk...")
                    val processBuilder = ProcessBuilder()
                    processBuilder.command(apksigner, "sign", "-v",
                            "--ks", signingConfig!!.storeFile?.absolutePath,
                            "--ks-pass", "pass:" + signingConfig.storePassword,
                            "--key-pass", "pass:" + signingConfig.keyPassword,
                            "--ks-key-alias", signingConfig.keyAlias,
                            outputFile.absolutePath)
                    val process = processBuilder.start()
                    process.waitFor()
                    if (process.exitValue() != 0) {
                        throw GradleException(process.errorStream.bufferedReader().readLines()
                                .joinTo(StringBuilder(), "\n").toString())
                    }
                }
                val backApk = inputFile.parentFile.absolutePath + "/" + inputFile.name.substring(0, inputFile.name.indexOf('.')) + "_back.apk"
                inputFile.renameTo(File(backApk))
                outputFile.renameTo(File(originalApk))

                //modify R.txt to delete the removed resources
                if (removeResources.isNotEmpty()) {
                    val styleableItera = styleableMap.keys.iterator()
                    while (styleableItera.hasNext()) {
                        val styleable = styleableItera.next()
                        val attrs = styleableMap[styleable]
                        var j = 0
                        for (i in 0 until (attrs!!.size)) {
                            j = i
                            if (!removeResources.containsValue(attrs[i].right)) {
                                break
                            }
                        }
                        if (attrs.size > 0 && j == attrs.size) {
                            Log.i(TAG, "removed styleable $styleable")
                            styleableItera.remove()
                        }
                    }
                    val newResTxtFile = resTxtFile.parentFile.absolutePath + "/" + resTxtFile.name.substring(0, resTxtFile.name.indexOf('.')) + "_shrinked.txt"
                    RemoveUnusedResourceHelper.shrinkResourceTxtFile(newResTxtFile, resourceMap, styleableMap)

                    //Other plugins such as "Tinker" may depend on the R.txt file, so we should not modify R.txt directly .
                    //new File(newResTxtFile).renameTo(resTxtFile);
                }

            } finally {
                zipOutputStream?.close()
            }
        } catch (e: Exception) {
            Log.printErrStackTrace(TAG, e, "remove unused resources occur error!")
        }
    }

}
