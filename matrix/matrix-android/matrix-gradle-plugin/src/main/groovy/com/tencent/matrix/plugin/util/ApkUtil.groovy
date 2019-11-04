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

import com.tencent.matrix.javalib.util.Util

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

    static String entryToResouceName(String entry) {
        String resourceName = "";
        if (!Util.isNullOrNil(entry)) {
            String typeName = entry.substring(4, entry.lastIndexOf('/'))
            String resName = entry.substring(entry.lastIndexOf('/') + 1, entry.indexOf('.'))
            if (!Util.isNullOrNil(typeName) && !Util.isNullOrNil(resName)) {
                int index = typeName.indexOf('-')
                if (index >= 0) {
                    typeName = typeName.substring(0, index)
                }
                resourceName = "R." + typeName + "." + resName;
            }
        }
        return resourceName;
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
}