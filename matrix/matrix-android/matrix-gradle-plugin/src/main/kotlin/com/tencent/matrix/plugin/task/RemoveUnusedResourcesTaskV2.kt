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
import com.android.utils.FileUtils
import com.google.gson.Gson
import com.google.gson.JsonArray
import com.tencent.matrix.javalib.util.Log
import com.tencent.matrix.javalib.util.Pair
import com.tencent.matrix.javalib.util.Util
import com.tencent.matrix.plugin.compat.AgpCompat
import com.tencent.matrix.plugin.compat.CreationConfig
import com.tencent.matrix.plugin.extension.MatrixRemoveUnusedResExtension
import com.tencent.matrix.resguard.ResguardMapping
import com.tencent.matrix.shrinker.ApkUtil
import com.tencent.matrix.shrinker.ProguardStringBuilder
import com.tencent.mm.arscutil.ArscUtil
import com.tencent.mm.arscutil.io.ArscReader
import com.tencent.mm.arscutil.io.ArscWriter
import org.gradle.api.Action
import org.gradle.api.DefaultTask
import org.gradle.api.GradleException
import org.gradle.api.Project
import org.gradle.api.tasks.TaskAction
import java.io.File
import java.io.FileOutputStream
import java.io.PrintWriter
import java.util.zip.ZipEntry
import java.util.zip.ZipFile
import java.util.zip.ZipOutputStream

abstract class RemoveUnusedResourcesTaskV2 : DefaultTask() {

    companion object {
        const val TAG = "Matrix.RemoveUnusedResourcesTaskV2"

        const val ARSC_FILE_NAME = "resources.arsc"
        const val RES_DIR_PROGUARD_NAME = "r"
        const val RES_PATH_MAPPING_PREFIX = "res path mapping:"
        const val RES_ID_MAPPING_PREFIX = "res id mapping:"
        const val RES_FILE_MAPPING_PREFIX = "res file mapping:"

        const val BACKUP_DIR_NAME = "shrinkResourceBackup"
        const val SHRUNK_R_TXT_FILE = "R_shrinked.txt"
        const val RES_GUARD_MAPPING_FILE_NAME = "resguard-mapping.txt"
    }

    private lateinit var variant: BaseVariant

    private var isDeleteDuplicatesEnabled: Boolean = false
    private var isShrinkArscEnabled: Boolean = false
    private var isSigningEnabled: Boolean = false
    private var isZipAlignEnabled: Boolean = false
    private var is7zipEnabled: Boolean = false
    private var isResGuardEnabled: Boolean = false

    private var obfuscatedResourcesDirectoryName: String? = null
    private var overrideInputApkFiles: List<File> = emptyList()
    private var resguardMappingFile: File? = null
    private var resguardMapping: ResguardMapping? = null

    private lateinit var pathOfApkChecker: String
    private lateinit var pathOfApkSigner: String
    private lateinit var pathOfZipAlign: String
    private lateinit var pathOfSevenZip: String

    private lateinit var rulesOfIgnore: Set<String>

    fun overrideInputApkFiles(inputApkFiles: List<String>) {
        this.overrideInputApkFiles = inputApkFiles.map { File(it) }
    }

    fun withResguardMapping(path: String) {
        resguardMappingFile = File(path)
        resguardMapping = ResguardMapping(resguardMappingFile!!)
    }

    @TaskAction
    fun removeResources() {
        overrideInputApkFiles
            .ifEmpty {
                variant.outputs.map { it.outputFile }
            }
            .forEach { apk ->
                val startTime = System.currentTimeMillis()
                removeResourcesV2(
                    project = project,
                    originalApkFile = apk,
                    signingConfig = AgpCompat.getSigningConfig(variant),
                    nameOfSymbolDirectory = AgpCompat.getIntermediatesSymbolDirName()
                )
                Log.i(TAG, "cost time %f s", (System.currentTimeMillis() - startTime) / 1000.0f)
            }
    }

