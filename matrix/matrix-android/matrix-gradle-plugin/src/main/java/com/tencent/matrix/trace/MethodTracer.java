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

package com.tencent.matrix.trace;

import com.tencent.matrix.javalib.util.FileUtil;
import com.tencent.matrix.javalib.util.Log;
import com.tencent.matrix.plugin.compat.AgpCompat;
import com.tencent.matrix.trace.item.TraceMethod;
import com.tencent.matrix.trace.retrace.MappingCollector;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Opcodes;
import org.objectweb.asm.commons.AdviceAdapter;
import org.objectweb.asm.util.CheckClassAdapter;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Future;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.zip.ZipEntry;
import java.util.zip.ZipException;
import java.util.zip.ZipFile;
import java.util.zip.ZipOutputStream;

/**
 * Created by caichongyang on 2017/6/4.
 * <p>
 * This class hooks all collected methods in oder to trace method in/out.
 * </p>
 */

public class MethodTracer {

    private static final String TAG = "Matrix.MethodTracer";
    private static AtomicInteger traceMethodCount = new AtomicInteger();
    private final Configuration configuration;
    private final ConcurrentHashMap<String, TraceMethod> collectedMethodMap;
    private final ConcurrentHashMap<String, String> collectedClassExtendMap;
    private final ExecutorService executor;
    private MappingCollector mappingCollector;

    private volatile boolean traceError = false;

    public MethodTracer(ExecutorService executor, MappingCollector mappingCollector, Configuration config, ConcurrentHashMap<String, TraceMethod> collectedMap, ConcurrentHashMap<String, String> collectedClassExtendMap) {
        this.configuration = config;
        this.mappingCollector = mappingCollector;
        this.executor = executor;
        this.collectedClassExtendMap = collectedClassExtendMap;
        this.collectedMethodMap = collectedMap;
    }

    public void trace(Map<File, File> srcFolderList, Map<File, File> dependencyJarList, ClassLoader classLoader, boolean ignoreCheckClass) throws ExecutionException, InterruptedException {
        List<Future> futures = new LinkedList<>();
        traceMethodFromSrc(srcFolderList, futures, classLoader, ignoreCheckClass);
        traceMethodFromJar(dependencyJarList, futures, classLoader, ignoreCheckClass);
        for (Future future : futures) {
            future.get();
        }
        if (traceError) {
            throw new IllegalArgumentException("something wrong with trace, see detail log before");
        }
        futures.clear();
    }

    private void traceMethodFromSrc(Map<File, File> srcMap, List<Future> futures, final ClassLoader classLoader, final boolean skipCheckClass) {
        if (null != srcMap) {
            for (Map.Entry<File, File> entry : srcMap.entrySet()) {
                futures.add(executor.submit(new Runnable() {
                    @Override
                    public void run() {
                        innerTraceMethodFromSrc(entry.getKey(), entry.getValue(), classLoader, skipCheckClass);
                    }
                }));
            }
        }
    }

    private void traceMethodFromJar(Map<File, File> dependencyMap, List<Future> futures, final ClassLoader classLoader, final boolean skipCheckClass) {
        if (null != dependencyMap) {
            for (Map.Entry<File, File> entry : dependencyMap.entrySet()) {
                futures.add(executor.submit(new Runnable() {
                    @Override
                    public void run() {
                        innerTraceMethodFromJar(entry.getKey(), entry.getValue(), classLoader, skipCheckClass);
                    }
                }));
            }
        }
    }

