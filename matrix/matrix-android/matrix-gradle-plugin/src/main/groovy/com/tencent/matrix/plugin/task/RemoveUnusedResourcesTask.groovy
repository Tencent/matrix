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

import com.google.gson.Gson
import com.google.gson.JsonArray
import com.google.gson.JsonObject
import com.tencent.matrix.javalib.util.Log
import com.tencent.matrix.javalib.util.Util
import com.tencent.matrix.plugin.util.ApkUtil
import com.tencent.matrix.plugin.util.ProguardStringBuilder
import com.tencent.mm.arscutil.ArscUtil
import com.tencent.mm.arscutil.data.ResTable
import com.tencent.mm.arscutil.io.ArscReader
import com.tencent.mm.arscutil.io.ArscWriter
import org.apache.commons.io.FileUtils
import org.gradle.api.DefaultTask
import org.gradle.api.GradleException
import org.gradle.api.tasks.TaskAction
import org.gradle.internal.Pair

import java.util.zip.ZipEntry
import java.util.zip.ZipFile
import java.util.zip.ZipOutputStream

/**
 * Created by jinqiuchen on 17/12/26.
 */

public class RemoveUnusedResourcesTask extends DefaultTask {

    static final String TAG = "Matrix.MatrixPlugin.RemoveUnusedResourcesTask"

    public static final String BUILD_VARIANT = "build_variant"

    public Set<String> ignoreResources = new HashSet<>()

    Map<String, Integer> resourceMap = new HashMap()
    File resTxtFile
    Map<String, Pair<String, Integer>[]> styleableMap = new HashMap()
    Set<String> unusedResources = new HashSet<String>()
    Map<String, Set<String>> dupResources = new HashMap()
    Map<String, Integer> removeResources = new HashMap<>()
    Map<String, String> dupReplaceResources = new HashMap<>()
    Map<String, String> resNameProguard = new HashMap<>()
    Map<String, String> resDirProguard = new HashMap<>()
    Map<String, String> resFileProguard = new HashMap<>()

    public static final ARSC_FILE_NAME = "resources.arsc"
    public static final RES_DIR_PROGUARD_NAME = "r"
    public static final RES_PATH_MAPPING_PREFIX = "res path mapping:"
    public static final RES_ID_MAPPING_PREFIX = "res id mapping:"
    public static final RES_FILE_MAPPING_PREFIX = "res file mapping:"

    public static final BACKUP_DIR_NAME = "shrinkResourceBackup"
    public static final SHRINKED_R_TXT_FILE = "R_shrinked.txt"
    public static final RESGUARD_MAPPING_FILE_NAME = "resguard-mapping.txt"

    RemoveUnusedResourcesTask() {
    }

    boolean ignoreResource(String name) {
        for (String path : ignoreResources) {
            if (name.matches(path)) {
                return true
            }
        }
        return false
    }