    class CreationAction(
            private val creationConfig: CreationConfig,
            private val removeUnusedResources: MatrixRemoveUnusedResExtension
    ) : Action<RemoveUnusedResourcesTaskV2>, BaseCreationAction<RemoveUnusedResourcesTaskV2>(creationConfig) {

        override val name = computeTaskName("remove", "UnusedResourcesV2")
        override val type = RemoveUnusedResourcesTaskV2::class.java

        override fun execute(task: RemoveUnusedResourcesTaskV2) {

            task.variant = creationConfig.variant

            with(removeUnusedResources) {

                task.isDeleteDuplicatesEnabled = shrinkDuplicates
                task.isShrinkArscEnabled = shrinkArsc
                task.isSigningEnabled = needSign
                task.isZipAlignEnabled = zipAlign
                task.is7zipEnabled = use7zip
                task.isResGuardEnabled = embedResGuard

                task.obfuscatedResourcesDirectoryName = obfuscatedResourcesDirectoryName?.let {
                    if (it.endsWith("/")) it else "${it}/"
                }

                task.pathOfApkChecker = apkCheckerPath
                task.pathOfApkSigner = apksignerPath
                task.pathOfZipAlign = zipAlignPath
                task.pathOfSevenZip = sevenZipPath

                task.rulesOfIgnore = ignoreResources
            }

            task.parametersInvalidation()

        }
    }

    private fun checkIfIgnored(nameOfRes: String, ignored: Set<String>): Boolean {
        for (path in ignored) {
            if (nameOfRes.matches(path.toRegex())) {
                return true
            }
        }
        return false
    }

