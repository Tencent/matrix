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

import com.tencent.matrix.javalib.util.Log;
import com.tencent.matrix.javalib.util.Util;
import com.tencent.matrix.trace.item.TraceMethod;
import com.tencent.matrix.trace.retrace.MappingCollector;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Opcodes;
import org.objectweb.asm.tree.AbstractInsnNode;
import org.objectweb.asm.tree.MethodNode;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.io.Writer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.List;
import java.util.ListIterator;
import java.util.Map;
import java.util.Scanner;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

/**
 * Created by caichongyang on 2017/6/3.
 * <p>
 * collect all methods need to hook,and export the trace mapping<Method ID,Method Name> file.
 * </p>
 */

public class MethodCollector {

    private static final String TAG = "Matrix.MethodCollector";
    private final HashMap<String, TraceMethod> mCollectedMethodMap;
    private final HashMap<String, TraceMethod> mCollectedIgnoreMethodMap;
    private final HashMap<String, TraceMethod> mCollectedBlackMethodMap;

    private final HashMap<String, String> mCollectedClassExtendMap;

    private final TraceBuildConfig mTraceConfig;
    private final AtomicInteger mMethodId = new AtomicInteger(0);
    private final MappingCollector mMappingCollector;
    private int mIncrementCount, mIgnoreCount;
    private static final int METHOD_ID_MAX = 0xFFFFF;
    public static final int METHOD_ID_DISPATCH = METHOD_ID_MAX - 1;

    public MethodCollector(TraceBuildConfig config, MappingCollector mappingCollector) {
        this.mCollectedMethodMap = new HashMap<>();
        this.mCollectedClassExtendMap = new HashMap<>();
        this.mCollectedIgnoreMethodMap = new HashMap<>();
        this.mCollectedBlackMethodMap = new HashMap<>();
        this.mTraceConfig = config;
        this.mMappingCollector = mappingCollector;
    }

    public HashMap<String, String> getCollectedClassExtendMap() {
        return mCollectedClassExtendMap;
    }

    public HashMap collect(List<File> srcFolderList, List<File> dependencyJarList) {
        mTraceConfig.parseBlackFile(mMappingCollector);

        File originMethodMapFile = new File(mTraceConfig.getBaseMethodMap());
        getMethodFromBaseMethod(originMethodMapFile);
        Log.i(TAG, "[collect] %s method from %s", mCollectedMethodMap.size(), mTraceConfig.getBaseMethodMap());
        retraceMethodMap(mMappingCollector, mCollectedMethodMap);

        collectMethodFromSrc(srcFolderList, true);
        collectMethodFromJar(dependencyJarList, true);
        collectMethodFromSrc(srcFolderList, false);
        collectMethodFromJar(dependencyJarList, false);
        Log.i(TAG, "[collect] incrementCount:%s ignoreMethodCount:%s", mIncrementCount, mIgnoreCount);

        saveCollectedMethod(mMappingCollector);
        saveIgnoreCollectedMethod(mMappingCollector);

        return mCollectedMethodMap;

    }

    private void retraceMethodMap(MappingCollector processor, HashMap<String, TraceMethod> methodMap) {
        if (null == processor || null == methodMap) {
            return;
        }
        HashMap<String, TraceMethod> retraceMethodMap = new HashMap<>(methodMap.size());
        for (Map.Entry<String, TraceMethod> entry : methodMap.entrySet()) {
            TraceMethod traceMethod = entry.getValue();
            traceMethod.proguard(processor);
            retraceMethodMap.put(traceMethod.getMethodName(), traceMethod);
        }
        methodMap.clear();
        methodMap.putAll(retraceMethodMap);
        retraceMethodMap.clear();

    }