    private void innerTraceMethodFromSrc(File input, File output, ClassLoader classLoader, boolean ignoreCheckClass) {

        ArrayList<File> classFileList = new ArrayList<>();
        if (input.isDirectory()) {
            listClassFiles(classFileList, input);
        } else {
            classFileList.add(input);
        }

        for (File classFile : classFileList) {
            InputStream is = null;
            FileOutputStream os = null;
            try {
                final String changedFileInputFullPath = classFile.getAbsolutePath();
                final File changedFileOutput = new File(changedFileInputFullPath.replace(input.getAbsolutePath(), output.getAbsolutePath()));
                if (!changedFileOutput.exists()) {
                    changedFileOutput.getParentFile().mkdirs();
                }
                changedFileOutput.createNewFile();

                if (MethodCollector.isNeedTraceFile(classFile.getName())) {

                    is = new FileInputStream(classFile);
                    ClassReader classReader = new ClassReader(is);
                    ClassWriter classWriter = new TraceClassWriter(ClassWriter.COMPUTE_FRAMES, classLoader);
                    ClassVisitor classVisitor = new TraceClassAdapter(AgpCompat.getAsmApi(), classWriter);
                    classReader.accept(classVisitor, ClassReader.EXPAND_FRAMES);
                    is.close();

                    byte[] data = classWriter.toByteArray();

                    if (!ignoreCheckClass) {
                        try {
                            ClassReader cr = new ClassReader(data);
                            ClassWriter cw = new ClassWriter(0);
                            ClassVisitor check = new CheckClassAdapter(cw);
                            cr.accept(check, ClassReader.EXPAND_FRAMES);
                        } catch (Throwable e) {
                            System.err.println("trace output ERROR : " + e.getMessage() + ", " + classFile);
                            traceError = true;
                        }
                    }

                    if (output.isDirectory()) {
                        os = new FileOutputStream(changedFileOutput);
                    } else {
                        os = new FileOutputStream(output);
                    }
                    os.write(data);
                    os.close();
                } else {
                    FileUtil.copyFileUsingStream(classFile, changedFileOutput);
                }
            } catch (Exception e) {
                Log.e(TAG, "[innerTraceMethodFromSrc] input:%s e:%s", input.getName(), e.getMessage());
                try {
                    Files.copy(input.toPath(), output.toPath(), StandardCopyOption.REPLACE_EXISTING);
                } catch (Exception e1) {
                    e1.printStackTrace();
                }
            } finally {
                try {
                    is.close();
                    os.close();
                } catch (Exception e) {
                    // ignore
                }
            }
        }
    }

    private void innerTraceMethodFromJar(File input, File output, final ClassLoader classLoader, boolean skipCheckClass) {
        ZipOutputStream zipOutputStream = null;
        ZipFile zipFile = null;
        try {
            zipOutputStream = new ZipOutputStream(new FileOutputStream(output));
            zipFile = new ZipFile(input);
            Enumeration<? extends ZipEntry> enumeration = zipFile.entries();
            while (enumeration.hasMoreElements()) {
                ZipEntry zipEntry = enumeration.nextElement();
                String zipEntryName = zipEntry.getName();
                if (MethodCollector.isNeedTraceFile(zipEntryName)) {

                    InputStream inputStream = zipFile.getInputStream(zipEntry);
                    ClassReader classReader = new ClassReader(inputStream);
                    ClassWriter classWriter = new TraceClassWriter(ClassWriter.COMPUTE_FRAMES, classLoader);
                    ClassVisitor classVisitor = new TraceClassAdapter(AgpCompat.getAsmApi(), classWriter);
                    classReader.accept(classVisitor, ClassReader.EXPAND_FRAMES);
                    byte[] data = classWriter.toByteArray();
//
                    if (!skipCheckClass) {
                        try {
                            ClassReader r = new ClassReader(data);
                            ClassWriter w = new ClassWriter(0);
                            ClassVisitor v = new CheckClassAdapter(w);
                            r.accept(v, ClassReader.EXPAND_FRAMES);
                        } catch (Throwable e) {
                            System.err.println("trace jar output ERROR: " + e.getMessage() + ", " + zipEntryName);
//                        e.printStackTrace();
                            traceError = true;
                        }
                    }

                    InputStream byteArrayInputStream = new ByteArrayInputStream(data);
                    ZipEntry newZipEntry = new ZipEntry(zipEntryName);
                    FileUtil.addZipEntry(zipOutputStream, newZipEntry, byteArrayInputStream);
                } else {
                    InputStream inputStream = zipFile.getInputStream(zipEntry);
                    ZipEntry newZipEntry = new ZipEntry(zipEntryName);
                    FileUtil.addZipEntry(zipOutputStream, newZipEntry, inputStream);
                }
            }
        } catch (Exception e) {
            Log.e(TAG, "[innerTraceMethodFromJar] input:%s output:%s e:%s", input, output, e.getMessage());
            if (e instanceof ZipException) {
                e.printStackTrace();
            }
            try {
                if (input.length() > 0) {
                    Files.copy(input.toPath(), output.toPath(), StandardCopyOption.REPLACE_EXISTING);
                } else {
                    Log.e(TAG, "[innerTraceMethodFromJar] input:%s is empty", input);
                }
            } catch (Exception e1) {
                e1.printStackTrace();
            }
        } finally {
            try {
                if (zipOutputStream != null) {
                    zipOutputStream.finish();
                    zipOutputStream.flush();
                    zipOutputStream.close();
                }
                if (zipFile != null) {
                    zipFile.close();
                }
            } catch (Exception e) {
                Log.e(TAG, "close stream err!");
            }
        }
    }

