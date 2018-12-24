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

package com.tencent.matrix.plugin.transform

import com.android.build.api.transform.DirectoryInput
import com.android.build.api.transform.QualifiedContent
import com.android.build.api.transform.Status
import com.android.build.api.transform.Transform
import com.google.common.base.Charsets
import com.google.common.hash.Hashing
import com.tencent.matrix.javalib.util.ReflectUtil

import java.lang.reflect.Field

/**
 * Created by caichongyang on 2017/6/20.
 */

public abstract class BaseProxyTransform extends Transform {
    protected final Transform origTransform

    public BaseProxyTransform(Transform origTransform) {
        this.origTransform = origTransform
    }

    @Override
    Set<QualifiedContent.ContentType> getInputTypes() {
        return origTransform.getInputTypes()
    }

    @Override
    Set<QualifiedContent.Scope> getScopes() {
        return origTransform.getScopes()
    }

    @Override
    boolean isIncremental() {
        return origTransform.isIncremental()
    }

    protected String getUniqueJarName(File jarFile) {
        final String origJarName = jarFile.getName()
        final String hashing = Hashing.sha1().hashString(jarFile.getPath(), Charsets.UTF_16LE).toString()
        final int dotPos = origJarName.lastIndexOf('.')
        if (dotPos < 0) {
            return "${origJarName}_${hashing}"
        } else {
            final String nameWithoutDotExt = origJarName.substring(0, dotPos)
            final String dotExt = origJarName.substring(dotPos)
            return "${nameWithoutDotExt}_${hashing}${dotExt}"
        }
    }

    protected void replaceFile(QualifiedContent input, File newFile) {
        final Field fileField = ReflectUtil.getDeclaredFieldRecursive(input.getClass(), 'file')
        fileField.set(input, newFile)
    }

    protected void replaceChangedFile(DirectoryInput dirInput, Map<File, Status> changedFiles) {
        final Field changedFilesField = ReflectUtil.getDeclaredFieldRecursive(dirInput.getClass(), 'changedFiles')
        changedFilesField.set(dirInput, changedFiles)
    }
}
