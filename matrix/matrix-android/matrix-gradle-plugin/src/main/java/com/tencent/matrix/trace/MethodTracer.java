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
import com.tencent.matrix.trace.item.TraceMethod;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Opcodes;
import org.objectweb.asm.commons.AdviceAdapter;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.Map;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.zip.ZipEntry;
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
    private final TraceBuildConfig mTraceConfig;
    private final HashMap<String, TraceMethod> mCollectedMethodMap;
    private final HashMap<String, String> mCollectedClassExtendMap;
    private final LinkedList<String> mApplicationClazz = new LinkedList<>();

    MethodTracer(TraceBuildConfig config, HashMap<String, TraceMethod> collectedMap, HashMap<String, String> collectedClassExtendMap) {
        this.mTraceConfig = config;
        this.mCollectedClassExtendMap = collectedClassExtendMap;
        this.mCollectedMethodMap = collectedMap;
        StringBuffer stringBuffer = new StringBuffer();
        for (String clazz : mCollectedClassExtendMap.keySet()) {
            if (mTraceConfig.isApplicationOrSubClass(clazz, mCollectedClassExtendMap)) {
                mApplicationClazz.add(clazz);
                stringBuffer.append(clazz).append(',');
            }
        }
        Log.i(TAG, "[MethodTracer] ApplicationClazz:%s", stringBuffer.toString());
    }


    public void trace(Map<File, File> srcFolderList, Map<File, File> dependencyJarList) {
        traceMethodFromSrc(srcFolderList);
        traceMethodFromJar(dependencyJarList);
    }

    private void traceMethodFromSrc(Map<File, File> srcMap) {
        if (null != srcMap) {
            for (Map.Entry<File, File> entry : srcMap.entrySet()) {
                innerTraceMethodFromSrc(entry.getKey(), entry.getValue());
            }
        }
    }

    private void traceMethodFromJar(Map<File, File> dependencyMap) {
        if (null != dependencyMap) {
            for (Map.Entry<File, File> entry : dependencyMap.entrySet()) {
                innerTraceMethodFromJar(entry.getKey(), entry.getValue());
            }
        }
    }

    private void innerTraceMethodFromSrc(File input, File output) {

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

                if (mTraceConfig.isNeedTraceClass(classFile.getName())) {
                    is = new FileInputStream(classFile);
                    ClassReader classReader = new ClassReader(is);
                    ClassWriter classWriter = new ClassWriter(ClassWriter.COMPUTE_MAXS);
                    ClassVisitor classVisitor = new TraceClassAdapter(Opcodes.ASM5, classWriter);
                    classReader.accept(classVisitor, ClassReader.EXPAND_FRAMES);
                    is.close();

                    if (output.isDirectory()) {
                        os = new FileOutputStream(changedFileOutput);
                    } else {
                        os = new FileOutputStream(output);
                    }
                    os.write(classWriter.toByteArray());
                    os.close();
                } else {
                    FileUtil.copyFileUsingStream(classFile, changedFileOutput);
                }
            } catch (Exception e) {
                Log.e(TAG, "[innerTraceMethodFromSrc] input:%s e:%s", input.getName(), e);
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

    private void innerTraceMethodFromJar(File input, File output) {
        ZipOutputStream zipOutputStream = null;
        ZipFile zipFile = null;
        try {
            zipOutputStream = new ZipOutputStream(new FileOutputStream(output));
            zipFile = new ZipFile(input);
            Enumeration<? extends ZipEntry> enumeration = zipFile.entries();
            while (enumeration.hasMoreElements()) {
                ZipEntry zipEntry = enumeration.nextElement();
                String zipEntryName = zipEntry.getName();
                if (mTraceConfig.isNeedTraceClass(zipEntryName)) {
                    InputStream inputStream = zipFile.getInputStream(zipEntry);
                    ClassReader classReader = new ClassReader(inputStream);
                    ClassWriter classWriter = new ClassWriter(ClassWriter.COMPUTE_MAXS);
                    ClassVisitor classVisitor = new TraceClassAdapter(Opcodes.ASM5, classWriter);
                    classReader.accept(classVisitor, ClassReader.EXPAND_FRAMES);
                    byte[] data = classWriter.toByteArray();
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
            Log.e(TAG, "[innerTraceMethodFromJar] input:%s e:%s", input.getName(), e);
            try {
                Files.copy(input.toPath(), output.toPath(), StandardCopyOption.REPLACE_EXISTING);
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
        private boolean isABSClass = false;
        private boolean isMethodBeatClass = false;
        private boolean hasWindowFocusMethod = false;

        TraceClassAdapter(int i, ClassVisitor classVisitor) {
            super(i, classVisitor);
        }

        @Override
        public void visit(int version, int access, String name, String signature, String superName, String[] interfaces) {
            super.visit(version, access, name, signature, superName, interfaces);
            this.className = name;
            if ((access & Opcodes.ACC_ABSTRACT) > 0 || (access & Opcodes.ACC_INTERFACE) > 0) {
                this.isABSClass = true;
            }
            if (mTraceConfig.isMethodBeatClass(className, mCollectedClassExtendMap)) {
                isMethodBeatClass = true;
            }
        }

        @Override
        public MethodVisitor visitMethod(int access, String name, String desc,
                                         String signature, String[] exceptions) {
            if (isABSClass) {
                return super.visitMethod(access, name, desc, signature, exceptions);
            } else {
                if (!hasWindowFocusMethod) {
                    hasWindowFocusMethod = mTraceConfig.isWindowFocusChangeMethod(name, desc);
                }
                MethodVisitor methodVisitor = cv.visitMethod(access, name, desc, signature, exceptions);
                return new TraceMethodAdapter(api, methodVisitor, access, name, desc, this.className,
                        hasWindowFocusMethod, isMethodBeatClass);
            }
        }


        @Override
        public void visitEnd() {
            TraceMethod traceMethod = TraceMethod.create(-1, Opcodes.ACC_PUBLIC, className,
                    TraceBuildConstants.MATRIX_TRACE_ON_WINDOW_FOCUS_METHOD, TraceBuildConstants.MATRIX_TRACE_ON_WINDOW_FOCUS_METHOD_ARGS);
            if (!hasWindowFocusMethod && mTraceConfig.isActivityOrSubClass(className, mCollectedClassExtendMap)
                    && mCollectedMethodMap.containsKey(traceMethod.getMethodName())) {
                insertWindowFocusChangeMethod(cv);
            }
            super.visitEnd();
        }
    }

    private class TraceMethodAdapter extends AdviceAdapter {

        private final String methodName;
        private final String name;
        private final String className;
        private final boolean hasWindowFocusMethod;
        private final boolean isMethodBeatClass;

        protected TraceMethodAdapter(int api, MethodVisitor mv, int access, String name, String desc, String className,
                                     boolean hasWindowFocusMethod, boolean isMethodBeatClass) {
            super(api, mv, access, name, desc);
            TraceMethod traceMethod = TraceMethod.create(0, access, className, name, desc);
            this.methodName = traceMethod.getMethodName();
            this.isMethodBeatClass = isMethodBeatClass;
            this.hasWindowFocusMethod = hasWindowFocusMethod;
            this.className = className;
            this.name = name;
        }

        @Override
        protected void onMethodEnter() {
            TraceMethod traceMethod = mCollectedMethodMap.get(methodName);
            if (traceMethod != null) {
                traceMethodCount.incrementAndGet();
                mv.visitLdcInsn(traceMethod.id);
                mv.visitMethodInsn(INVOKESTATIC, TraceBuildConstants.MATRIX_TRACE_CLASS, "i", "(I)V", false);
            }
        }

        // pass jni method trace, because its super method can cover it
        /*@Override
        public void visitMethodInsn(int opcode, String owner, String name, String desc, boolean itf) {
            String nativeMethodName = owner.replace("/", ".") + "." + name;
            TraceMethod traceMethod = mCollectedMethodMap.get(nativeMethodName);
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
            TraceMethod traceMethod = mCollectedMethodMap.get(methodName);
            if (traceMethod != null) {
                if (hasWindowFocusMethod && mTraceConfig.isActivityOrSubClass(className, mCollectedClassExtendMap)
                        && mCollectedMethodMap.containsKey(traceMethod.getMethodName())) {
                    TraceMethod windowFocusChangeMethod = TraceMethod.create(-1, Opcodes.ACC_PUBLIC, className,
                            TraceBuildConstants.MATRIX_TRACE_ON_WINDOW_FOCUS_METHOD, TraceBuildConstants.MATRIX_TRACE_ON_WINDOW_FOCUS_METHOD_ARGS);
                    if (windowFocusChangeMethod.equals(traceMethod)) {
                        traceWindowFocusChangeMethod(mv);
                    }
                }

                traceMethodCount.incrementAndGet();
                mv.visitLdcInsn(traceMethod.id);
                mv.visitMethodInsn(INVOKESTATIC, TraceBuildConstants.MATRIX_TRACE_CLASS, "o", "(I)V", false);
            }
        }
    }

    private void traceApplicationContext(MethodVisitor mv, TraceMethod traceMethod) {
        mv.visitVarInsn(Opcodes.ALOAD, 0);
        mv.visitLdcInsn(traceMethod.methodName);
        mv.visitLdcInsn(traceMethod.desc);
        mv.visitMethodInsn(Opcodes.INVOKESTATIC, TraceBuildConstants.MATRIX_TRACE_CLASS, "trace", "(Ljava/lang/Object;Ljava/lang/String;Ljava/lang/String;)V", false);
    }

    private void traceWindowFocusChangeMethod(MethodVisitor mv) {
        mv.visitVarInsn(Opcodes.ALOAD, 0);
        mv.visitVarInsn(Opcodes.ILOAD, 1);
        mv.visitMethodInsn(Opcodes.INVOKESTATIC, TraceBuildConstants.MATRIX_TRACE_CLASS, "at", "(Landroid/app/Activity;Z)V", false);
    }

    private void insertApplicationOnCreateMethod(ClassVisitor cv, String className) {
        MethodVisitor methodVisitor = cv.visitMethod(Opcodes.ACC_PUBLIC, TraceBuildConstants.MATRIX_TRACE_APPLICATION_ON_CREATE,
                TraceBuildConstants.MATRIX_TRACE_APPLICATION_ON_CREATE_ARGS, null, null);
        methodVisitor.visitCode();
        methodVisitor.visitVarInsn(Opcodes.ALOAD, 0);
        methodVisitor.visitMethodInsn(Opcodes.INVOKESPECIAL, TraceBuildConstants.MATRIX_TRACE_APPLICATION_CLASS, TraceBuildConstants.MATRIX_TRACE_APPLICATION_ON_CREATE,
                TraceBuildConstants.MATRIX_TRACE_APPLICATION_ON_CREATE_ARGS, false);
        TraceMethod applicationOnCreate = TraceMethod.create(-1, Opcodes.ACC_PUBLIC, className,
                TraceBuildConstants.MATRIX_TRACE_APPLICATION_ON_CREATE, TraceBuildConstants.MATRIX_TRACE_APPLICATION_ON_CREATE_ARGS);
        traceApplicationContext(methodVisitor, applicationOnCreate);
        methodVisitor.visitInsn(Opcodes.RETURN);
        methodVisitor.visitMaxs(1, 1);
        methodVisitor.visitEnd();
    }

    private void insertAttachBaseContextMethod(ClassVisitor cv, String className) {
        MethodVisitor methodVisitor = cv.visitMethod(Opcodes.ACC_PUBLIC, TraceBuildConstants.MATRIX_TRACE_ATTACH_BASE_CONTEXT,
                TraceBuildConstants.MATRIX_TRACE_ATTACH_BASE_CONTEXT_ARGS, null, null);
        methodVisitor.visitCode();
        methodVisitor.visitVarInsn(Opcodes.ALOAD, 0);
        methodVisitor.visitVarInsn(Opcodes.ALOAD, 1);
        methodVisitor.visitMethodInsn(Opcodes.INVOKESPECIAL, TraceBuildConstants.MATRIX_TRACE_APPLICATION_CLASS, TraceBuildConstants.MATRIX_TRACE_ATTACH_BASE_CONTEXT,
                TraceBuildConstants.MATRIX_TRACE_ATTACH_BASE_CONTEXT_ARGS, false);
        TraceMethod attachBaseContextMethod = TraceMethod.create(-1, Opcodes.ACC_PUBLIC, className,
                TraceBuildConstants.MATRIX_TRACE_ATTACH_BASE_CONTEXT, TraceBuildConstants.MATRIX_TRACE_ATTACH_BASE_CONTEXT_ARGS);
        traceApplicationContext(methodVisitor, attachBaseContextMethod);
        methodVisitor.visitInsn(Opcodes.RETURN);
        methodVisitor.visitMaxs(2, 2);
        methodVisitor.visitEnd();
    }

    private void insertWindowFocusChangeMethod(ClassVisitor cv) {
        MethodVisitor methodVisitor = cv.visitMethod(Opcodes.ACC_PUBLIC, TraceBuildConstants.MATRIX_TRACE_ON_WINDOW_FOCUS_METHOD,
                TraceBuildConstants.MATRIX_TRACE_ON_WINDOW_FOCUS_METHOD_ARGS, null, null);
        methodVisitor.visitCode();
        methodVisitor.visitVarInsn(Opcodes.ALOAD, 0);
        methodVisitor.visitVarInsn(Opcodes.ILOAD, 1);
        methodVisitor.visitMethodInsn(Opcodes.INVOKESPECIAL, TraceBuildConstants.MATRIX_TRACE_ACTIVITY_CLASS, TraceBuildConstants.MATRIX_TRACE_ON_WINDOW_FOCUS_METHOD,
                TraceBuildConstants.MATRIX_TRACE_ON_WINDOW_FOCUS_METHOD_ARGS, false);
        traceWindowFocusChangeMethod(methodVisitor);
        methodVisitor.visitInsn(Opcodes.RETURN);
        methodVisitor.visitMaxs(2, 2);
        methodVisitor.visitEnd();

    }

}