    @TaskAction
    void removeResources() {

        String variantName = this.inputs.properties.get(BUILD_VARIANT)
        Log.i(TAG, "variant %s, removeResources", variantName)

        String backupPath = project.getBuildDir().getAbsolutePath() + "${File.separator}outputs${File.separator}${BACKUP_DIR_NAME}${File.separator}"
        File backupDir = new File(backupPath)
        backupDir.mkdir()

        project.extensions.android.applicationVariants.all { variant ->
            if (variant.name.equalsIgnoreCase(variantName)) {
                variant.outputs.forEach { output ->

                    String backupOutputPath = backupPath + output.outputFile.name.substring(0, output.outputFile.name.indexOf('.'))

                    String originalApk = output.outputFile.getAbsolutePath()
                    Log.i(TAG, "original apk file %s", originalApk)
                    long startTime = System.currentTimeMillis()
                    String rTxtFile = project.getBuildDir().getAbsolutePath() + "${File.separator}intermediates${File.separator}symbols${File.separator}${variant.name}${File.separator}R.txt"
                    String mappingTxtFile = project.getBuildDir().getAbsolutePath() + "${File.separator}outputs${File.separator}mapping${File.separator}${variant.name}${File.separator}mapping.txt"
                    findUnusedResources(originalApk, rTxtFile, mappingTxtFile, unusedResources, dupResources)
                    Log.i(TAG, "find %d unused resources:\n %s", unusedResources.size(), unusedResources)
                    Log.i(TAG, "find %d duplicated resources", dupResources.size())
                    for (String md5 : dupResources.keySet()) {
                        Log.i(TAG, "md5:%s, files:%s", md5, dupResources.get(md5))
                    }
                    Log.i(TAG, "find unused resources cost time %f s ", (System.currentTimeMillis() - startTime) / 1000.0f)

                    resTxtFile = new File(rTxtFile)
                    ApkUtil.readResourceTxtFile(resTxtFile, resourceMap, styleableMap)

                    prepareResources()

                    File inputFile = new File(originalApk)
                    String outputApk = inputFile.getParentFile().getAbsolutePath() + File.separator + inputFile.getName().substring(0, inputFile.getName().indexOf('.')) + "_shrinked.apk"
                    File outputFile = new File(outputApk)
                    if (outputFile.exists()) {
                        Log.w(TAG, "output apk file %s is already exists! It will be deleted anyway!", outputApk)
                        outputFile.delete()
                        outputFile.createNewFile()
                    }

                    startTime = System.currentTimeMillis()
                    boolean removeSuccess = removeUnusedResources(inputFile, outputFile)
                    Log.i(TAG, "remove duplicated resources & unused resources success? %s, cost time %f s", removeSuccess, (System.currentTimeMillis() - startTime) / 1000.0f)


                    if (removeSuccess) {
                        //resign the apk
                        boolean needSign = project.extensions.matrix.removeUnusedResources.needSign
                        String apksigner = project.extensions.matrix.removeUnusedResources.apksignerPath
                        if (needSign) {
                            if (Util.isNullOrNil(apksigner)) {
                                throw new GradleException("need sign apk but apksigner not found!")
                            } else if (!new File(apksigner).exists()) {
                                throw new GradleException("need sign apk but apksigner " + apksigner + " is not exist!")
                            } else if (signingConfig == null) {
                                throw new GradleException("need sign apk but signingConfig not found!")
                            }
                            Log.i(TAG, "resign apk...")
                            ApkUtil.signApk(outputApk, apksigner, variant.variantData.variantConfiguration.signingConfig)
                        }
                        String backApk = backupOutputPath + "${File.separator}backup.apk"
                        FileUtils.copyFile(inputFile, new File(backApk))
                        outputFile.renameTo(new File(originalApk))

                        //modify R.txt
                        modifyRFile(backupOutputPath)
                    }

                    //write resource mapping to resguard-mapping.txt
                    boolean resguard = project.extensions.matrix.removeUnusedResources.resguard
                    if (resguard) {
                        File resguardMappingFile = new File(backupOutputPath + File.separator + RESGUARD_MAPPING_FILE_NAME)
                        resguardMappingFile.createNewFile()
                        PrintWriter fileWriter = new PrintWriter(resguardMappingFile)
                        fileWriter.println(RES_PATH_MAPPING_PREFIX)
                        if (!resDirProguard.isEmpty()) {
                            for(String srcDir : resDirProguard.keySet()) {
                                fileWriter.println("    " + srcDir + " -> " + resDirProguard.get(srcDir))
                            }
                        }
                        fileWriter.println(RES_ID_MAPPING_PREFIX)
                        if (!resNameProguard.isEmpty()) {
                            for(String srcRes : resNameProguard.keySet()) {
                                fileWriter.println("    " + srcRes + " -> " + resNameProguard.get(srcRes))
                            }
                        }
                        fileWriter.println(RES_FILE_MAPPING_PREFIX)
                        if (!resFileProguard.isEmpty()) {
                            for (String srcFile : resFileProguard.keySet()) {
                                fileWriter.println("    " + srcFile + " -> " + resFileProguard.get(srcFile))
                            }
                        }
                        fileWriter.close()
                    }
                }
            }
        }
    }