    private fun removeUnusedResources(
            fromOriginalApkFile: File,
            toShrunkApkFile: File,
            setOfUnusedResources: Set<String>,
            mapOfResources: Map<String, Int>,
            mapOfObfuscatedNames: Map<String, String>,
            mapOfResourcesGonnaRemoved: Map<String, Int>,
            mapOfDuplicatesReplacements: MutableMap<String, String>,
            resultOfObfuscatedDirs: MutableMap<String, String>,
            resultOfObfuscatedFiles: MutableMap<String, String>
    ): Boolean {

        try {
            var zipOutputStream: ZipOutputStream? = null
            try {

                val rmUnused = setOfUnusedResources.isNotEmpty()
                val rmDuplicated = mapOfDuplicatesReplacements.isNotEmpty()

                Log.i(TAG, "rmUnsed %s, rmDuplicated %s, isResguardEnabled %s", rmUnused, rmDuplicated, isResGuardEnabled)

                if (!(rmUnused || rmDuplicated || isResGuardEnabled)) {
                    return false
                }

                val zipInputFile = ZipFile(fromOriginalApkFile)
                val arsc = zipInputFile.getEntry(ARSC_FILE_NAME)

                val unzipDir = File(fromOriginalApkFile.parentFile.canonicalPath + File.separator + fromOriginalApkFile.name.substring(0, fromOriginalApkFile.name.lastIndexOf(".")) + "_unzip")
                FileUtils.deleteRecursivelyIfExists(unzipDir)
                unzipDir.mkdir()

                val srcArscFile = File(unzipDir, ARSC_FILE_NAME)
                val destArscFile = File(unzipDir, "shrinked_$ARSC_FILE_NAME")

                ApkUtil.unzipEntry(zipInputFile, arsc, srcArscFile.canonicalPath)

                val reader = ArscReader(srcArscFile.canonicalPath)
                val resTable = reader.readResourceTable()

                //remove unused resources
                if (rmUnused && isShrinkArscEnabled) {
                    for (resName in mapOfResourcesGonnaRemoved.keys) {
                        val resourceId = mapOfResourcesGonnaRemoved[resName]
                        if (resourceId != null) {
                            ArscUtil.removeResource(resTable, resourceId, resName)
                        }
                    }
                }

                //remove duplicated resources
                if (rmDuplicated) {
                    val replaceIterator = mapOfDuplicatesReplacements.keys.iterator()
                    while (replaceIterator.hasNext()) {
                        val sourceFile = replaceIterator.next()
                        val sourceRes = ApkUtil.entryToResourceName(sourceFile, resguardMapping)
                        val sourceId = mapOfResources[sourceRes]!!
                        val targetFile = mapOfDuplicatesReplacements[sourceFile]
                        val targetRes = ApkUtil.entryToResourceName(targetFile, resguardMapping)
                        val targetId = mapOfResources[targetRes]!!
                        val success = ArscUtil.replaceFileResource(resTable, sourceId, sourceFile, targetId, targetFile)
                        if (!success) {
                            Log.w(TAG, "replace %s(%s) with %s(%s) failed!", sourceRes, sourceFile, targetRes, targetFile)
                            replaceIterator.remove()
                        }
                    }
                }

                //proguard resource name
                if (isResGuardEnabled) {
                    val resIterator = mapOfResources.keys.iterator()
                    val resIdProguard = HashMap<Int, String>()
                    while (resIterator.hasNext()) {
                        val resource = resIterator.next()
                        if (mapOfObfuscatedNames.containsKey(resource)) {
                            mapOfResources[resource]?.let { resIdProguard.put(it, ApkUtil.parseResourceName(mapOfObfuscatedNames[resource])) }
                        }
                    }
                    if (resIdProguard.isNotEmpty()) {
                        ArscUtil.replaceResEntryName(resTable, resIdProguard)
                    }
                }

                val dirProguard = ProguardStringBuilder()

                val dirFileProguard = HashMap<String, ProguardStringBuilder>()

                val compressedEntry = HashSet<String>()

                for (zipEntry in zipInputFile.entries()) {
                    if (zipEntry.isDirectory) continue

                    var destFile = unzipDir.canonicalPath + File.separator + zipEntry.name.replace('/', File.separatorChar)

                    if (zipEntry.name.startsWith(obfuscatedResourcesDirectoryName ?: "res/")) {
                        val resourceName = ApkUtil.entryToResourceName(zipEntry.name, resguardMapping)
                        if (!Util.isNullOrNil(resourceName)) {
                            if (mapOfResourcesGonnaRemoved.containsKey(resourceName)) {
                                Log.i(TAG, "remove unused resource %s file %s", resourceName, zipEntry.name)
                                continue
                            } else if (mapOfDuplicatesReplacements.containsKey(zipEntry.name)) {
                                Log.i(TAG, "remove duplicated resource file %s", zipEntry.name)
                                continue
                            } else {
                                if (arsc != null && isResGuardEnabled) {
                                    if (mapOfResources.containsKey(resourceName)) {
                                        val dir = zipEntry.name.substring(0, zipEntry.name.lastIndexOf("/"))
                                        val suffix = zipEntry.name.substring(zipEntry.name.indexOf("."))
                                        Log.d(TAG, "resource %s dir %s", resourceName, dir)
                                        if (!resultOfObfuscatedDirs.containsKey(dir)) {
                                            val proguardDir = dirProguard.generateNextProguardFileName()
                                            resultOfObfuscatedDirs[dir] = "$RES_DIR_PROGUARD_NAME/$proguardDir"
                                            dirFileProguard[dir] = ProguardStringBuilder()
                                            Log.i(TAG, "dir %s, proguard builder", dir)
                                        }
                                        resultOfObfuscatedFiles[zipEntry.name] = resultOfObfuscatedDirs[dir] + "/" + dirFileProguard[dir]!!.generateNextProguardFileName() + suffix
                                        val success = ArscUtil.replaceResFileName(resTable, mapOfResources[resourceName]!!, zipEntry.name, resultOfObfuscatedFiles[zipEntry.name])
                                        if (success) {
                                            destFile = unzipDir.canonicalPath + File.separator + resultOfObfuscatedFiles[zipEntry.name]!!.replace('/', File.separatorChar)
                                        }
                                    }
                                }
                                if (zipEntry.method == ZipEntry.DEFLATED) {
                                    compressedEntry.add(destFile)
                                }
                                Log.d(TAG, "unzip %s to file %s", zipEntry.name, destFile)
                                ApkUtil.unzipEntry(zipInputFile, zipEntry, destFile)
                            }
                        } else {
                            Log.w(TAG, "parse entry %s resource name failed!", zipEntry.name)
                        }
                    } else {
                        if (!zipEntry.name.startsWith("META-INF/") || (!zipEntry.name.endsWith(".SF") && !zipEntry.name.endsWith(".MF") && !zipEntry.name.endsWith(".RSA"))) {
                            if (zipEntry.method == ZipEntry.DEFLATED) {
                                compressedEntry.add(destFile)
                            }
                            if (zipEntry.name != ARSC_FILE_NAME) {                            // has already unzip resources.arsc before
                                Log.d(TAG, "unzip %s to file %s", zipEntry.name, destFile)
                                ApkUtil.unzipEntry(zipInputFile, zipEntry, destFile)
                            }
                        }
                    }
                }

                val writer = ArscWriter(destArscFile.canonicalPath)
                writer.writeResTable(resTable)
                Log.i(TAG, "shrink resources.arsc size %f KB", (srcArscFile.length() - destArscFile.length()) / 1024.0)
                srcArscFile.delete()
                FileUtils.copyFile(destArscFile, srcArscFile)
                destArscFile.delete()

                if (is7zipEnabled) {
                    if (Util.isNullOrNil(pathOfSevenZip)) {
                        throw GradleException("use 7zip, but 7zip not specified!")
                    } else if (!File(pathOfSevenZip).exists()) {
                        throw GradleException("use 7zip but the path $pathOfSevenZip is not exist!")
                    }
                    ApkUtil.sevenZipFile(pathOfSevenZip, unzipDir.canonicalPath + "${File.separator}*", toShrunkApkFile.canonicalPath, false)

                    if (compressedEntry.isNotEmpty()) {
                        Log.i(TAG, "7zip %d DEFLATED files to apk", compressedEntry.size)
                        val deflateDir = File(fromOriginalApkFile.parentFile, fromOriginalApkFile.name.substring(0, fromOriginalApkFile.name.lastIndexOf(".")) + "_deflated")
                        FileUtils.deleteRecursivelyIfExists(deflateDir)
                        deflateDir.mkdir()
                        for (compress in compressedEntry) {
                            val entry = compress.substring(unzipDir.canonicalPath.length + 1)
                            val deflateFile = File(deflateDir, entry)
                            deflateFile.parentFile.mkdirs()
                            deflateFile.createNewFile()
                            FileUtils.copyFile(File(compress), deflateFile)
                        }
                        ApkUtil.sevenZipFile(pathOfSevenZip, deflateDir.canonicalPath + "${File.separator}*", toShrunkApkFile.canonicalPath, true)
                        FileUtils.deleteRecursivelyIfExists(deflateDir)
                    }
                } else {
                    zipOutputStream = ZipOutputStream(FileOutputStream(toShrunkApkFile))
                    zipFile(zipOutputStream, unzipDir, unzipDir.canonicalPath, compressedEntry)
                }

                FileUtils.deleteRecursivelyIfExists(unzipDir)

                Log.i(TAG, "shrink apk size %f KB", (fromOriginalApkFile.length() - toShrunkApkFile.length()) / 1024.0)
                return true

            } finally {
                zipOutputStream?.close()
            }
        } catch (e: Exception) {
            Log.printErrStackTrace(TAG, e, "remove unused resources occur error!")
            return false
        }
    }

