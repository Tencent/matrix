package com.tencent.matrix.plugin.compat

enum class AGPVersion(
        val value: String
) {
    // Versions that we cared about.
    AGP_3_5_0("3.5.0"),
    AGP_3_6_0("3.6.0"),
    AGP_4_0_0("4.0.0"),
    AGP_4_1_0("4.1.0"),
}

class VersionsCompat {

    companion object {

        @Suppress("DEPRECATION")
        private fun initCurrentAndroidGradlePluginVersion(): String {
            var newVersion = false
            try {
                if (Class.forName("com.android.Version") != null) {
                    newVersion = true
                }
            } catch(t: Throwable) {
            }

            return if (newVersion) {
                com.android.Version.ANDROID_GRADLE_PLUGIN_VERSION
            } else {
                com.android.builder.model.Version.ANDROID_GRADLE_PLUGIN_VERSION
            }
        }

        var androidGradlePluginVersion: String = initCurrentAndroidGradlePluginVersion()

        var lessThan = {agpVersion: AGPVersion -> androidGradlePluginVersion < agpVersion.value }

        var greatThanOrEqual = {agpVersion: AGPVersion -> androidGradlePluginVersion >= agpVersion.value }

        var equalTo = {agpVersion: AGPVersion -> androidGradlePluginVersion.compareTo(agpVersion.value) == 0}
    }

}