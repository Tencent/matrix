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

package com.tencent.matrix.resource.analyzer.model;

import java.lang.ref.WeakReference;
import java.util.EnumSet;

/**
 * Created by tangyinsheng on 2017/8/9.
 */

public enum AndroidExcludedBmpRefs {

    EXCLUDE_GCROOT_WITH_SYSTEM_CLASS() {
        @Override
        void config(ExcludedBmps.Builder builder) {
            builder.addClassNamePattern("^android\\..*", true);
            builder.addClassNamePattern("^com\\.android\\..*", true);
        }
    },

    EXCLUDE_WEAKREFERENCE_HOLDER() {
        @Override
        void config(ExcludedBmps.Builder builder) {
            builder.instanceField(WeakReference.class.getName(), "referent");
        }
    };

    public static ExcludedBmps.Builder createDefaults() {
        ExcludedBmps.Builder builder = ExcludedBmps.builder();
        for (AndroidExcludedBmpRefs item : EnumSet.allOf(AndroidExcludedBmpRefs.class)) {
            item.config(builder);
        }
        return builder;
    }

    abstract void config(ExcludedBmps.Builder builder);
}
