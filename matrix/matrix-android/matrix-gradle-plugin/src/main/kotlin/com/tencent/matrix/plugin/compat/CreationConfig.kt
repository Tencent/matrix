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

package com.tencent.matrix.plugin.compat

import com.android.build.gradle.api.BaseVariant
import com.android.builder.model.CodeShrinker
import org.gradle.api.Project

class CreationConfig(
        val variant: BaseVariant,
        val project: Project
) {
    companion object {

        fun getCodeShrinker(project: Project): CodeShrinker {

            var enableR8: Boolean = when (val property = project.properties["android.enableR8"]) {
                null -> true
                else -> (property as String).toBoolean()
            }

            return when {
                enableR8 -> CodeShrinker.R8
                else -> CodeShrinker.PROGUARD
            }
        }
    }
}