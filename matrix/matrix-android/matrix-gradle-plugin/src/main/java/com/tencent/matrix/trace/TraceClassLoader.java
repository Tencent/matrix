package com.tencent.matrix.trace;

import com.android.build.gradle.AppExtension;
import com.android.build.gradle.BaseExtension;
import com.android.build.gradle.LibraryExtension;
import com.google.common.collect.ImmutableList;

import org.gradle.api.Project;

import java.io.File;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.Collection;

/**
 * Created by habbyge on 2019/4/24.
 */
public class TraceClassLoader {

    public static URLClassLoader getClassLoader(Project project, Collection<File> inputFiles)
            throws MalformedURLException {

        ImmutableList.Builder<URL> urls = new ImmutableList.Builder<>();
        File androidJar = getAndroidJar(project);
        if (androidJar != null) {
            urls.add(androidJar.toURI().toURL());
        }

        for (File inputFile : inputFiles) {
            urls.add(inputFile.toURI().toURL());
        }

//        for (TransformInput inputs : Iterables.concat(invocation.getInputs(), invocation.getReferencedInputs())) {
//            for (DirectoryInput directoryInput : inputs.getDirectoryInputs()) {
//                if (directoryInput.getFile().isDirectory()) {
//                    urls.add(directoryInput.getFile().toURI().toURL());
//                }
//            }
//            for (JarInput jarInput : inputs.getJarInputs()) {
//                if (jarInput.getFile().isFile()) {
//                    urls.add(jarInput.getFile().toURI().toURL());
//                }
//            }
//        }

        ImmutableList<URL> urlImmutableList = urls.build();
        URL[] classLoaderUrls = urlImmutableList.toArray(new URL[urlImmutableList.size()]);
        return new URLClassLoader(classLoaderUrls);
    }

    private static File getAndroidJar(Project project) {
        BaseExtension extension = null;
        if (project.getPlugins().hasPlugin("com.android.application")) {
            extension = project.getExtensions().findByType(AppExtension.class);
        } else if (project.getPlugins().hasPlugin("com.android.library")) {
            extension = project.getExtensions().findByType(LibraryExtension.class);
        }
        if (extension == null) {
            return null;
        }

        String sdkDirectory = extension.getSdkDirectory().getAbsolutePath();
        String compileSdkVersion = extension.getCompileSdkVersion();
        sdkDirectory = sdkDirectory + File.separator + "platforms" + File.separator;
        String androidJarPath = sdkDirectory + compileSdkVersion + File.separator + "android.jar";
        File androidJar = new File(androidJarPath);
        return androidJar.exists() ? androidJar : null;
    }
}
