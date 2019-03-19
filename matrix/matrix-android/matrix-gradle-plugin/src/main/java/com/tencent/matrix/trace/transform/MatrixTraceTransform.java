package com.tencent.matrix.trace.transform;

import com.android.build.api.transform.DirectoryInput;
import com.android.build.api.transform.JarInput;
import com.android.build.api.transform.QualifiedContent;
import com.android.build.api.transform.Status;
import com.android.build.api.transform.Transform;
import com.android.build.api.transform.TransformException;
import com.android.build.api.transform.TransformInput;
import com.android.build.api.transform.TransformInvocation;
import com.android.build.gradle.internal.pipeline.TransformManager;
import com.android.build.gradle.internal.pipeline.TransformTask;
import com.android.build.gradle.internal.scope.GlobalScope;
import com.android.build.gradle.internal.scope.VariantScope;
import com.android.build.gradle.internal.variant.BaseVariantData;
import com.android.utils.FileUtils;
import com.google.common.base.Charsets;
import com.google.common.base.Joiner;
import com.google.common.hash.Hashing;
import com.tencent.matrix.javalib.util.IOUtil;
import com.tencent.matrix.javalib.util.Log;
import com.tencent.matrix.javalib.util.ReflectUtil;
import com.tencent.matrix.javalib.util.Util;
import com.tencent.matrix.trace.Configuration;
import com.tencent.matrix.trace.MethodCollector;
import com.tencent.matrix.trace.MethodTracer;
import com.tencent.matrix.trace.TraceBuildConstants;
import com.tencent.matrix.trace.extension.MatrixTraceExtension;
import com.tencent.matrix.trace.item.TraceMethod;
import com.tencent.matrix.trace.retrace.MappingCollector;
import com.tencent.matrix.trace.retrace.MappingReader;

import org.gradle.api.Project;
import org.gradle.api.Task;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.lang.reflect.Field;
import java.util.Collection;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Scanner;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.atomic.AtomicInteger;

import static com.android.builder.model.AndroidProject.FD_OUTPUTS;

public class MatrixTraceTransform extends Transform {

    private static final String TAG = "MatrixTraceTransform";
    private Configuration config;
    private Transform origTransform;
    private ExecutorService executor = Executors.newFixedThreadPool(10);

    public static void inject(Project project, MatrixTraceExtension extension, VariantScope variantScope) {

        GlobalScope globalScope = variantScope.getGlobalScope();
        BaseVariantData variant = variantScope.getVariantData();
        String mappingOut = Joiner.on(File.separatorChar).join(
                String.valueOf(globalScope.getBuildDir()),
                FD_OUTPUTS,
                "mapping",
                variantScope.getVariantConfiguration().getDirName());

        String traceClassOut = Joiner.on(File.separatorChar).join(
                String.valueOf(globalScope.getBuildDir()),
                FD_OUTPUTS,
                "traceClassOut",
                variantScope.getVariantConfiguration().getDirName());
        Configuration config = new Configuration.Builder()
                .setPackageName(variant.getApplicationId())
                .setBaseMethodMap(extension.getBaseMethodMapFile())
                .setBlackListFile(extension.getBlackListFile())
                .setMethodMapFilePath(mappingOut + "/methodMapping.txt")
                .setIgnoreMethodMapFilePath(mappingOut + "/ignoreMethodMapping.txt")
                .setMappingPath(mappingOut)
                .setTraceClassOut(traceClassOut)
                .build();

        try {
            String[] hardTask = getTransformTaskName(extension.getCustomDexTransformName(), variant.getName());
            for (Task task : project.getTasks()) {
                for (String str : hardTask) {
                    if (task.getName().equalsIgnoreCase(str) && task instanceof TransformTask) {
                        TransformTask transformTask = (TransformTask) task;
                        Log.i(TAG, "successfully inject task:" + transformTask.getName());
                        Field field = TransformTask.class.getDeclaredField("transform");
                        field.setAccessible(true);
                        field.set(task, new MatrixTraceTransform(config, transformTask.getTransform()));
                        break;
                    }
                }
            }
        } catch (Exception e) {
            Log.e(TAG, e.toString());
        }

    }