    private fun zipFile(zipOutputStream: ZipOutputStream, file: File, rootDir: String, compressedEntry: Set<String>) {
        if (file.isDirectory) {
            val unZipFiles = file.listFiles()
            for (subFile in unZipFiles) {
                zipFile(zipOutputStream, subFile, rootDir, compressedEntry)
            }
        } else {
            val zipEntry = ZipEntry(file.canonicalPath.substring(rootDir.length + 1))
            val method = if (compressedEntry.contains(file.canonicalPath)) ZipEntry.DEFLATED else ZipEntry.STORED
            Log.i(TAG, "zip file %s -> entry %s, DEFLATED %s", file.canonicalPath, zipEntry.name, method == ZipEntry.DEFLATED)
            zipEntry.method = method
            ApkUtil.addZipEntry(zipOutputStream, zipEntry, file)
        }
    }

    class FindOutUnusedResources(
            private val project: Project,
            private val pathOfApkChecker: String,
            private val configOfIgnoredResources: Set<String>,
            private val findOutDuplicates: Boolean
    ) {
        fun exec(
                pathOfOriginalApk: String,
                pathOfMapping: String,
                pathOfRTxt: String,
                resguardMappingFile: File?,
                resultOfUnused: MutableSet<String>,
                resultOfDuplicates: MutableMap<String, MutableSet<String>>
        ) {

            val pathOfBuildDir = project.buildDir.canonicalPath

            val pathOfOutputWithoutSuffix = File(pathOfBuildDir, "apk_checker").absolutePath

            val parameters = ArrayList<String>()
            parameters.add("java")
            parameters.add("-jar")
            parameters.add(pathOfApkChecker)
            parameters.add("--apk")
            parameters.add(pathOfOriginalApk)
            parameters.add("--output")
            parameters.add(pathOfOutputWithoutSuffix)

            if (File(pathOfMapping).exists()) {
                parameters.add("--mappingTxt")
                parameters.add(pathOfMapping)
            }

            if (resguardMappingFile?.exists() == true) {
                parameters.add("--resMappingTxt")
                parameters.add(resguardMappingFile.absolutePath)
            }

            parameters.add("--format")
            parameters.add("json")
            parameters.add("-unusedResources")
            parameters.add("--rTxt")
            parameters.add(pathOfRTxt)

            val parametersOfIgnoredResources = StringBuilder()
            if (configOfIgnoredResources.isNotEmpty()) {
                for (ignore in configOfIgnoredResources) {
                    parametersOfIgnoredResources.append(ignore)
                    parametersOfIgnoredResources.append(',')
                }
                parametersOfIgnoredResources.deleteCharAt(parametersOfIgnoredResources.length - 1)
                parameters.add("--ignoreResources")
                parameters.add(parametersOfIgnoredResources.toString())
            }

            if (findOutDuplicates) {
                parameters.add("-duplicatedFile")
            }

            val process = ProcessBuilder().command(parameters).start()
            ApkUtil.waitForProcessOutput(process)
            if (process.exitValue() != 0) {
                throw GradleException(process.errorStream.bufferedReader().readLines()
                        .joinTo(StringBuilder(), "\n").toString())
            }

            val checkerOutputFile = File(pathOfBuildDir, "apk_checker.json")   // -_-|||
            if (checkerOutputFile.exists()) {
                val jsonArray = Gson().fromJson(checkerOutputFile.readText(), JsonArray::class.java)
                for (i in 0 until jsonArray.size()) {
                    if (jsonArray.get(i).asJsonObject.get("taskType").asInt == 12) {
                        val resList = jsonArray.get(i).asJsonObject.get("unused-resources").asJsonArray
                        for (j in 0 until resList.size()) {
                            resultOfUnused.add(resList.get(j).asString)
                        }
                    }
                    if (jsonArray.get(i).asJsonObject.get("taskType").asInt == 10) {
                        val duplicatedFiles = jsonArray.get(i).asJsonObject.get("files").asJsonArray
                        for (k in 0 until duplicatedFiles.size()) {
                            val obj = duplicatedFiles.get(k).asJsonObject
                            val md5 = obj.get("md5")
                            resultOfDuplicates[md5.toString()] = HashSet()
                            val fileList = obj.get("files").asJsonArray
                            for (m in 0 until fileList.size()) {
                                resultOfDuplicates[md5.toString()]?.add(fileList.get(m).asString)
                            }
                        }
                    }
                }
            }
        }
    }

