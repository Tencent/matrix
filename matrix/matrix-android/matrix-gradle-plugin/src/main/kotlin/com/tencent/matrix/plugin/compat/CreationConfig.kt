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