    private static String[] getTransformTaskName(String customDexTransformName, String buildTypeSuffix) {
        if (!Util.isNullOrNil(customDexTransformName)) {
            return new String[]{customDexTransformName + "For" + "buildTypeSuffix"};
        } else {
            String[] names = new String[]{
                    "transformClassesWithDexBuilderFor" + buildTypeSuffix,
                    "transformClassesWithDexFor" + buildTypeSuffix,
            };
            return names;
        }
    }

    public MatrixTraceTransform(Configuration config, Transform origTransform) {
        this.config = config;
        this.origTransform = origTransform;
    }

    @Override
    public String getName() {
        return TAG;
    }

    @Override
    public Set<QualifiedContent.ContentType> getInputTypes() {
        return TransformManager.CONTENT_CLASS;
    }

    @Override
    public Set<QualifiedContent.Scope> getScopes() {
        return TransformManager.SCOPE_FULL_PROJECT;
    }

    @Override
    public boolean isIncremental() {
        return true;
    }

    @Override
    public void transform(TransformInvocation transformInvocation) throws TransformException, InterruptedException, IOException {
        super.transform(transformInvocation);
        long start = System.currentTimeMillis();
        try {
            doTransform(transformInvocation); // hack
        } catch (ExecutionException e) {
            e.printStackTrace();
        }
        long cost = System.currentTimeMillis() - start;
        long begin = System.currentTimeMillis();
        origTransform.transform(transformInvocation);
        long origTransformCost = System.currentTimeMillis() - begin;
        Log.i("Matrix." + getName(), "[transform] cost time: %dms %s:%sms MatrixTraceTransform:%sms", System.currentTimeMillis() - start, origTransform.getClass().getSimpleName(), origTransformCost, cost);
    }

    private void doTransform(TransformInvocation transformInvocation) throws ExecutionException, InterruptedException {
        final boolean isIncremental = transformInvocation.isIncremental() && this.isIncremental();

        /**
         * step 1
         */
        long start = System.currentTimeMillis();

        List<Future> futures = new LinkedList<>();

        final MappingCollector mappingCollector = new MappingCollector();
        final AtomicInteger methodId = new AtomicInteger(0);
        final ConcurrentHashMap<String, TraceMethod> collectedMethodMap = new ConcurrentHashMap<>();

        futures.add(executor.submit(new ParseMappingTask(mappingCollector, collectedMethodMap, methodId)));

        Map<File, File> dirInputOutMap = new ConcurrentHashMap<>();
        Map<File, File> jarInputOutMap = new ConcurrentHashMap<>();
        Collection<TransformInput> inputs = transformInvocation.getInputs();

        for (TransformInput input : inputs) {

            for (DirectoryInput directoryInput : input.getDirectoryInputs()) {
                futures.add(executor.submit(new CollectDirectoryInputTask(dirInputOutMap, directoryInput, isIncremental)));
            }

            for (JarInput inputJar : input.getJarInputs()) {
                futures.add(executor.submit(new CollectJarInputTask(inputJar, isIncremental, jarInputOutMap, dirInputOutMap)));
            }
        }

        for (Future future : futures) {
            future.get();
        }
        futures.clear();

        Log.i(TAG, "[doTransform] Step(1)[Parse]... cost:%sms", System.currentTimeMillis() - start);


        /**
         * step 2
         */
        start = System.currentTimeMillis();
        MethodCollector methodCollector = new MethodCollector(executor, mappingCollector, methodId, config, collectedMethodMap);
        methodCollector.collect(dirInputOutMap.keySet(), jarInputOutMap.keySet());
        Log.i(TAG, "[doTransform] Step(2)[Collection]... cost:%sms", System.currentTimeMillis() - start);

        /**
         * step 3
         */
        start = System.currentTimeMillis();
        MethodTracer methodTracer = new MethodTracer(executor, config, methodCollector.getCollectedMethodMap(), methodCollector.getCollectedClassExtendMap());
        methodTracer.trace(dirInputOutMap, jarInputOutMap);
        Log.i(TAG, "[doTransform] Step(3)[Trace]... cost:%sms", System.currentTimeMillis() - start);

    }


    private class ParseMappingTask implements Runnable {

        final MappingCollector mappingCollector;
        final ConcurrentHashMap<String, TraceMethod> collectedMethodMap;
        private final AtomicInteger methodId;