    private fun parametersInvalidation() {

        // Validate path of ApkChecker tool
        if (!Util.isNullOrNil(pathOfApkChecker)) {
            if (!File(pathOfApkChecker).exists()) {
                throw GradleException("the path of Matrix-ApkChecker $pathOfApkChecker is not exist!")
            }
        } else {
            throw GradleException("the path of Matrix-ApkChecker not found!")
        }

        // Validate path of Zipalign tool
        if (isZipAlignEnabled) {
            if (Util.isNullOrNil(pathOfZipAlign)) {
                throw GradleException("Need zip align apk but the path of zipalign is not specified!")
            } else if (!File(pathOfZipAlign).exists()) {
                throw GradleException("Need zipalign apk but $pathOfZipAlign is not exist!")
            }
        }

        // Validate path of signing
        if (isSigningEnabled) {
            if (Util.isNullOrNil(pathOfApkSigner)) {
                throw GradleException("Need signing apk but the path of apksigner is not specified!")
            } else if (!File(pathOfApkSigner).exists()) {
                throw GradleException("Need signing apk but $pathOfApkSigner is not exist!")
            } else if (AgpCompat.getSigningConfig(variant) == null) {
                throw GradleException("Need signing apk but signingConfig is not specified!")
            }
        }
    }

    private fun calculateResources(
            setOfUnusedResources: Set<String>,
            mapOfDuplicatedResources: Map<String, MutableSet<String>>,
            mapOfResources: MutableMap<String, Int>,
            resultOfResourcesGonnaRemoved: MutableMap<String, Int>,
            resultOfDuplicatesReplacements: MutableMap<String, String>,
            resultOfObfuscatedNames: MutableMap<String, String>
    ) {

        // Prepare unused resources
        val regexpRules = HashSet<String>()
        for (res in rulesOfIgnore) {
            regexpRules.add(Util.globToRegexp(res))
        }

        for (resName in setOfUnusedResources) {
            if (!checkIfIgnored(resName, regexpRules)) {
                val resId = mapOfResources.remove(resName)
                if (resId != null) {
                    resultOfResourcesGonnaRemoved[resName] = resId
                }
            } else {
                Log.i(TAG, "ignore remove unused resources %s", resName)
            }
        }

        Log.i(TAG, "unused resources count:%d", setOfUnusedResources.size)

        // Prepare duplicated resources
        for (md5 in mapOfDuplicatedResources.keys) {
            val duplicatesEntries = mapOfDuplicatedResources[md5]
            val duplicatesNames = HashMap<String, String>()
            if (duplicatesEntries != null) {
                for (entry in duplicatesEntries) {
                    if (!entry.startsWith(obfuscatedResourcesDirectoryName ?: "res/")) {
                        Log.w(TAG, "   %s is not resource file!", entry)
                        continue
                    } else {
                        duplicatesNames[entry] = ApkUtil.entryToResourceName(entry, resguardMapping)
                    }
                }
            }

            if (duplicatesNames.size > 0) {
                if (!ApkUtil.isSameResourceType(duplicatesNames.keys, obfuscatedResourcesDirectoryName)) {
                    Log.w(TAG, "the type of duplicated resources %s are not same!", duplicatesEntries)
                    continue
                } else {
                    var it = duplicatesNames.keys.iterator()
                    while (it.hasNext()) {
                        val entry = it.next()
                        if (!mapOfResources.containsKey(duplicatesNames[entry])) {
                            Log.w(TAG, "can not find duplicated resources %s!", entry)
                            it.remove()
                        }
                    }

                    if (duplicatesNames.size > 1) {
                        it = duplicatesNames.keys.iterator()
                        val replace = it.next()
                        while (it.hasNext()) {
                            val dup = it.next()
                            Log.i(TAG, "replace %s with %s", dup, replace)
                            resultOfDuplicatesReplacements[dup] = replace
                        }
                    }
                }
            }
        }

        // Prepare proguard resource name
        val mapOfResTypeName = HashMap<String, HashSet<String>>()
        for (name in mapOfResources.keys) {
            val type = ApkUtil.parseResourceType(name)
            if (!mapOfResTypeName.containsKey(type)) {
                mapOfResTypeName[type] = HashSet()
            }
            mapOfResTypeName[type]!!.add(name)
        }

        for (resType in mapOfResTypeName.keys) {

            val resTypeBuilder = ProguardStringBuilder()
            val resNames = mapOfResTypeName[resType]

            if (resNames != null) {
                for (resName in resNames) {
                    if (!checkIfIgnored(resName, regexpRules)) {
                        resultOfObfuscatedNames[resName] = "R." + resType + "." + resTypeBuilder.generateNextProguard()
                    } else {
                        Log.i(TAG, "ignore proguard resource name %s", resName)
                    }
                }
            }
        }

    }

