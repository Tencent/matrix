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

package com.tencent.matrix.trace.retrace;

import com.tencent.matrix.javalib.util.Log;

import org.objectweb.asm.Type;

import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedHashSet;
import java.util.Map;
import java.util.Set;

/**
 * Created by caichongyang on 2017/8/3.
 */
public class MappingCollector implements MappingProcessor {
    private final static String TAG = "MappingCollector";
    private final static int DEFAULT_CAPACITY = 2000;
    public HashMap<String, String> mObfuscatedRawClassMap = new HashMap<>(DEFAULT_CAPACITY);
    public HashMap<String, String> mRawObfuscatedClassMap = new HashMap<>(DEFAULT_CAPACITY);
    public HashMap<String, String> mRawObfuscatedPackageMap = new HashMap<>(DEFAULT_CAPACITY);
    private final Map<String, Map<String, Set<MethodInfo>>> mObfuscatedClassMethodMap = new HashMap<>();
    private final Map<String, Map<String, Set<MethodInfo>>> mOriginalClassMethodMap = new HashMap<>();

    @Override
    public boolean processClassMapping(String className, String newClassName) {
        this.mObfuscatedRawClassMap.put(newClassName, className);
        this.mRawObfuscatedClassMap.put(className, newClassName);
        int classNameLen = className.lastIndexOf('.');
        int newClassNameLen = newClassName.lastIndexOf('.');
        if (classNameLen > 0 && newClassNameLen > 0) {
            this.mRawObfuscatedPackageMap.put(className.substring(0, classNameLen), newClassName.substring(0, newClassNameLen));
        } else {
            Log.e(TAG, "class without package name: %s -> %s, pls check input mapping", className, newClassName);
        }
        return true;
    }

    @Override
    public void processMethodMapping(String className, String methodReturnType, String methodName, String methodArguments, String newClassName, String newMethodName) {
        newClassName = mRawObfuscatedClassMap.get(className);
        Map<String, Set<MethodInfo>> methodMap = mObfuscatedClassMethodMap.get(newClassName);
        if (methodMap == null) {
            methodMap = new HashMap<>();
            mObfuscatedClassMethodMap.put(newClassName, methodMap);
        }
        Set<MethodInfo> methodSet = methodMap.get(newMethodName);
        if (methodSet == null) {
            methodSet = new LinkedHashSet<>();
            methodMap.put(newMethodName, methodSet);
        }
        methodSet.add(new MethodInfo(className, methodReturnType, methodName, methodArguments));


        Map<String, Set<MethodInfo>> methodMap2 = mOriginalClassMethodMap.get(className);
        if (methodMap2 == null) {
            methodMap2 = new HashMap<>();
            mOriginalClassMethodMap.put(className, methodMap2);
        }
        Set<MethodInfo> methodSet2 = methodMap2.get(methodName);
        if (methodSet2 == null) {
            methodSet2 = new LinkedHashSet<>();
            methodMap2.put(methodName, methodSet2);
        }
        methodSet2.add(new MethodInfo(newClassName, methodReturnType, newMethodName, methodArguments));

    }

    public String originalClassName(String proguardClassName, String defaultClassName) {
        if (mObfuscatedRawClassMap.containsKey(proguardClassName)) {
            return mObfuscatedRawClassMap.get(proguardClassName);
        } else {
            return defaultClassName;
        }
    }

    public String proguardClassName(String originalClassName, String defaultClassName) {
        if (mRawObfuscatedClassMap.containsKey(originalClassName)) {
            return mRawObfuscatedClassMap.get(originalClassName);
        } else {
            return defaultClassName;
        }
    }

    public String proguardPackageName(String originalPackage, String defaultPackage) {
        if (mRawObfuscatedPackageMap.containsKey(originalPackage)) {
            return mRawObfuscatedPackageMap.get(originalPackage);
        } else {
            return defaultPackage;
        }
    }

    /**
     * get original method info
     *
     * @param obfuscatedClassName
     * @param obfuscatedMethodName
     * @param obfuscatedMethodDesc
     * @return
     */
    public MethodInfo originalMethodInfo(String obfuscatedClassName, String obfuscatedMethodName, String obfuscatedMethodDesc) {
        DescInfo descInfo = parseMethodDesc(obfuscatedMethodDesc, false);

        // obfuscated name -> original method names.
        Map<String, Set<MethodInfo>> methodMap = mObfuscatedClassMethodMap.get(obfuscatedClassName);
        if (methodMap != null) {
            Set<MethodInfo> methodSet = methodMap.get(obfuscatedMethodName);
            if (methodSet != null) {
                // Find all matching methods.
                Iterator<MethodInfo> methodInfoIterator = methodSet.iterator();
                while (methodInfoIterator.hasNext()) {
                    MethodInfo methodInfo = methodInfoIterator.next();
                    if (methodInfo.matches(descInfo.returnType, descInfo.arguments)) {
                        MethodInfo newMethodInfo = new MethodInfo(methodInfo);
                        newMethodInfo.setDesc(descInfo.desc);
                        return newMethodInfo;
                    }
                }
            }
        }

        MethodInfo defaultMethodInfo = MethodInfo.deFault();
        defaultMethodInfo.setDesc(descInfo.desc);
        defaultMethodInfo.setOriginalName(obfuscatedMethodName);
        return defaultMethodInfo;
    }