    void findUnusedResources(String originalApk, String rTxtFile, String mappingTxtFile, Set<String> unusedResources, Map<String, Set<String>> dupResources) {
        ProcessBuilder processBuilder = new ProcessBuilder()
        String apkChecker = project.extensions.matrix.removeUnusedResources.apkCheckerPath

        if (!Util.isNullOrNil(apkChecker)) {
            if (!new File(apkChecker).exists()) {
                throw new GradleException("the path of Matrix-ApkChecker " + apkChecker + " is not exist!")
            }
            String tempOutput = project.getBuildDir().getAbsolutePath() + "${File.separator}apk_checker"
            ArrayList<String> commandList = new ArrayList<>()
            commandList.add("java")
            commandList.add("-jar")
            commandList.add(apkChecker)
            commandList.add("--apk")
            commandList.add(originalApk)
            commandList.add("--output")
            commandList.add(tempOutput)

            if (new File(mappingTxtFile).exists()) {
                commandList.add("--mappingTxt")
                commandList.add(mappingTxtFile)
            }

            commandList.add("--format")
            commandList.add("json")
            commandList.add("-unusedResources")
            commandList.add("--rTxt")
            commandList.add(rTxtFile)

            if (project.extensions.matrix.removeUnusedResources.deDuplicate) {
                commandList.add("-duplicatedFile")
            }

            processBuilder.command(commandList)
            Process process = processBuilder.start()
            println process.text
            process.waitFor()
            File outputFile = new File(project.getBuildDir().getAbsolutePath() + "${File.separator}apk_checker.json")
            if (outputFile.exists()) {
                Gson gson = new Gson()
                JsonArray jsonArray = gson.fromJson(outputFile.text, JsonArray.class)
                for (int i = 0; i < jsonArray.size(); i++) {
                    if (jsonArray.get(i).asJsonObject.get("taskType").asInt == 12) {
                        JsonArray resList = jsonArray.get(i).asJsonObject.get("unused-resources").asJsonArray
                        for (int j = 0; j < resList.size(); j++) {
                            unusedResources.add(resList.get(j).asString)
                        }
                    }
                    if (jsonArray.get(i).asJsonObject.get("taskType").asInt == 10) {
                        JsonArray dupList = jsonArray.get(i).asJsonObject.get("files").asJsonArray
                        for (int k = 0; k < dupList.size(); k++) {
                            JsonObject dupObj = dupList.get(k).asJsonObject
                            String md5 = dupObj.get("md5")
                            dupResources.put(md5, new HashSet())
                            JsonArray fileList = dupObj.get("files").asJsonArray
                            for (int m = 0; m < fileList.size(); m++) {
                                dupResources.get(md5).add(fileList.get(m).getAsString())
                            }
                        }
                    }
                }
                //outputFile.delete()
            }
        } else {
            throw new GradleException("the path of Matrix-ApkChecker not found!")
        }
    }

    void prepareResources() {

        //prepare unused resources
        Set<String> ignoreRes = project.extensions.matrix.removeUnusedResources.ignoreResources
        for (String res : ignoreRes) {
            ignoreResources.add(Util.globToRegexp(res))
        }

        for (String resName : unusedResources) {
            if (!ignoreResource(resName)) {
                removeResources.put(resName, resourceMap.remove(resName))
            } else {
                Log.i(TAG, "ignore remove unused resources %s", resName)
            }
        }

        Log.i(TAG, "unused resources count:%d", unusedResources.size())

        //prepare duplicated resources
        for (String md5 : dupResources.keySet()) {
            Set<String> dupFileList = dupResources.get(md5)
            Map<String, String> dupResName = new HashMap<>()
            for (String file : dupFileList) {
                if (!file.startsWith("res/")) {
                    Log.w(TAG, " %s is not resource file!", file)
                    continue
                } else {
                    dupResName.put(file, ApkUtil.entryToResourceName(file))
                }
            }
            if (dupResName.size() > 0) {
                if (!ApkUtil.isSameResourceType(dupResName.keySet())) {
                    Log.w(TAG, "the type of duplicated resources %s are not same!", dupFileList)
                    continue
                } else {
                    Iterator itera = dupResName.keySet().iterator()
                    while (itera.hasNext()) {
                        String resFile = itera.next()
                        if (!resourceMap.containsKey(dupResName.get(resFile))) {
                            Log.w(TAG, "can not find duplicated resources %s!", resFile)
                            itera.remove()
                        }
                    }

                    if (dupResName.size() > 1) {
                        itera = dupResName.keySet().iterator()
                        String replace = itera.next()
                        while (itera.hasNext()) {
                            String dup = itera.next()
                            Log.i(TAG, "replace %s with %s", dup, replace)
                            dupReplaceResources.put(dup, replace)
                        }
                    }
                }
            }
        }

        //prepare proguard resource name

        HashMap<String, HashSet<String>> resTypeNameMap = new HashMap<>()
        for (String resource : resourceMap.keySet()) {
            String type = ApkUtil.parseResourceType(resource)
            if (!resTypeNameMap.containsKey(type)) {
                resTypeNameMap.put(type, new HashSet<String>())
            }
            resTypeNameMap.get(type).add(resource)
        }

        for (String resType : resTypeNameMap.keySet()) {

            ProguardStringBuilder resTypeBuilder = new ProguardStringBuilder()
            Set<String> resNames = resTypeNameMap.get(resType)

            for (String resName : resNames) {
                if (!ignoreResource(resName)) {
                    resNameProguard.put(resName, "R." + resType + "." + resTypeBuilder.generateNextProguard())
                } else {
                    Log.i(TAG, "ignore proguard resource name %s", resName)
                }
            }
        }

    }

