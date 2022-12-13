package com.tencent.matrix.resguard

import java.io.File

internal class ResguardMapping(mappingFile: File) {

    private enum class ParseState {
        None, Path, ID;
    }

    private val pathMapping: Map<String, String>

    private val idMapping: Map<String, Map<String, String>>

    companion object {
        private val parsePathRegex = "^\\s+(.*) -> (.*)\$".toRegex()
        private val parseIDRegex = "^\\s+(\\S+)?R\\.(\\S+?)\\.(\\S+) -> (\\S+)?R\\.(\\S+?)\\.(\\S+)\$".toRegex()
    }

    init {
        var state = ParseState.None
        val pathMappingRaw = mutableMapOf<String, String>()
        val idMappingRaw = mutableMapOf<String, MutableMap<String, String>>()
        mappingFile.forEachLine { line ->
            when {
                line == "res path mapping:" -> {
                    state = ParseState.Path
                }
                line == "res id mapping:" -> {
                    state = ParseState.ID
                }
                line.isEmpty() -> {
                    state = ParseState.None
                }
                else -> {
                    if (state == ParseState.Path) {
                        parsePathRegex.matchEntire(line)?.groupValues?.let {
                            pathMappingRaw[it[2]] = it[1]
                        }
                    } else if (state == ParseState.ID) {
                        parseIDRegex.matchEntire(line)?.groupValues?.let {
                            idMappingRaw.getOrPut(it[2]) { mutableMapOf() }[it[6]] = it[3]
                        }
                    }
                }
            }
        }
        pathMapping = pathMappingRaw
        idMapping = idMappingRaw
    }

    fun originPath(path: String): String {
        return pathMapping[path] ?: path
    }

    fun originID(type: String, name: String): String {
        return idMapping[type]?.get(name) ?: name
    }
}