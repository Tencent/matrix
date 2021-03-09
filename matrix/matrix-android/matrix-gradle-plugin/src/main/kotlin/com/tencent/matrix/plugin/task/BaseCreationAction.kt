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

import com.android.utils.appendCapitalized
import com.tencent.matrix.plugin.compat.CreationConfig
import org.gradle.api.Task
import org.gradle.api.tasks.TaskContainer
import org.gradle.api.tasks.TaskProvider

interface ICreationAction<TaskT> {
    val name: String
    val type: Class<TaskT>
}

abstract class BaseCreationAction<TaskT>(
        private val creationConfig: CreationConfig
) : ICreationAction<TaskT> {

    companion object {
        @JvmField
        val computeTaskName = { prefix: String, name: String, suffix: String
            ->
            prefix.appendCapitalized(name, suffix)
        }

        fun findNamedTask(taskContainer: TaskContainer, name: String): TaskProvider<Task>? {
            try {
                return taskContainer.named(name)
            } catch (t: Throwable) {} finally {}

            return null
        }
    }

    fun computeTaskName(prefix: String, suffix: String): String =
            computeTaskName(prefix, creationConfig.variant.name, suffix)
}