    boolean removeUnusedResources(File inputFile, File outputFile) {

        ZipOutputStream zipOutputStream = null

        boolean shrinkArsc = project.extensions.matrix.removeUnusedResources.shrinkArsc
        boolean resguard = project.extensions.matrix.removeUnusedResources.resguard
        boolean needSign = project.extensions.matrix.removeUnusedResources.needSign

        try {
            try {

                boolean rmUnused = unusedResources.size() > 0
                boolean rmDuplicated = dupReplaceResources.size() > 0

                ZipFile zipInputFile = new ZipFile(inputFile)
                zipOutputStream = new ZipOutputStream(new FileOutputStream(outputFile))
                ZipEntry arsc = zipInputFile.getEntry(ARSC_FILE_NAME)

                Log.i(TAG, "rmUnsed %s, rmDuplicated %s, resguard %s", rmUnused, rmDuplicated, resguard)

                if (!(rmUnused || rmDuplicated || resguard)) {
                    return false
                }

                File srcArscFile = new File(inputFile.getParentFile().getAbsolutePath() + File.separator + ARSC_FILE_NAME)
                File destArscFile = new File(inputFile.getParentFile().getAbsolutePath() + File.separator + "shrinked_" + ARSC_FILE_NAME)
                if (srcArscFile.exists()) {
                    srcArscFile.delete()
                    srcArscFile.createNewFile()
                }
                ApkUtil.unzipEntry(zipInputFile, arsc, srcArscFile)

                if (rmUnused || rmDuplicated || resguard) {

                    ArscReader reader = new ArscReader(srcArscFile.getAbsolutePath())
                    ResTable resTable = reader.readResourceTable()

                    //remove unused resources
                    if (rmUnused && shrinkArsc) {
                        for (String resName : removeResources.keySet()) {
                            ArscUtil.removeResource(resTable, removeResources.get(resName), resName)
                        }
                    }

                    //remove duplicated resources
                    if (rmDuplicated) {
                        Iterator replaceIterator = dupReplaceResources.keySet().iterator()
                        while (replaceIterator.hasNext()) {
                            String sourceFile = replaceIterator.next()
                            String sourceRes = ApkUtil.entryToResourceName(sourceFile)
                            int sourceId = resourceMap.get(sourceRes)
                            String targetFile = dupReplaceResources.get(sourceFile)
                            String targetRes = ApkUtil.entryToResourceName(targetFile)
                            int targetId = resourceMap.get(targetRes)
                            boolean success = ArscUtil.replaceFileResource(resTable, sourceId, sourceFile, targetId, targetFile)
                            if (!success) {
                                Log.w(TAG, "replace %s(%s) with %s(%s) failed!", sourceRes, sourceFile, targetRes, targetFile)
                                replaceIterator.remove()
                            }
                        }
                    }

                    //proguard resource name
                    if (resguard) {
                        Iterator resIterator = resourceMap.keySet().iterator()
                        Map<Integer, String> resIdProguard = new HashMap<>()
                        while (resIterator.hasNext()) {
                            String resource = resIterator.next()
                            if (resNameProguard.containsKey(resource)) {
                                resIdProguard.put(resourceMap.get(resource), ApkUtil.parseResourceName(resNameProguard.get(resource)))
                            }
                        }
                        if (!resIdProguard.isEmpty()) {
                            ArscUtil.replaceResEntryName(resTable, resIdProguard)
                        }
                    }

                    ProguardStringBuilder dirProguard = new ProguardStringBuilder()

                    Map<String, ProguardStringBuilder> dirFileProguard = new HashMap<>()

                    for (ZipEntry zipEntry : zipInputFile.entries()) {
                        if (zipEntry.name.startsWith("res/")) {
                            String resourceName = ApkUtil.entryToResourceName(zipEntry.name)
                            if (!Util.isNullOrNil(resourceName)) {
                                if (removeResources.containsKey(resourceName)) {
                                    Log.i(TAG, "remove unused resource %s file %s", resourceName, zipEntry.name)
                                    continue
                                } else if (dupReplaceResources.containsKey(zipEntry.name)) {
                                    Log.i(TAG, "remove duplicated resource file %s", zipEntry.name)
                                    continue
                                } else {
                                    if (arsc != null && resguard) {
                                        String dir = zipEntry.name.substring(0, zipEntry.name.lastIndexOf('/'))
                                        String suffix = zipEntry.name.substring(zipEntry.name.indexOf('.'))
                                        if (!resDirProguard.containsKey(dir)) {
                                            String proguardDir = dirProguard.generateNextProguardFileName()
                                            resDirProguard.put(dir, RES_DIR_PROGUARD_NAME + "/" + proguardDir)
                                            dirFileProguard.put(dir, new ProguardStringBuilder())
                                        }
                                        resFileProguard.put(zipEntry.name, resDirProguard.get(dir) + "/" + dirFileProguard.get(dir).generateNextProguardFileName() + suffix)
                                        boolean success = ArscUtil.replaceResFileName(resTable, resourceMap.get(resourceName), zipEntry.name, resFileProguard.get(zipEntry.name))
                                        if (success) {
                                            ApkUtil.addZipEntry(zipOutputStream, zipEntry, resFileProguard.get(zipEntry.name), zipInputFile)
                                        } else {
                                            ApkUtil.addZipEntry(zipOutputStream, zipEntry, zipInputFile)
                                        }
                                    } else {
                                        ApkUtil.addZipEntry(zipOutputStream, zipEntry, zipInputFile)
                                    }
                                }
                            } else {
                                Log.w(TAG, "parse entry %s resource name failed!", zipEntry.name)
                            }
                        } else {
                            if (needSign && zipEntry.name.startsWith("META-INF/")) {
                                continue
                            } else if (!zipEntry.name.equals(ARSC_FILE_NAME)) {
                                ApkUtil.addZipEntry(zipOutputStream, zipEntry, zipInputFile)
                            }
                        }
                    }

                    ArscWriter writer = new ArscWriter(destArscFile.getAbsolutePath())
                    writer.writeResTable(resTable)
                    Log.i(TAG, "shrink resources.arsc size %f KB", (srcArscFile.length() - destArscFile.length()) / 1024.0)
                    ApkUtil.addZipEntry(zipOutputStream, arsc, destArscFile)

                    srcArscFile.delete()
                    destArscFile.delete()
                }

                if (zipOutputStream != null) {
                    zipOutputStream.close()
                }
                Log.i(TAG, "shrink apk size %f KB", (inputFile.length() - outputFile.length()) / 1024.0)
                return true

            } finally {
                if (zipOutputStream != null) {
                    zipOutputStream.close()
                }
            }
        } catch (Exception e) {
            Log.printErrStackTrace(TAG, e, "remove unused resources occur error!")
            return false
        }
    }

    void modifyRFile(String backupOutputPath) {

        //modify R.txt to delete the removed resources
        if (!removeResources.isEmpty()) {
            Iterator<String> styleableItera = styleableMap.keySet().iterator()
            while (styleableItera.hasNext()) {
                String styleable = styleableItera.next()
                Pair<String, Integer>[] attrs = styleableMap.get(styleable)
                int i = 0;
                for (i = 0; i < attrs.length; i++) {
                    if (!removeResources.containsValue(attrs[i].right)) {
                        break
                    }
                }
                if (attrs.length > 0 && i == attrs.length) {
                    Log.i(TAG, "removed styleable " + styleable)
                    styleableItera.remove()
                }
            }
            //Log.d(TAG, "styleable %s", styleableMap.keySet().size())
            String newResTxtFile = backupOutputPath + File.separator + SHRINKED_R_TXT_FILE
            ApkUtil.shrinkResourceTxtFile(newResTxtFile, resourceMap, styleableMap)

            //Other plugins such as "Tinker" may depend on the R.txt file, so we should not modify R.txt directly .
            //new File(newResTxtFile).renameTo(resTxtFile)
        }
    }
}