    private void listClassFiles(ArrayList<File> classFiles, File folder) {
        File[] files = folder.listFiles();
        if (null == files) {
            Log.e(TAG, "[listClassFiles] files is null! %s", folder.getAbsolutePath());
            return;
        }
        for (File file : files) {
            if (file == null) {
                continue;
            }
            if (file.isDirectory()) {
                listClassFiles(classFiles, file);
            } else {
                if (null != file && file.isFile()) {
                    classFiles.add(file);
                }

            }
        }
    }

    private class TraceClassAdapter extends ClassVisitor {

        private String className;
        private String superName;
        private boolean isABSClass = false;
        private boolean hasWindowFocusMethod = false;
        private boolean isActivityOrSubClass;
        private boolean isNeedTrace;

        TraceClassAdapter(int i, ClassVisitor classVisitor) {
            super(i, classVisitor);
        }

        @Override
        public void visit(int version, int access, String name, String signature, String superName, String[] interfaces) {
            super.visit(version, access, name, signature, superName, interfaces);
            this.className = name;
            this.superName = superName;
            this.isActivityOrSubClass = isActivityOrSubClass(className, collectedClassExtendMap);
            this.isNeedTrace = MethodCollector.isNeedTrace(configuration, className, mappingCollector);
            if ((access & Opcodes.ACC_ABSTRACT) > 0 || (access & Opcodes.ACC_INTERFACE) > 0) {
                this.isABSClass = true;
            }

        }

        @Override
        public MethodVisitor visitMethod(int access, String name, String desc,
                                         String signature, String[] exceptions) {
            if (!hasWindowFocusMethod) {
                hasWindowFocusMethod = MethodCollector.isWindowFocusChangeMethod(name, desc);
            }
            if (isABSClass) {
                return super.visitMethod(access, name, desc, signature, exceptions);
            } else {
                MethodVisitor methodVisitor = cv.visitMethod(access, name, desc, signature, exceptions);
                return new TraceMethodAdapter(api, methodVisitor, access, name, desc, this.className,
                        hasWindowFocusMethod, isActivityOrSubClass, isNeedTrace);
            }
        }


        @Override
        public void visitEnd() {
            if (!hasWindowFocusMethod && isActivityOrSubClass && isNeedTrace) {
                insertWindowFocusChangeMethod(cv, className, superName);
            }
            super.visitEnd();
        }
    }

    private class TraceMethodAdapter extends AdviceAdapter {

        private final String methodName;
        private final String name;
        private final String className;
        private final boolean hasWindowFocusMethod;
        private final boolean isNeedTrace;
        private final boolean isActivityOrSubClass;

        protected TraceMethodAdapter(int api, MethodVisitor mv, int access, String name, String desc, String className,
                                     boolean hasWindowFocusMethod, boolean isActivityOrSubClass, boolean isNeedTrace) {
            super(api, mv, access, name, desc);
            TraceMethod traceMethod = TraceMethod.create(0, access, className, name, desc);
            this.methodName = traceMethod.getMethodName();
            this.hasWindowFocusMethod = hasWindowFocusMethod;
            this.className = className;
            this.name = name;
            this.isActivityOrSubClass = isActivityOrSubClass;
            this.isNeedTrace = isNeedTrace;

        }

        @Override
        protected void onMethodEnter() {
            TraceMethod traceMethod = collectedMethodMap.get(methodName);
            if (traceMethod != null) {
                traceMethodCount.incrementAndGet();
                mv.visitLdcInsn(traceMethod.id);
                mv.visitMethodInsn(INVOKESTATIC, TraceBuildConstants.MATRIX_TRACE_CLASS, "i", "(I)V", false);

                if (checkNeedTraceWindowFocusChangeMethod(traceMethod)) {
                    traceWindowFocusChangeMethod(mv, className);
                }
            }
        }