    private void saveIgnoreCollectedMethod(MappingCollector mappingCollector) {
        File methodMapFile = new File(mTraceConfig.getIgnoreMethodMapFile());
        if (!methodMapFile.getParentFile().exists()) {
            methodMapFile.getParentFile().mkdirs();
        }
        List<TraceMethod> ignoreMethodList = new ArrayList<>();
        ignoreMethodList.addAll(mCollectedIgnoreMethodMap.values());
        Log.i(TAG, "[saveIgnoreCollectedMethod] size:%s path:%s", mCollectedIgnoreMethodMap.size(), methodMapFile.getAbsolutePath());

        List<TraceMethod> blackMethodList = new ArrayList<>();
        blackMethodList.addAll(mCollectedBlackMethodMap.values());
        Log.i(TAG, "[saveIgnoreBlackMethod] size:%s path:%s", mCollectedBlackMethodMap.size(), methodMapFile.getAbsolutePath());

        Collections.sort(ignoreMethodList, new Comparator<TraceMethod>() {
            @Override
            public int compare(TraceMethod o1, TraceMethod o2) {
                return o1.className.compareTo(o2.className);
            }
        });

        Collections.sort(blackMethodList, new Comparator<TraceMethod>() {
            @Override
            public int compare(TraceMethod o1, TraceMethod o2) {
                return o1.className.compareTo(o2.className);
            }
        });

        PrintWriter pw = null;
        try {
            FileOutputStream fileOutputStream = new FileOutputStream(methodMapFile, false);
            Writer w = new OutputStreamWriter(fileOutputStream, "UTF-8");
            pw = new PrintWriter(w);
            pw.println("ignore methods:");
            for (TraceMethod traceMethod : ignoreMethodList) {
                traceMethod.revert(mappingCollector);
                pw.println(traceMethod.toIgnoreString());
            }
            pw.println("");
            pw.println("black methods:");
            for (TraceMethod traceMethod : blackMethodList) {
                traceMethod.revert(mappingCollector);
                pw.println(traceMethod.toIgnoreString());
            }

        } catch (Exception e) {
            Log.e(TAG, "write method map Exception:%s", e.getMessage());
            e.printStackTrace();
        } finally {
            if (pw != null) {
                pw.flush();
                pw.close();
            }
        }
    }


    private void saveCollectedMethod(MappingCollector mappingCollector) {
        File methodMapFile = new File(mTraceConfig.getMethodMapFile());
        if (!methodMapFile.getParentFile().exists()) {
            methodMapFile.getParentFile().mkdirs();
        }
        List<TraceMethod> methodList = new ArrayList<>();

        TraceMethod extra = TraceMethod.create(METHOD_ID_DISPATCH, Opcodes.ACC_PUBLIC, "android.os.Handler",
                "dispatchMessage", "(Landroid.os.Message;)V");
        mCollectedMethodMap.put(extra.getMethodName(), extra);

        methodList.addAll(mCollectedMethodMap.values());
        Log.i(TAG, "[saveCollectedMethod] size:%s path:%s", mCollectedMethodMap.size(), methodMapFile.getAbsolutePath());

        Collections.sort(methodList, new Comparator<TraceMethod>() {
            @Override
            public int compare(TraceMethod o1, TraceMethod o2) {
                return o1.id - o2.id;
            }
        });

        PrintWriter pw = null;
        try {
            FileOutputStream fileOutputStream = new FileOutputStream(methodMapFile, false);
            Writer w = new OutputStreamWriter(fileOutputStream, "UTF-8");
            pw = new PrintWriter(w);
            for (TraceMethod traceMethod : methodList) {
                traceMethod.revert(mappingCollector);
                pw.println(traceMethod.toString());
            }
        } catch (Exception e) {
            Log.e(TAG, "write method map Exception:%s", e.getMessage());
            e.printStackTrace();
        } finally {
            if (pw != null) {
                pw.flush();
                pw.close();
            }
        }
    }


    private void collectMethodFromSrc(List<File> srcFolderList, boolean isSingle) {
        if (null != srcFolderList) {
            for (File srcFile : srcFolderList) {
                innerCollectMethodFromSrc(srcFile, isSingle);
            }
        }
    }


    private void collectMethodFromJar(List<File> dependencyJarList, boolean isSingle) {
        if (null != dependencyJarList) {
            for (File jarFile : dependencyJarList) {
                innerCollectMethodFromJar(jarFile, isSingle);
            }
        }

    }

