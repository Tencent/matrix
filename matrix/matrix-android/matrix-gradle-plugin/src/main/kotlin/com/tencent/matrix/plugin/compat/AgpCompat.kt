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
import com.android.builder.model.SigningConfig

class AgpCompat {

    companion object {
        @JvmField
        val getIntermediatesSymbolDirName = {
            when {
                VersionsCompat.lessThan(AGPVersion.AGP_3_6_0) -> "symbols"
                VersionsCompat.greatThanOrEqual(AGPVersion.AGP_3_6_0) -> "runtime_symbol_list"
                else -> "symbols"
            }
        }

        fun getSigningConfig(variant: BaseVariant): SigningConfig? {
            return variant.buildType.signingConfig
        }
    }

}