        // pass jni method trace, because its super method can cover it
        /*@Override
        public void visitMethodInsn(int opcode, String owner, String name, String desc, boolean itf) {
            String nativeMethodName = owner.replace("/", ".") + "." + name;
            TraceMethod traceMethod = collectedMethodMap.get(nativeMethodName);
            if (traceMethod != null && traceMethod.isNativeMethod()) {
                traceMethodCount.incrementAndGet();
                mv.visitLdcInsn(traceMethod.id);
                mv.visitMethodInsn(INVOKESTATIC, TraceBuildConstants.MATRIX_TRACE_CLASS, "i", "(I)V", false);
            }
            super.visitMethodInsn(opcode, owner, name, desc, itf);
            if (traceMethod != null && traceMethod.isNativeMethod()) {
                traceMethodCount.incrementAndGet();
                mv.visitLdcInsn(traceMethod.id);
                mv.visitMethodInsn(INVOKESTATIC, TraceBuildConstants.MATRIX_TRACE_CLASS, "o", "(I)V", false);
            }
        }*/

        @Override
        protected void onMethodExit(int opcode) {
            TraceMethod traceMethod = collectedMethodMap.get(methodName);
            if (traceMethod != null) {
                traceMethodCount.incrementAndGet();
                mv.visitLdcInsn(traceMethod.id);
                mv.visitMethodInsn(INVOKESTATIC, TraceBuildConstants.MATRIX_TRACE_CLASS, "o", "(I)V", false);
            }
        }

        private boolean checkNeedTraceWindowFocusChangeMethod(TraceMethod traceMethod) {
            if (hasWindowFocusMethod && isActivityOrSubClass && isNeedTrace) {
                TraceMethod windowFocusChangeMethod = TraceMethod.create(-1, Opcodes.ACC_PUBLIC, className,
                        TraceBuildConstants.MATRIX_TRACE_ON_WINDOW_FOCUS_METHOD, TraceBuildConstants.MATRIX_TRACE_ON_WINDOW_FOCUS_METHOD_ARGS);
                if (windowFocusChangeMethod.equals(traceMethod)) {
                    return true;
                }
            }
            return false;
        }
    }

    private boolean isActivityOrSubClass(String className, ConcurrentHashMap<String, String> mCollectedClassExtendMap) {
        className = className.replace(".", "/");
        boolean isActivity = className.equals(TraceBuildConstants.MATRIX_TRACE_ACTIVITY_CLASS)
                || className.equals(TraceBuildConstants.MATRIX_TRACE_V4_ACTIVITY_CLASS)
                || className.equals(TraceBuildConstants.MATRIX_TRACE_V7_ACTIVITY_CLASS)
                || className.equals(TraceBuildConstants.MATRIX_TRACE_ANDROIDX_ACTIVITY_CLASS);
        if (isActivity) {
            return true;
        } else {
            if (!mCollectedClassExtendMap.containsKey(className)) {
                return false;
            } else {
                return isActivityOrSubClass(mCollectedClassExtendMap.get(className), mCollectedClassExtendMap);
            }
        }
    }

    private void traceWindowFocusChangeMethod(MethodVisitor mv, String classname) {
        mv.visitVarInsn(Opcodes.ALOAD, 0);
        mv.visitVarInsn(Opcodes.ILOAD, 1);
        mv.visitMethodInsn(Opcodes.INVOKESTATIC, TraceBuildConstants.MATRIX_TRACE_CLASS, "at", "(Landroid/app/Activity;Z)V", false);
    }

    private void insertWindowFocusChangeMethod(ClassVisitor cv, String classname, String superClassName) {
        MethodVisitor methodVisitor = cv.visitMethod(Opcodes.ACC_PUBLIC, TraceBuildConstants.MATRIX_TRACE_ON_WINDOW_FOCUS_METHOD,
                TraceBuildConstants.MATRIX_TRACE_ON_WINDOW_FOCUS_METHOD_ARGS, null, null);
        methodVisitor.visitCode();
        methodVisitor.visitVarInsn(Opcodes.ALOAD, 0);
        methodVisitor.visitVarInsn(Opcodes.ILOAD, 1);
        methodVisitor.visitMethodInsn(Opcodes.INVOKESPECIAL, superClassName, TraceBuildConstants.MATRIX_TRACE_ON_WINDOW_FOCUS_METHOD,
                TraceBuildConstants.MATRIX_TRACE_ON_WINDOW_FOCUS_METHOD_ARGS, false);
        traceWindowFocusChangeMethod(methodVisitor, classname);
        methodVisitor.visitInsn(Opcodes.RETURN);
        methodVisitor.visitMaxs(2, 2);
        methodVisitor.visitEnd();

    }

}