    private void getMethodFromBaseMethod(File baseMethodFile) {
        if (!baseMethodFile.exists()) {
            Log.w(TAG, "[getMethodFromBaseMethod] not exist!%s", baseMethodFile.getAbsolutePath());
            return;
        }
        Scanner fileReader = null;
        try {
            fileReader = new Scanner(baseMethodFile, "UTF-8");
            while (fileReader.hasNext()) {
                String nextLine = fileReader.nextLine();
                if (!Util.isNullOrNil(nextLine)) {
                    nextLine = nextLine.trim();
                    if (nextLine.startsWith("#")) {
                        Log.i("[getMethodFromBaseMethod] comment %s", nextLine);
                        continue;
                    }
                    String[] fields = nextLine.split(",");
                    TraceMethod traceMethod = new TraceMethod();
                    traceMethod.id = Integer.parseInt(fields[0]);
                    traceMethod.accessFlag = Integer.parseInt(fields[1]);
                    String[] methodField = fields[2].split(" ");
                    traceMethod.className = methodField[0].replace("/", ".");
                    traceMethod.methodName = methodField[1];
                    if (methodField.length > 2) {
                        traceMethod.desc = methodField[2].replace("/", ".");
                    }
                    if (mMethodId.get() < traceMethod.id && traceMethod.id != METHOD_ID_DISPATCH) {
                        mMethodId.set(traceMethod.id);
                    }
                    if (mTraceConfig.isNeedTrace(traceMethod.className, mMappingCollector)) {
                        mCollectedMethodMap.put(traceMethod.getMethodName(), traceMethod);
                    }
                }
            }
        } catch (Exception e) {
            Log.e(TAG, "[getMethodFromBaseMethod] err!");
        } finally {
            if (fileReader != null) {
                fileReader.close();
            }
        }
    }


    private void innerCollectMethodFromSrc(File srcFile, boolean isSingle) {
        ArrayList<File> classFileList = new ArrayList<>();
        if (srcFile.isDirectory()) {
            listClassFiles(classFileList, srcFile);
        } else {
            classFileList.add(srcFile);
        }

        for (File classFile : classFileList) {
            InputStream is = null;
            try {
                is = new FileInputStream(classFile);
                ClassReader classReader = new ClassReader(is);
                ClassWriter classWriter = new ClassWriter(ClassWriter.COMPUTE_MAXS);
                ClassVisitor visitor;
                if (isSingle) {
                    visitor = new SingleTraceClassAdapter(Opcodes.ASM5, classWriter);
                } else {
                    visitor = new TraceClassAdapter(Opcodes.ASM5, classWriter);
                }
                classReader.accept(visitor, 0);
            } catch (Exception e) {
                e.printStackTrace();
            } finally {
                try {
                    is.close();
                } catch (Exception e) {
                    // ignore
                }
            }
        }
    }

    private void innerCollectMethodFromJar(File fromJar, boolean isSingle) {
        ZipFile zipFile = null;

        try {
            zipFile = new ZipFile(fromJar);
            Enumeration<? extends ZipEntry> enumeration = zipFile.entries();
            while (enumeration.hasMoreElements()) {
                ZipEntry zipEntry = enumeration.nextElement();
                String zipEntryName = zipEntry.getName();
                if (mTraceConfig.isNeedTraceClass(zipEntryName)) {
                    InputStream inputStream = zipFile.getInputStream(zipEntry);
                    ClassReader classReader = new ClassReader(inputStream);
                    ClassWriter classWriter = new ClassWriter(ClassWriter.COMPUTE_MAXS);
                    ClassVisitor visitor;
                    if (isSingle) {
                        visitor = new SingleTraceClassAdapter(Opcodes.ASM5, classWriter);
                    } else {
                        visitor = new TraceClassAdapter(Opcodes.ASM5, classWriter);
                    }
                    classReader.accept(visitor, 0);
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        } finally {
            try {
                zipFile.close();
            } catch (Exception e) {
                Log.e(TAG, "close stream err! fromJar:%s", fromJar.getAbsolutePath());
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
                if (null != file && file.isFile() && mTraceConfig.isNeedTraceClass(file.getName())) {
                    classFiles.add(file);
                }

            }
        }
    }

    private class SingleTraceClassAdapter extends ClassVisitor {

        SingleTraceClassAdapter(int i, ClassVisitor classVisitor) {
            super(i, classVisitor);
        }

        @Override
        public void visit(int version, int access, String name, String signature, String superName, String[] interfaces) {
            super.visit(version, access, name, signature, superName, interfaces);
            mCollectedClassExtendMap.put(name, superName);
        }

    }


    private class TraceClassAdapter extends ClassVisitor {
        private String className;
        private boolean isABSClass = false;
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
            mCollectedClassExtendMap.put(className, superName);

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
                return new CollectMethodNode(className, access, name, desc, signature, exceptions);
            }
        }

        @Override
        public void visitEnd() {
            super.visitEnd();
            // collect Activity#onWindowFocusChange
            TraceMethod traceMethod = TraceMethod.create(-1, Opcodes.ACC_PUBLIC, className,
                    TraceBuildConstants.MATRIX_TRACE_ON_WINDOW_FOCUS_METHOD, TraceBuildConstants.MATRIX_TRACE_ON_WINDOW_FOCUS_METHOD_ARGS);
            if (!hasWindowFocusMethod && mTraceConfig.isActivityOrSubClass(className, mCollectedClassExtendMap) && mTraceConfig.isNeedTrace(traceMethod.className, mMappingCollector)) {
                mCollectedMethodMap.put(traceMethod.getMethodName(), traceMethod);
            }
        }

    }

