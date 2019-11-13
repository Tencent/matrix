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

package com.tencent.matrix.plugin.util

import com.android.build.gradle.internal.dsl.SigningConfig
import com.tencent.matrix.javalib.util.Util
import org.gradle.api.GradleException
import org.gradle.internal.Pair

import java.util.zip.CRC32
import java.util.zip.ZipEntry
import java.util.zip.ZipFile
import java.util.zip.ZipOutputStream

public class ApkUtil {

    static String parseResourceId(String resId) {
        if (!Util.isNullOrNil(resId) && resId.startsWith("0x")) {
            if (resId.length() == 10) {
                return resId
            } else if (resId.length() < 10) {
                StringBuilder strBuilder = new StringBuilder(resId)
                for (int i = 0; i < 10 - resId.length(); i++) {
                    strBuilder.append('0')
                }
                return strBuilder.toString()
            }
        }
        return ""
    }

    static String entryToResourceName(String entry) {
        String resourceName = ""
        if (!Util.isNullOrNil(entry)) {
            String typeName = parseEntryResourceType(entry)
            String resName = entry.substring(entry.lastIndexOf('/') + 1, entry.indexOf('.'))
            if (!Util.isNullOrNil(typeName) && !Util.isNullOrNil(resName)) {
                resourceName = "R." + typeName + "." + resName
            }
        }
        return resourceName
    }

    static String parseEntryResourceType(String entry) {
        if (!Util.isNullOrNil(entry) && entry.length() > 4) {
            String typeName = entry.substring(4, entry.lastIndexOf('/'))
            if (!Util.isNullOrNil(typeName)) {
                int index = typeName.indexOf('-')
                if (index >= 0) {
                    typeName = typeName.substring(0, index)
                }
                return typeName
            }
        }
        return ""
    }

    static boolean isSameResourceType(Set<String> entries) {
        String resType = ""
        for (String entry : entries) {
            if (!Util.isNullOrNil(entry)) {
                if (Util.isNullOrNil(resType)) {
                    resType = parseEntryResourceType(entry)
                    continue
                }
                if (!resType.equals(parseEntryResourceType(entry))) {
                    return false
                }
            } else {
                return false
            }
        }
        if (Util.isNullOrNil(resType)) {
            return false
        } else {
            return true
        }
    }

    static String parseResourceType(String resource) {
        resource.substring(resource.indexOf('.') + 1, resource.lastIndexOf('.'))
    }

    static String parseResourceName(String resource) {
        resource.substring(resource.lastIndexOf('.') + 1)
    }

    static byte[] readFileContent(InputStream inputStream) throws IOException {
        ByteArrayOutputStream output = new ByteArrayOutputStream()
        try {
            final BufferedInputStream bufferedInput = new BufferedInputStream(inputStream)
            int len
            byte[] buffer = new byte[4096]
            while ((len = bufferedInput.read(buffer)) != -1) {
                output.write(buffer, 0, len)
            }
            bufferedInput.close()
        } finally {
            output.close()
        }
        return output.toByteArray()
    }

    static void unzipEntry(ZipFile zipFile, ZipEntry zipEntry, File destFile) {
        BufferedOutputStream outputStream = new BufferedOutputStream(new FileOutputStream(destFile))
        InputStream inputStream = zipFile.getInputStream(zipEntry)
        outputStream.write(readFileContent(inputStream))
        outputStream.close()
    }

    static void addZipEntry(ZipOutputStream zipOutputStream, ZipEntry zipEntry, File file) {
        ZipEntry writeEntry = new ZipEntry(zipEntry.getName())
        InputStream inputStream = new FileInputStream(file)
        byte[] content = readFileContent(inputStream)
        if (zipEntry.getMethod() == ZipEntry.DEFLATED) {
            writeEntry.setMethod(ZipEntry.DEFLATED)
        } else {
            writeEntry.setMethod(ZipEntry.STORED)
            CRC32 crc32 = new CRC32()
            crc32.update(content)
            writeEntry.setCrc(crc32.getValue())
        }
        writeEntry.setSize(content.length)
        zipOutputStream.putNextEntry(writeEntry)
        zipOutputStream.write(content)
        zipOutputStream.flush()
        zipOutputStream.closeEntry()
    }

    static void addZipEntry(ZipOutputStream zipOutputStream, ZipEntry zipEntry, ZipFile zipFile) throws IOException {
        ZipEntry writeEntry = new ZipEntry(zipEntry.getName())
        InputStream inputStream = zipFile.getInputStream(zipEntry)
        byte[] content = readFileContent(inputStream)
        if (zipEntry.getMethod() == ZipEntry.DEFLATED) {
            writeEntry.setMethod(ZipEntry.DEFLATED)
        } else {
            writeEntry.setMethod(ZipEntry.STORED)
            writeEntry.setCrc(zipEntry.getCrc().longValue())
            writeEntry.setSize(zipEntry.size)
        }
        zipOutputStream.putNextEntry(writeEntry)
        zipOutputStream.write(content)
        zipOutputStream.flush()
        zipOutputStream.closeEntry()
    }

    static void addZipEntry(ZipOutputStream zipOutputStream, ZipEntry zipEntry, String newName, ZipFile zipFile) throws IOException {
        ZipEntry writeEntry = new ZipEntry(newName)
        InputStream inputStream = zipFile.getInputStream(zipEntry)
        byte[] content = readFileContent(inputStream)
        if (zipEntry.getMethod() == ZipEntry.DEFLATED) {
            writeEntry.setMethod(ZipEntry.DEFLATED)
        } else {
            writeEntry.setMethod(ZipEntry.STORED)
            writeEntry.setCrc(zipEntry.getCrc().longValue())
            writeEntry.setSize(zipEntry.size)
        }
        zipOutputStream.putNextEntry(writeEntry)
        zipOutputStream.write(content)
        zipOutputStream.flush()
        zipOutputStream.closeEntry()
    }

    static void readResourceTxtFile(File resTxtFile, Map<String, Integer> resourceMap, Map<String, Pair<String, Integer>> styleableMap) throws IOException {
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

    static void shrinkResourceTxtFile(String resourceTxt, HashMap<String, Integer> resourceMap, Map<String, Pair<String, Integer>[]> styleableMap) throws IOException {
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

    static void signApk(String apkFilePath, String apksigner, SigningConfig signingConfig) {
        ProcessBuilder processBuilder = new ProcessBuilder()
        processBuilder.command(apksigner, "sign", "-v",
                "--ks", signingConfig.storeFile.getAbsolutePath(),
                "--ks-pass", "pass:" + signingConfig.storePassword,
                "--key-pass", "pass:" + signingConfig.keyPassword,
                "--ks-key-alias", signingConfig.keyAlias,
                apkFilePath)
        //Log.i(TAG, "%s", processBuilder.command())
        Process process = processBuilder.start()
        process.waitFor()
        if (process.exitValue() != 0) {
            throw new GradleException(process.getErrorStream().text)
        }
    }

}