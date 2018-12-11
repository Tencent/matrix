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

package com.tencent.matrix.resource.common.utils;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.Closeable;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

/**
 * Created by tangyinsheng on 2017/7/10.
 */

public final class StreamUtil {

    public static void closeQuietly(Object target) {
        if (target == null) {
            return;
        }
        try {
            if (target instanceof Closeable) {
                ((Closeable) target).close();
            } else if (target instanceof ZipFile) {
                ((ZipFile) target).close();
            }
        } catch (Throwable ignored) {
            // Ignored.
        }
    }

    public static void extractZipEntry(ZipFile zipFile, ZipEntry targetEntry, File output) throws IOException {
        InputStream is = null;
        OutputStream os = null;
        try {
            is = new BufferedInputStream(zipFile.getInputStream(targetEntry));
            os = new BufferedOutputStream(new FileOutputStream(output));
            final byte[] buffer = new byte[4096];
            int bytesRead = 0;
            while ((bytesRead = is.read(buffer)) > 0) {
                os.write(buffer, 0, bytesRead);
            }
        } finally {
            closeQuietly(os);
            closeQuietly(is);
        }
    }

    public static void copyFileToStream(File in, OutputStream out) throws IOException {
        InputStream is = null;
        final byte[] buffer = new byte[4096];
        try {
            is = new BufferedInputStream(new FileInputStream(in));
            int bytesRead = 0;
            while ((bytesRead = is.read(buffer)) > 0) {
                out.write(buffer, 0, bytesRead);
            }
            out.flush();
        } finally {
            closeQuietly(is);
        }
    }

    private StreamUtil() {
        throw new UnsupportedOperationException();
    }
}