        public ParseMappingTask(MappingCollector mappingCollector, ConcurrentHashMap<String, TraceMethod> collectedMethodMap, AtomicInteger methodId) {
            this.mappingCollector = mappingCollector;
            this.collectedMethodMap = collectedMethodMap;
            this.methodId = methodId;
        }

        @Override
        public void run() {
            try {
                long start = System.currentTimeMillis();

                File mappingFile = new File(config.mappingDir, "mapping.txt");
                if (mappingFile.exists() && mappingFile.isFile()) {
                    MappingReader mappingReader = new MappingReader(mappingFile);
                    mappingReader.read(mappingCollector);
                }
                config.parseBlackFile(mappingCollector);

                File baseMethodMapFile = new File(config.baseMethodMapPath);
                getMethodFromBaseMethod(baseMethodMapFile);
                retraceMethodMap(mappingCollector, collectedMethodMap);

                Log.i(TAG, "[ParseMappingTask#run] cost:%s %s method from %s", System.currentTimeMillis() - start, collectedMethodMap.size(), config.baseMethodMapPath);

            } catch (IOException e) {
                e.printStackTrace();
            }
        }

        private void retraceMethodMap(MappingCollector processor, ConcurrentHashMap<String, TraceMethod> methodMap) {
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

                        if (methodId.get() < traceMethod.id && traceMethod.id != TraceBuildConstants.METHOD_ID_DISPATCH) {
                            methodId.set(traceMethod.id);
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
    }


    private class CollectDirectoryInputTask implements Runnable {

        Map<File, File> dirInputOutMap;
        DirectoryInput directoryInput;
        boolean isIncremental;
        String traceClassOut;

        public CollectDirectoryInputTask(Map<File, File> dirInputOutMap, DirectoryInput directoryInput, boolean isIncremental) {
            this.dirInputOutMap = dirInputOutMap;
            this.directoryInput = directoryInput;
            this.isIncremental = isIncremental;
            this.traceClassOut = config.traceClassOut;
        }

        @Override
        public void run() {
            try {
                handle();
            } catch (IOException e) {
                e.printStackTrace();
            } catch (IllegalAccessException e) {
                e.printStackTrace();
            } catch (NoSuchFieldException e) {
                e.printStackTrace();
            } catch (ClassNotFoundException e) {
                e.printStackTrace();
            }
        }

        private void handle() throws IOException, IllegalAccessException, NoSuchFieldException, ClassNotFoundException {
            final File dirInput = directoryInput.getFile();
            final File dirOutput = new File(traceClassOut, dirInput.getName());
            final String inputFullPath = dirInput.getAbsolutePath();
            final String outputFullPath = dirOutput.getAbsolutePath();

            if (!dirOutput.exists()) {
                dirOutput.mkdirs();
            }

            if (!dirInput.exists() && dirOutput.exists()) {
                if (dirInput.isDirectory()) {
                    FileUtils.deleteFolder(dirOutput);
                } else {
                    FileUtils.delete(dirOutput);
                }
            }

            if (isIncremental) {
                Map<File, Status> fileStatusMap = directoryInput.getChangedFiles();
                final Map<File, Status> outChangedFiles = new HashMap<>();

                for (Map.Entry<File, Status> entry : fileStatusMap.entrySet()) {
                    final Status status = entry.getValue();
                    final File changedFileInput = entry.getKey();

                    final String changedFileInputFullPath = changedFileInput.getAbsolutePath();
                    final File changedFileOutput = new File(changedFileInputFullPath.replace(inputFullPath, outputFullPath));

                    if (status == Status.ADDED || status == Status.CHANGED) {
                        dirInputOutMap.put(changedFileInput, changedFileOutput);
                    } else if (status == Status.REMOVED) {
                        changedFileOutput.delete();
                    }
                    outChangedFiles.put(changedFileOutput, status);
                }
                replaceChangedFile(directoryInput, outChangedFiles);

            } else {
                dirInputOutMap.put(dirInput, dirOutput);
            }
        }
    }

    private class CollectJarInputTask implements Runnable {
        JarInput inputJar;
        boolean isIncremental;
        Map<File, File> jarInputOutMap;
        Map<File, File> dirInputOutMap;

        public CollectJarInputTask(JarInput inputJar, boolean isIncremental, Map<File, File> jarInputOutMap, Map<File, File> dirInputOutMap) {
            this.inputJar = inputJar;
            this.isIncremental = isIncremental;
            this.jarInputOutMap = jarInputOutMap;
            this.dirInputOutMap = dirInputOutMap;
        }

        @Override
        public void run() {
            try {
                handle();
            } catch (IllegalAccessException e) {
                e.printStackTrace();
            } catch (NoSuchFieldException e) {
                e.printStackTrace();
            } catch (ClassNotFoundException e) {
                e.printStackTrace();
            } catch (IOException e) {
                e.printStackTrace();
            }
        }

        private void handle() throws IllegalAccessException, NoSuchFieldException, ClassNotFoundException, IOException {
            String traceClassOut = config.traceClassOut;

            final File jarInput = inputJar.getFile();
            final File jarOutput = new File(traceClassOut, getUniqueJarName(jarInput));

            if (IOUtil.isRealZipOrJar(jarInput)) {
                if (isIncremental) {
                    if (inputJar.getStatus() == Status.ADDED || inputJar.getStatus() == Status.CHANGED) {
                        jarInputOutMap.put(jarInput, jarOutput);
                    } else if (inputJar.getStatus() == Status.REMOVED) {
                        jarOutput.delete();
                    }

                } else {
                    jarInputOutMap.put(jarInput, jarOutput);
                }

            } else {
                // Special case for WeChat AutoDex. Its rootInput jar file is actually
                // a txt file contains path list.
                BufferedReader br = null;
                BufferedWriter bw = null;
                try {
                    br = new BufferedReader(new InputStreamReader(new FileInputStream(jarInput)));
                    bw = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(jarOutput)));
                    String realJarInputFullPath;
                    while ((realJarInputFullPath = br.readLine()) != null) {
                        // src jar.
                        final File realJarInput = new File(realJarInputFullPath);
                        // dest jar, moved to extra guard intermediate output dir.
                        File realJarOutput = new File(traceClassOut, realJarInput.getName());

                        if (realJarInput.exists() && IOUtil.isRealZipOrJar(realJarInput)) {
                            jarInputOutMap.put(realJarInput, realJarOutput);
                        } else {
                            realJarOutput.delete();
                            if (realJarInput.exists() && realJarInput.isDirectory()) {
                                realJarOutput = new File(traceClassOut, realJarInput.getName());
                                if (!realJarOutput.exists()) {
                                    realJarOutput.mkdirs();
                                }
                                dirInputOutMap.put(realJarInput, realJarOutput);
                            }

                        }
                        // write real output full path to the fake jar at rootOutput.
                        final String realJarOutputFullPath = realJarOutput.getAbsolutePath();
                        bw.write(realJarOutputFullPath);
                        bw.newLine();
                    }
                } catch (FileNotFoundException e) {
                    Log.e("Matrix." + getName(), "FileNotFoundException:%s", e.toString());
                } finally {
                    IOUtil.closeQuietly(br);
                    IOUtil.closeQuietly(bw);
                }
                jarInput.delete(); // delete raw inputList
            }

            replaceFile(inputJar, jarOutput);

        }
    }

    protected String getUniqueJarName(File jarFile) {
        final String origJarName = jarFile.getName();
        final String hashing = Hashing.sha1().hashString(jarFile.getPath(), Charsets.UTF_16LE).toString();
        final int dotPos = origJarName.lastIndexOf('.');
        if (dotPos < 0) {
            return String.format("%s_%s", origJarName, hashing);
        } else {
            final String nameWithoutDotExt = origJarName.substring(0, dotPos);
            final String dotExt = origJarName.substring(dotPos);
            return String.format("%s_%s%s", nameWithoutDotExt, hashing, dotExt);
        }
    }

    private void replaceFile(QualifiedContent input, File newFile) throws NoSuchFieldException, ClassNotFoundException, IllegalAccessException {
        final Field fileField = ReflectUtil.getDeclaredFieldRecursive(input.getClass(), "file");
        fileField.set(input, newFile);
    }

    private void replaceChangedFile(DirectoryInput dirInput, Map<File, Status> changedFiles) throws NoSuchFieldException, ClassNotFoundException, IllegalAccessException {
        final Field changedFilesField = ReflectUtil.getDeclaredFieldRecursive(dirInput.getClass(), "changedFiles");
        changedFilesField.set(dirInput, changedFiles);
    }

}