    private fun saveShrunkRTxt(
            pathOfBackup: String,
            mapOfResources: Map<String, Int>,
            mapOfResourcesGonnaRemoved: Map<String, Int>,
            mapOfStyleables: MutableMap<String, Array<Pair<String, Int>>>
    ) {

        // Modify R.txt to delete the removed resources
        if (mapOfResourcesGonnaRemoved.isNotEmpty()) {
            val styleableIterator = mapOfStyleables.keys.iterator()
            while (styleableIterator.hasNext()) {
                val styleable = styleableIterator.next()
                val attrs = mapOfStyleables[styleable]
                var j = 0
                if (attrs != null) {
                    for (i in 0 until attrs.size) {
                        j = i
                        if (!mapOfResourcesGonnaRemoved.containsValue(attrs[i].right)) {
                            break
                        }
                    }
                    if (attrs.isNotEmpty() && j == attrs.size) {
                        Log.i(TAG, "removed styleable $styleable")
                        styleableIterator.remove()
                    }
                }
            }
            val newResTxtFile = pathOfBackup + File.separator + SHRUNK_R_TXT_FILE
            ApkUtil.shrinkResourceTxtFile(newResTxtFile, mapOfResources, mapOfStyleables)

            //Other plugins such as "Tinker" may depend on the R.txt file, so we should not modify R.txt directly .
            //new File(newResTxtFile).renameTo(resTxtFile)
        }
    }