    private class CollectMethodNode extends MethodNode {
        private String className;
        private boolean isConstructor;


        CollectMethodNode(String className, int access, String name, String desc,
                          String signature, String[] exceptions) {
            super(Opcodes.ASM5, access, name, desc, signature, exceptions);
            this.className = className;
        }

        @Override
        public void visitEnd() {
            super.visitEnd();
            TraceMethod traceMethod = TraceMethod.create(0, access, className, name, desc);

            if ("<init>".equals(name) /*|| "<clinit>".equals(name)*/) {
                isConstructor = true;
            }
            // filter simple methods
            if ((isEmptyMethod() || isGetSetMethod() || isSingleMethod())
                    && mTraceConfig.isNeedTrace(traceMethod.className, mMappingCollector)) {
                mIgnoreCount++;
                mCollectedIgnoreMethodMap.put(traceMethod.getMethodName(), traceMethod);
                return;
            }

            if (mTraceConfig.isNeedTrace(traceMethod.className, mMappingCollector) && !mCollectedMethodMap.containsKey(traceMethod.getMethodName())) {
                traceMethod.id = mMethodId.incrementAndGet();
                mCollectedMethodMap.put(traceMethod.getMethodName(), traceMethod);
                mIncrementCount++;
            } else if (!mTraceConfig.isNeedTrace(traceMethod.className, mMappingCollector)
                    && !mCollectedBlackMethodMap.containsKey(traceMethod.className)) {
                mIgnoreCount++;
                mCollectedBlackMethodMap.put(traceMethod.getMethodName(), traceMethod);
            }

        }

        private boolean isGetSetMethod() {
            // complex method
//            if (!isConstructor && instructions.size() > 20) {
//                return false;
//            }
            int ignoreCount = 0;
            ListIterator<AbstractInsnNode> iterator = instructions.iterator();
            while (iterator.hasNext()) {
                AbstractInsnNode insnNode = iterator.next();
                int opcode = insnNode.getOpcode();
                if (-1 == opcode) {
                    continue;
                }
                if (opcode != Opcodes.GETFIELD
                        && opcode != Opcodes.GETSTATIC
                        && opcode != Opcodes.H_GETFIELD
                        && opcode != Opcodes.H_GETSTATIC

                        && opcode != Opcodes.RETURN
                        && opcode != Opcodes.ARETURN
                        && opcode != Opcodes.DRETURN
                        && opcode != Opcodes.FRETURN
                        && opcode != Opcodes.LRETURN
                        && opcode != Opcodes.IRETURN

                        && opcode != Opcodes.PUTFIELD
                        && opcode != Opcodes.PUTSTATIC
                        && opcode != Opcodes.H_PUTFIELD
                        && opcode != Opcodes.H_PUTSTATIC
                        && opcode > Opcodes.SALOAD) {
                    if (isConstructor && opcode == Opcodes.INVOKESPECIAL) {
                        ignoreCount++;
                        if (ignoreCount > 1) {
//                            Log.e(TAG, "[ignore] classname %s, name %s", className, name);
                            return false;
                        }
                        continue;
                    }
                    return false;
                }
            }
            return true;
        }

        private boolean isSingleMethod() {
            ListIterator<AbstractInsnNode> iterator = instructions.iterator();
            while (iterator.hasNext()) {
                AbstractInsnNode insnNode = iterator.next();
                int opcode = insnNode.getOpcode();
                if (-1 == opcode) {
                    continue;
                } else if (Opcodes.INVOKEVIRTUAL <= opcode && opcode <= Opcodes.INVOKEDYNAMIC) {
                    return false;
                }
            }
            return true;
        }


        private boolean isEmptyMethod() {
            ListIterator<AbstractInsnNode> iterator = instructions.iterator();
            while (iterator.hasNext()) {
                AbstractInsnNode insnNode = iterator.next();
                int opcode = insnNode.getOpcode();
                if (-1 == opcode) {
                    continue;
                } else {
                    return false;
                }
            }
            return true;
        }

    }

}
