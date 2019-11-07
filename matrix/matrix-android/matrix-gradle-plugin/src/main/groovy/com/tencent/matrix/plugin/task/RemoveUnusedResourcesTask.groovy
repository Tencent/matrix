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

import com.android.build.gradle.internal.dsl.SigningConfig
import com.google.gson.Gson
import com.google.gson.JsonArray
import com.google.gson.JsonObject
import com.tencent.matrix.javalib.util.Log
import com.tencent.matrix.javalib.util.Util
import com.tencent.matrix.plugin.util.ApkUtil
import com.tencent.mm.arscutil.ArscUtil
import com.tencent.mm.arscutil.data.ResTable
import com.tencent.mm.arscutil.io.ArscReader
import com.tencent.mm.arscutil.io.ArscWriter
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

    public static final ARSC_FILE_NAME = "resources.arsc"

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

        project.extensions.android.applicationVariants.all { variant ->
            if (variant.name.equalsIgnoreCase(variantName)) {
                variant.outputs.forEach { output ->
                    String unsignedApkPath = output.outputFile.getAbsolutePath()
                    Log.i(TAG, "original apk file %s", unsignedApkPath)
                    long startTime = System.currentTimeMillis()
                    String rTxtFile = project.getBuildDir().getAbsolutePath() + "/intermediates/symbols/${variant.name}/R.txt"
                    String mappingTxtFile = project.getBuildDir().getAbsolutePath() + "/outputs/mapping/${variant.name}/mapping.txt"
                    findUnusedResources(unsignedApkPath, rTxtFile, mappingTxtFile, unusedResources, dupResources)
                    Log.i(TAG, "find %d unused resources:\n %s", unusedResources.size(), unusedResources)
                    Log.i(TAG, "find %d duplicated resources", dupResources.size())
                    for(String md5 : dupResources.keySet()) {
                        Log.i(TAG, "md5:%s, files:%s", md5, dupResources.get(md5))
                    }
                    Log.i(TAG, "find unused resources cost time %f s ", (System.currentTimeMillis() - startTime) / 1000.0f)

                    resTxtFile = new File(rTxtFile)
                    readResourceTxtFile(resTxtFile)

                    if (dupResources.size() > 0 || unusedResources.size() > 0) {
                        startTime = System.currentTimeMillis()
                        removeUnusedResources(unsignedApkPath, variant.variantData.variantConfiguration.signingConfig)
                        Log.i(TAG, "remove duplicated resources & unused resources cost time %f s", (System.currentTimeMillis() - startTime) / 1000.0f)
                    }
                }
            }
        }
    }

    void findUnusedResources(String originalApk, String rTxtFile, String mappingTxtFile, Set<String> unusedResources, Map<String, Set<String>> dupResources) {
        ProcessBuilder processBuilder = new ProcessBuilder()
        String apkChecker = project.extensions.matrix.removeUnusedResources.apkCheckerPath

        if (!Util.isNullOrNil(apkChecker)) {
            if (! new File(apkChecker).exists()) {
                throw new GradleException("the path of Matrix-ApkChecker " + apkChecker + " is not exist!")
            }
            String tempOutput = project.getBuildDir().getAbsolutePath() + "/apk_checker"
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
            File outputFile = new File(project.getBuildDir().getAbsolutePath() + "/apk_checker.json")
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

    void removeUnusedResources(String originalApk, SigningConfig signingConfig) {
        ZipOutputStream zipOutputStream = null
        boolean needSign = project.extensions.matrix.removeUnusedResources.needSign
        boolean shrinkArsc = project.extensions.matrix.removeUnusedResources.shrinkArsc
        String apksigner = project.extensions.matrix.removeUnusedResources.apksignerPath
        if (needSign) {
            if (Util.isNullOrNil(apksigner)) {
                throw new GradleException("need sign apk but apksigner not found!")
            } else if (! new File(apksigner).exists()) {
                throw new GradleException( "need sign apk but apksigner " + apksigner + " is not exist!")
            } else if (signingConfig == null) {
                throw new GradleException("need sign apk but signingConfig not found!")
            }
        }
        try {
            try {
                File inputFile = new File(originalApk)
                Set<String> ignoreRes = project.extensions.matrix.removeUnusedResources.ignoreResources
                for (String res : ignoreRes) {
                    ignoreResources.add(Util.globToRegexp(res))
                }
                Iterator<String> iterator = unusedResources.iterator()
                String res = null
                while (iterator.hasNext()) {
                    res = iterator.next()
                    if (ignoreResource(res)) {
                        iterator.remove()
                        Log.i(TAG, "ignore unused resources %s", res)
                    }
                }
                Log.i(TAG, "unused resources count:%d", unusedResources.size())

                String outputApk = inputFile.getParentFile().getAbsolutePath() + "/" + inputFile.getName().substring(0, inputFile.getName().indexOf('.')) + "_shrinked.apk"

                File outputFile = new File(outputApk)
                if (outputFile.exists()) {
                    Log.w(TAG, "output apk file %s is already exists! It will be deleted anyway!", outputApk)
                    outputFile.delete()
                    outputFile.createNewFile()
                }

                ZipFile zipInputFile = new ZipFile(inputFile)

                zipOutputStream = new ZipOutputStream(new FileOutputStream(outputFile))


                Map<String, Integer> removeResources = new HashMap<>()
                for (String resName : unusedResources) {
                    if (!ignoreResource(resName)) {
                        removeResources.put(resName, resourceMap.remove(resName))
                    }
                }

                Map<String, String> replaceResources = new HashMap<>()
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
                            while(itera.hasNext()) {
                                String resFile = itera.next()
                                if (!resourceMap.containsKey(dupResName.get(resFile))) {
                                    Log.w(TAG, "can not find duplicated resources %s!", resFile)
                                    itera.remove()
                                }
                            }

                            if (dupResName.size() > 1) {
                                itera  = dupResName.keySet().iterator()
                                String target = itera.next()
                                while (itera.hasNext()) {
                                    String dup = itera.next()
                                    Log.i(TAG, "replace %s with %s", dup, target)
                                    replaceResources.put(dup, target)
                                }
                            }
                        }
                    }
                }

                ZipEntry arsc = zipInputFile.getEntry(ARSC_FILE_NAME)

                if (arsc != null) {
                    if ((shrinkArsc && unusedResources.size() > 0) || replaceResources.size() > 0) {
                        File srcArscFile = new File(inputFile.getParentFile().getAbsolutePath() + "/" + ARSC_FILE_NAME)
                        File destArscFile = new File(inputFile.getParentFile().getAbsolutePath() + "/" + "shrinked_" + ARSC_FILE_NAME)
                        if (srcArscFile.exists()) {
                            srcArscFile.delete()
                            srcArscFile.createNewFile()
                        }
                        ApkUtil.unzipEntry(zipInputFile, arsc, srcArscFile)

                        ArscReader reader = new ArscReader(srcArscFile.getAbsolutePath())
                        ResTable resTable = reader.readResourceTable()
                        for (String resName : removeResources.keySet()) {
                            ArscUtil.removeResource(resTable, removeResources.get(resName), resName)
                        }
                        if (replaceResources.size() > 0) {
                            Iterator replaceIterator = replaceResources.keySet().iterator()
                            while (replaceIterator.hasNext()) {
                                String sourceFile = replaceIterator.next()
                                String sourceRes = ApkUtil.entryToResourceName(sourceFile)
                                int sourceId = resourceMap.get(sourceRes)
                                String targetFile = replaceResources.get(sourceFile)
                                String targetRes = ApkUtil.entryToResourceName(targetFile)
                                int targetId = resourceMap.get(targetRes)
                                boolean success = ArscUtil.replaceFileResource(resTable, sourceId, sourceFile, targetId, targetFile)
                                if (!success) {
                                    Log.w(TAG, "replace %s(%s) with %s(%s) failed!", sourceRes, sourceFile, targetRes, targetFile)
                                    replaceIterator.remove()
                                }
                            }
                        }
                        ArscWriter writer = new ArscWriter(destArscFile.getAbsolutePath())
                        writer.writeResTable(resTable)
                        Log.i(TAG, "shrink resources.arsc size %f KB", (srcArscFile.length() - destArscFile.length()) / 1024.0)
                        ApkUtil.addZipEntry(zipOutputStream, arsc, destArscFile)
                    } else {
                        ApkUtil.addZipEntry(zipOutputStream, arsc, zipInputFile)
                    }
                }

                for (ZipEntry zipEntry : zipInputFile.entries()) {
                    if (zipEntry.name.startsWith("res/")) {
                        String resourceName = ApkUtil.entryToResourceName(zipEntry.name)
                        if (!Util.isNullOrNil(resourceName)) {
                            if (removeResources.containsKey(resourceName)) {
                                Log.i(TAG, "remove unused resource %s", resourceName)
                                continue
                            } else if (replaceResources.containsKey(zipEntry.name)) {
                                Log.i(TAG, "remove duplicated resource %s", zipEntry.name)
                                continue
                            } else {
                                ApkUtil.addZipEntry(zipOutputStream, zipEntry, zipInputFile)
                            }
                        } else {
                            ApkUtil.addZipEntry(zipOutputStream, zipEntry, zipInputFile)
                        }
                    } else {
                        if (needSign && zipEntry.name.startsWith("META-INF/")) {
                            continue
                        } else if (!zipEntry.name.equals(ARSC_FILE_NAME)) {
                            ApkUtil.addZipEntry(zipOutputStream, zipEntry, zipInputFile)
                        }
                    }
                }

                if (zipOutputStream != null) {
                    zipOutputStream.close()
                }

                Log.i(TAG, "shrink apk size %f KB", (inputFile.length() - outputFile.length()) / 1024.0)
                if (needSign) {
                    Log.i(TAG, "resign apk...")
                    ProcessBuilder processBuilder = new ProcessBuilder()
                    processBuilder.command(apksigner, "sign", "-v",
                            "--ks", signingConfig.storeFile.getAbsolutePath(),
                            "--ks-pass", "pass:" + signingConfig.storePassword,
                            "--key-pass", "pass:" + signingConfig.keyPassword,
                            "--ks-key-alias", signingConfig.keyAlias,
                            outputFile.getAbsolutePath())
                    //Log.i(TAG, "%s", processBuilder.command())
                    Process process = processBuilder.start()
                    process.waitFor()
                    if (process.exitValue() != 0) {
                        throw new GradleException(process.getErrorStream().text)
                    }
                }
                String backApk = inputFile.getParentFile().getAbsolutePath() + "/" + inputFile.getName().substring(0, inputFile.getName().indexOf('.')) + "_back.apk";
                inputFile.renameTo(new File(backApk))
                outputFile.renameTo(new File(originalApk))

                //modify R.txt to delete the removed resources
                if (!removeResources.isEmpty()) {
                    Iterator<String> styleableItera =  styleableMap.keySet().iterator()
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
                    String newResTxtFile = resTxtFile.getParentFile().getAbsolutePath() + "/" + resTxtFile.getName().substring(0, resTxtFile.getName().indexOf('.')) + "_shrinked.txt";
                    shrinkResourceTxtFile(newResTxtFile, resourceMap, styleableMap)

                    //Other plugins such as "Tinker" may depend on the R.txt file, so we should not modify R.txt directly .
                    //new File(newResTxtFile).renameTo(resTxtFile)
                }

            } finally {
                if (zipOutputStream != null) {
                    zipOutputStream.close()
                }
            }
        } catch (Exception e) {
            Log.printErrStackTrace(TAG, e, "remove unused resources occur error!")
        }
    }

    private void shrinkResourceTxtFile(String resourceTxt, HashMap<String, Integer> resourceMap, Map<String, Pair<String, Integer>[]> styleableMap) throws IOException {
        BufferedWriter bufferedWriter = new BufferedWriter(new FileWriter(resourceTxt))
        try {
            for (String res : resourceMap.keySet()) {
                StringBuilder strBuilder = new StringBuilder()
                strBuilder.append("int").append(" ")
                        .append(res.substring(2, res.indexOf('.', 2))).append(" ")
                        .append(res.substring(res.indexOf('.', 2) + 1)).append(" ")
                        .append("0x" + Integer.toHexString(resourceMap.get(res)))
                //Log.d(TAG, "write %s to R.txt", strBuilder.toString())
                bufferedWriter.writeLine(strBuilder.toString())
            }
            for (String styleable : styleableMap.keySet()) {
                StringBuilder strBuilder = new StringBuilder()
                Pair<String, Integer>[] styleableAttrs = styleableMap.get(styleable)
                strBuilder.append("int[]").append(" ")
                        .append("styleable").append(" ")
                        .append(styleable.substring(styleable.indexOf('.', 2) + 1)).append(" ")
                        .append("{ ")
                for (int i = 0; i < styleableAttrs.length; i++) {
                    if (i != styleableAttrs.length - 1) {
                        strBuilder.append("0x" + Integer.toHexString(styleableAttrs[i].right)).append(", ")
                    } else {
                        strBuilder.append("0x" + Integer.toHexString(styleableAttrs[i].right))
                    }
                }
                strBuilder.append(" }")
                //Log.d(TAG, "write %s to R.txt", strBuilder.toString())
                bufferedWriter.writeLine(strBuilder.toString())
                for (int i = 0; i < styleableAttrs.length; i++) {
                    StringBuilder stringBuilder = new StringBuilder()
                    stringBuilder.append("int").append(" ")
                            .append("styleable").append(" ")
                            .append(styleableAttrs[i].left).append(" ")
                            .append(i)
                    //Log.d(TAG, "write %s to R.txt", stringBuilder.toString())
                    bufferedWriter.writeLine(stringBuilder.toString())
                }
            }
        } finally {
            bufferedWriter.close()
        }
    }

    void readResourceTxtFile(File resTxtFile) throws IOException {
        BufferedReader bufferedReader = new BufferedReader(new FileReader(resTxtFile))
        String line = bufferedReader.readLine()
        boolean styleable = false
        String styleableName = ""
        ArrayList<String> styleableAttrs = new ArrayList<>()
        try {
            while (line != null) {
                String[] columns = line.split(" ")
                if (columns.length >= 4) {
                    final String resourceName = "R." + columns[1] + "." + columns[2]
                    if (!columns[0].endsWith("[]") && columns[3].startsWith("0x")) {
                        if (styleable) {
                            styleable = false
                            styleableName = ""
                        }
                        final String resId = ApkUtil.parseResourceId(columns[3])
                        if (!Util.isNullOrNil(resId)) {
                            resourceMap.put(resourceName, Integer.decode(resId))
                        }
                    } else if (columns[1].equals("styleable")) {
                        if (columns[0].endsWith("[]")) {
                            if (columns.length > 5) {
                                styleableAttrs.clear()
                                styleable = true
                                styleableName = "R." + columns[1] + "." + columns[2]
                                for (int i = 4; i < columns.length - 1; i++) {
                                    if (columns[i].endsWith(",")) {
                                        styleableAttrs.add(columns[i].substring(0, columns[i].length() - 1))
                                    } else {
                                        styleableAttrs.add(columns[i])
                                    }
                                }
                                styleableMap.put(styleableName, new Pair<String, Integer>[styleableAttrs.size()])
                            }
                        } else {
                            if (styleable && !Util.isNullOrNil(styleableName)) {
                                final int index = Integer.parseInt(columns[3])
                                final String name = "R." + columns[1] + "." + columns[2]
                                styleableMap.get(styleableName)[index] = new Pair<String, Integer>(name, Integer.decode(ApkUtil.parseResourceId(styleableAttrs.get(index))))
                            }
                        }
                    } else {
                        if (styleable) {
                            styleable = false
                            styleableName = ""
                        }
                    }
                }
                line = bufferedReader.readLine()
            }
        } finally {
            bufferedReader.close()
        }
    }
}