    private fun writeResGuardMappingFile(
            pathOfBackup: String,
            mapOfObfuscatedNames: HashMap<String, String>,
            mapOfObfuscatedDirs: HashMap<String, String>,
            mapOfObfuscatedFiles: HashMap<String, String>
    ) {

        val resGuardMappingFile = File(pathOfBackup, RES_GUARD_MAPPING_FILE_NAME)
        resGuardMappingFile.createNewFile()
        val fileWriter = PrintWriter(resGuardMappingFile)
        fileWriter.println(RES_PATH_MAPPING_PREFIX)
        if (mapOfObfuscatedDirs.isNotEmpty()) {
            for (srcDir in mapOfObfuscatedDirs.keys) {
                fileWriter.println("    " + srcDir + " -> " + mapOfObfuscatedDirs[srcDir])
            }
        }
        fileWriter.println(RES_ID_MAPPING_PREFIX)
        if (mapOfObfuscatedNames.isNotEmpty()) {
            for (srcRes in mapOfObfuscatedNames.keys) {
                fileWriter.println("    " + srcRes + " -> " + mapOfObfuscatedNames[srcRes])
            }
        }
        fileWriter.println(RES_FILE_MAPPING_PREFIX)
        if (mapOfObfuscatedFiles.isNotEmpty()) {
            for (srcFile in mapOfObfuscatedFiles.keys) {
                fileWriter.println("    " + srcFile + " -> " + mapOfObfuscatedFiles[srcFile])
            }
        }
        fileWriter.close()
    }

