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

package com.tencent.matrix.trace.retrace;

/**
 * Created by caichongyang on 2017/6/3.
 */
public class MethodInfo {

    private final String originalClassName;

    public String originalType;
    public String originalArguments;
    public String originalName;
    public String desc;

    public MethodInfo(String originalClassName,
                      String originalType,
                      String originalName,
                      String originalArguments) {

        this.originalType = originalType;
        this.originalArguments = originalArguments;
        this.originalClassName = originalClassName;
        this.originalName = originalName;
    }

    public MethodInfo(MethodInfo methodInfo) {
        this.originalType = methodInfo.getOriginalType();
        this.originalArguments = methodInfo.getOriginalArguments();
        this.originalClassName = methodInfo.getOriginalClassName();
        this.originalName = methodInfo.getOriginalName();
        this.desc = methodInfo.getDesc();
    }

    public static MethodInfo deFault() {
        return new MethodInfo("", "", "", "");
    }

    public boolean matches(String originalType, String originalArguments) {
        boolean bool = (originalType == null || originalType.equals(this.originalType))
                && (originalArguments == null || originalArguments.equals(this.originalArguments));
        return bool;
    }

    public String getOriginalClassName() {
        return originalClassName;
    }

    public String getOriginalType() {
        return originalType;
    }

    public String getOriginalName() {
        return originalName;
    }

    public String getOriginalArguments() {
        return originalArguments;
    }

    public String getDesc() {
        return desc;
    }

    public void setDesc(String desc) {
        this.desc = desc;
    }

    public void setOriginalName(String originalName) {
        this.originalName = originalName;
    }

    public void setOriginalArguments(String originalArguments) {
        this.originalArguments = originalArguments;
    }

    public void setOriginalType(String originalType) {
        this.originalType = originalType;
    }

    @Override
    public String toString() {
        return "MethodInfo{" + "originalClassName='" + originalClassName + '\'' + ", originalType='"
                + originalType + '\'' + ", originalArguments='" + originalArguments
                + '\'' + ", originalName='" + originalName + '\'' + '}';
    }
}