    /**
     * get obfuscated method info
     *
     * @param originalClassName
     * @param originalMethodName
     * @param originalMethodDesc
     * @return
     */
    public MethodInfo obfuscatedMethodInfo(String originalClassName, String originalMethodName, String originalMethodDesc) {
        DescInfo descInfo = parseMethodDesc(originalMethodDesc, true);

        // Class name -> obfuscated method names.
        Map<String, Set<MethodInfo>> methodMap = mOriginalClassMethodMap.get(originalClassName);
        if (methodMap != null) {
            Set<MethodInfo> methodSet = methodMap.get(originalMethodName);
            if (null != methodSet) {
                // Find all matching methods.
                Iterator<MethodInfo> methodInfoIterator = methodSet.iterator();
                while (methodInfoIterator.hasNext()) {
                    MethodInfo methodInfo = methodInfoIterator.next();
                    MethodInfo newMethodInfo = new MethodInfo(methodInfo);
                    obfuscatedMethodInfo(newMethodInfo);
                    if (newMethodInfo.matches(descInfo.returnType, descInfo.arguments)) {
                        newMethodInfo.setDesc(descInfo.desc);
                        return newMethodInfo;
                    }
                }
            }
        }
        MethodInfo defaultMethodInfo = MethodInfo.deFault();
        defaultMethodInfo.setDesc(descInfo.desc);
        defaultMethodInfo.setOriginalName(originalMethodName);
        return defaultMethodInfo;
    }

    private void obfuscatedMethodInfo(MethodInfo methodInfo) {
        String methodArguments = methodInfo.getOriginalArguments();
        String[] args = methodArguments.split(",");
        StringBuffer stringBuffer = new StringBuffer();
        for (String str : args) {
            String key = str.replace("[", "").replace("]", "");
            if (mRawObfuscatedClassMap.containsKey(key)) {
                stringBuffer.append(str.replace(key, mRawObfuscatedClassMap.get(key)));
            } else {
                stringBuffer.append(str);
            }
            stringBuffer.append(',');
        }
        if (stringBuffer.length() > 0) {
            stringBuffer.deleteCharAt(stringBuffer.length() - 1);
        }
        String methodReturnType = methodInfo.getOriginalType();
        String key = methodReturnType.replace("[", "").replace("]", "");
        if (mRawObfuscatedClassMap.containsKey(key)) {
            methodReturnType = methodReturnType.replace(key, mRawObfuscatedClassMap.get(key));
        }
        methodInfo.setOriginalArguments(stringBuffer.toString());
        methodInfo.setOriginalType(methodReturnType);
    }

    /**
     * parse method desc
     *
     * @param desc
     * @param isRawToObfuscated
     * @return
     */
    private DescInfo parseMethodDesc(String desc, boolean isRawToObfuscated) {
        DescInfo descInfo = new DescInfo();
        Type[] argsObj = Type.getArgumentTypes(desc);
        StringBuffer argumentsBuffer = new StringBuffer();
        StringBuffer descBuffer = new StringBuffer();
        descBuffer.append('(');
        for (Type type : argsObj) {
            String key = type.getClassName().replace("[", "").replace("]", "");
            if (isRawToObfuscated) {
                if (mRawObfuscatedClassMap.containsKey(key)) {
                    argumentsBuffer.append(type.getClassName().replace(key, mRawObfuscatedClassMap.get(key)));
                    descBuffer.append(type.toString().replace(key, mRawObfuscatedClassMap.get(key)));
                } else {
                    argumentsBuffer.append(type.getClassName());
                    descBuffer.append(type.toString());
                }
            } else {
                if (mObfuscatedRawClassMap.containsKey(key)) {
                    argumentsBuffer.append(type.getClassName().replace(key, mObfuscatedRawClassMap.get(key)));
                    descBuffer.append(type.toString().replace(key, mObfuscatedRawClassMap.get(key)));
                } else {
                    argumentsBuffer.append(type.getClassName());
                    descBuffer.append(type.toString());
                }
            }
            argumentsBuffer.append(',');
        }
        descBuffer.append(')');

        Type returnObj;
        try {
            returnObj = Type.getReturnType(desc);
        } catch (ArrayIndexOutOfBoundsException e) {
            returnObj = Type.getReturnType(desc + ";");
        }
        if (isRawToObfuscated) {
            String key = returnObj.getClassName().replace("[", "").replace("]", "");
            if (mRawObfuscatedClassMap.containsKey(key)) {
                descInfo.setReturnType(returnObj.getClassName().replace(key, mRawObfuscatedClassMap.get(key)));
                descBuffer.append(returnObj.toString().replace(key, mRawObfuscatedClassMap.get(key)));
            } else {
                descInfo.setReturnType(returnObj.getClassName());
                descBuffer.append(returnObj.toString());
            }
        } else {
            String key = returnObj.getClassName().replace("[", "").replace("]", "");
            if (mObfuscatedRawClassMap.containsKey(key)) {
                descInfo.setReturnType(returnObj.getClassName().replace(key, mObfuscatedRawClassMap.get(key)));
                descBuffer.append(returnObj.toString().replace(key, mObfuscatedRawClassMap.get(key)));
            } else {
                descInfo.setReturnType(returnObj.getClassName());
                descBuffer.append(returnObj.toString());
            }
        }

        // delete last ,
        if (argumentsBuffer.length() > 0) {
            argumentsBuffer.deleteCharAt(argumentsBuffer.length() - 1);
        }
        descInfo.setArguments(argumentsBuffer.toString());

        descInfo.setDesc(descBuffer.toString());
        return descInfo;
    }

    /**
     * about method desc info
     */
    private static class DescInfo {
        private String desc;
        private String arguments;
        private String returnType;

        public void setArguments(String arguments) {
            this.arguments = arguments;
        }

        public void setReturnType(String returnType) {
            this.returnType = returnType;
        }

        public void setDesc(String desc) {
            this.desc = desc;
        }
    }

}