    private fun removeResourcesV2(
            project: Project,
            originalApkFile: File,
            signingConfig: SigningConfig?,
            nameOfSymbolDirectory: String
    ) {

        // Prepare backup dir for un-shrunk apk
        val fileOfBackup = File(project.buildDir, "outputs")
                .resolve(BACKUP_DIR_NAME)
                .resolve(originalApkFile.name.substring(0, originalApkFile.name.indexOf(".")))

        fileOfBackup.mkdirs()

        val originalApkPath = originalApkFile.canonicalPath
        Log.i(TAG, "original apk file %s", originalApkPath)

        var startTime = System.currentTimeMillis()

        val rTxtFile = File(project.buildDir, "intermediates")
            .resolve(nameOfSymbolDirectory)
            .resolve(variant.name)
            .resolve("R.txt")

        val mappingTxtFile = File(project.buildDir, "outputs")
                .resolve("mapping")
                .resolve(variant.name)
                .resolve("mapping.txt")

        // Find out unused resources
        var setOfUnusedResources: MutableSet<String> = HashSet()
        val mapOfDuplicatedResources: MutableMap<String, MutableSet<String>> = HashMap()
        FindOutUnusedResources(
                project = project,
                pathOfApkChecker = pathOfApkChecker,
                configOfIgnoredResources = rulesOfIgnore,
                findOutDuplicates = isDeleteDuplicatesEnabled
        ).exec(
                pathOfOriginalApk = originalApkPath,
                pathOfMapping = mappingTxtFile.absolutePath,
                pathOfRTxt = rTxtFile.absolutePath,
                resguardMappingFile = resguardMappingFile,

                // result
                resultOfUnused = setOfUnusedResources,
                resultOfDuplicates = mapOfDuplicatedResources
        )

        Log.i(TAG, "find out %d unused resources:\n %s", setOfUnusedResources.size, setOfUnusedResources)
        Log.i(TAG, "find out %d duplicated resources:", mapOfDuplicatedResources.size)
        for (md5 in mapOfDuplicatedResources.keys) {
            Log.i(TAG, "> md5:%s, files:%s", md5, mapOfDuplicatedResources[md5])
        }
        Log.i(TAG, "find unused resources cost time %fs ", (System.currentTimeMillis() - startTime) / 1000.0f)

        // Map of resource names parsed from R.txt
        val mapOfResources: MutableMap<String, Int> = HashMap()
        // Map of styleables parsed from R.txt
        val mapOfStyleables: MutableMap<String, Array<Pair<String, Int>>> = HashMap()

        // Parse R.txt
        ApkUtil.readResourceTxtFile(rTxtFile, mapOfResources, mapOfStyleables)

        // Compute resource to be removed, replaced and obfuscated
        val mapOfResourcesGonnaRemoved: MutableMap<String, Int> = HashMap()
        val mapOfDuplicatesReplacements: MutableMap<String, String> = HashMap()
        val mapOfObfuscatedNames = HashMap<String, String>()
        calculateResources(
                setOfUnusedResources = setOfUnusedResources,
                mapOfDuplicatedResources = mapOfDuplicatedResources,
                mapOfResources = mapOfResources,

                // result
                resultOfResourcesGonnaRemoved = mapOfResourcesGonnaRemoved,
                resultOfDuplicatesReplacements = mapOfDuplicatesReplacements,
                resultOfObfuscatedNames = mapOfObfuscatedNames
        )

        // Remove output apk file, no incremental
        val shrunkApkPath = originalApkFile.parentFile.canonicalPath + File.separator + originalApkFile.name.substring(0, originalApkFile.name.indexOf(".")) + "_shrinked.apk"
        val shrunkApkFile = File(shrunkApkPath)
        if (shrunkApkFile.exists()) {
            Log.w(TAG, "output apk file %s is already exists! It will be deleted anyway!", shrunkApkPath)
            shrunkApkFile.delete()
        }

        // Remove unused resources.
        startTime = System.currentTimeMillis()
        val mapOfObfuscatedDirs = HashMap<String, String>()
        val mapOfObfuscatedFiles = HashMap<String, String>()
        val removeSucceed = removeUnusedResources(
                fromOriginalApkFile = originalApkFile,
                toShrunkApkFile = shrunkApkFile,
                setOfUnusedResources = setOfUnusedResources,
                mapOfResources = mapOfResources,
                mapOfObfuscatedNames = mapOfObfuscatedNames,
                mapOfResourcesGonnaRemoved = mapOfResourcesGonnaRemoved,
                mapOfDuplicatesReplacements = mapOfDuplicatesReplacements,

                // result
                resultOfObfuscatedDirs = mapOfObfuscatedDirs,
                resultOfObfuscatedFiles = mapOfObfuscatedFiles
        )
        Log.i(TAG, "remove duplicated resources & unused resources success? %s, cost time %f s", removeSucceed, (System.currentTimeMillis() - startTime) / 1000.0f)

        if (removeSucceed) {

            // Zip align
            if (isZipAlignEnabled) {
                val alignedApk = originalApkFile.parentFile.canonicalPath + File.separator + originalApkFile.name.substring(0, originalApkFile.name.indexOf(".")) + "_aligned.apk"
                Log.i(TAG, "Zipalign apk...")
                ApkUtil.zipAlignApk(shrunkApkPath, alignedApk, pathOfZipAlign)
                shrunkApkFile.delete()
                FileUtils.copyFile(File(alignedApk), shrunkApkFile)
                File(alignedApk).delete()
            }

            // Signing apk
            if (isSigningEnabled) {
                Log.i(TAG, "Signing apk...")
                ApkUtil.signApk(shrunkApkPath, pathOfApkSigner, signingConfig)
            }

            // Backup original apk and swap shrunk apk
            FileUtils.copyFile(originalApkFile, File(fileOfBackup, "backup.apk"))
            originalApkFile.delete()
            FileUtils.copyFile(shrunkApkFile, originalApkFile)
            shrunkApkFile.delete()

            // Save shrunk R.txt
            saveShrunkRTxt(
                    pathOfBackup = fileOfBackup.absolutePath,
                    mapOfResources = mapOfResources,
                    mapOfStyleables = mapOfStyleables,
                    mapOfResourcesGonnaRemoved = mapOfResourcesGonnaRemoved
            )

            // Write resource mapping to resguard-mapping.txt
            if (isResGuardEnabled) {
                writeResGuardMappingFile(
                        pathOfBackup = fileOfBackup.absolutePath,
                        mapOfObfuscatedNames = mapOfObfuscatedNames,
                        mapOfObfuscatedDirs = mapOfObfuscatedDirs,
                        mapOfObfuscatedFiles = mapOfObfuscatedFiles
                )
            }
        }
    }
}
