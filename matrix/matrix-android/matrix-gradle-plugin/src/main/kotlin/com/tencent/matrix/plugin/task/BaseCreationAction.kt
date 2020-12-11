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