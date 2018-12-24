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

package com.tencent.matrix.apk.model.task.util;


import com.tencent.matrix.javalib.util.FileUtil;
import com.tencent.matrix.javalib.util.Log;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;
import org.xmlpull.v1.XmlPullParserFactory;
import java.io.BufferedInputStream;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Map;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

import brut.androlib.AndrolibException;
import brut.androlib.res.data.ResPackage;
import brut.androlib.res.data.ResResource;
import brut.androlib.res.data.ResTable;
import brut.androlib.res.data.ResValuesFile;
import brut.androlib.res.data.value.ResFileValue;
import brut.androlib.res.decoder.ARSCDecoder;
import brut.androlib.res.decoder.AXmlResourceParser;
import brut.androlib.res.decoder.ResAttrDecoder;
import brut.androlib.res.util.ExtMXSerializer;
import brut.androlib.res.xml.ResValuesXmlSerializable;

/**
 * Created by jinqiuchen on 17/7/13.
 * This decoder is used to decode binary resources (xml files and resources.arsc) in apk.
 * We use the ApkTool
 * <p>
 *  Copyright (C) 2017 Ryszard Wiśniewski <brut.alll@gmail.com>
 *  Copyright (C) 2017 Connor Tumbleson <connor.tumbleson@gmail.com>
 * <p>
 * to decode binary resources (xml files and resources.arsc).
 */
@SuppressWarnings("PMD")
public class ApkResourceDecoder {

    private static final String TAG = "Matrix.ApkResourceDecoder";

    public static final String PROPERTY_SERIALIZER_INDENTATION = "http://xmlpull.org/v1/doc/properties.html#serializer-indentation";
    public static final String PROPERTY_SERIALIZER_LINE_SEPARATOR = "http://xmlpull.org/v1/doc/properties.html#serializer-line-separator";
    public static final String PROPERTY_DEFAULT_ENCNDING = "DEFAULT_ENCODING";

    public static AXmlResourceParser createAXmlParser() {
        AXmlResourceParser resourceParser = new AXmlResourceParser();
        ResTable resTable =  new ResTable();
        resourceParser.setAttrDecoder(new ResAttrDecoder());
        resourceParser.getAttrDecoder().setCurrentPackage(new ResPackage(resTable, 0, null));
        return resourceParser;
    }

    public static AXmlResourceParser createAXmlParser(File arscFile) throws IOException, AndrolibException {
        AXmlResourceParser resourceParser = createAXmlParser();
        ResTable resTable = new ResTable();
        decodeArscFile(arscFile, resTable);
        resourceParser.getAttrDecoder().setCurrentPackage(resTable.listMainPackages().iterator().next());
        return resourceParser;
    }

    public static void decodeArscFile(File file, ResTable resTable) throws IOException, AndrolibException {
        if (file != null && FileUtil.isLegalFile(file)) {
            BufferedInputStream inputStream = new BufferedInputStream(new FileInputStream(file));
            try {
                try {
                    ResPackage[] resPackages =  ARSCDecoder.decode(inputStream, false, true, resTable).getPackages();
                    ResPackage mainPackage = getMainPackage(resPackages);
                    if (mainPackage != null) {
                        resTable.addPackage(mainPackage, true);
                    }
                    ResPackage[] frameworkPackages = loadFrameworkPackage(resTable);
                    for (ResPackage sysPackage : frameworkPackages) {
                        resTable.addPackage(sysPackage, false);
                    }
                } finally {
                    inputStream.close();
                }
            } catch (IOException e) {
                throw e;
            }
        }
    }

    private static ResPackage[] loadFrameworkPackage(ResTable resTable) throws IOException, AndrolibException {
        ResPackage[] resPackages = new ResPackage[0];
        ZipInputStream zipInputStream = new ZipInputStream(ApkResourceDecoder.class.getResourceAsStream("/android/android-framework.jar"));
        ZipEntry entry = zipInputStream.getNextEntry();
        BufferedInputStream bufInputStream = null;
        try {
            try {
                while (entry != null) {
                    if (entry.getName().equals("resources.arsc")) {
                        bufInputStream = new BufferedInputStream(zipInputStream);
                        resPackages = ARSCDecoder.decode(bufInputStream, false, true, resTable).getPackages();
                        break;
                    }
                    entry = zipInputStream.getNextEntry();
                }
            } finally {
                if (bufInputStream != null) {
                    bufInputStream.close();
                }
            }
        } catch (IOException e) {
            throw e;
        }
        return resPackages;
    }


    private static ResPackage getMainPackage(ResPackage[] resPackages) {
        ResPackage pkg = null;
        if (resPackages != null && resPackages.length > 0) {
            if (resPackages.length == 1) {
                pkg = resPackages[0];
            } else {
                int id = 0;
                int value = 0;
                int index = 0;
                ResPackage resPackage;
                for (int i = 0; i < resPackages.length; i++) {
                    resPackage = resPackages[i];
                    if (resPackage.getResSpecCount() > value && !resPackage.getName().equalsIgnoreCase("android")) {
                        value = resPackage.getResSpecCount();
                        id = resPackage.getId();
                        index = i;
                    }
                }

                // if id is still 0, we only have one pkgId which is "android" -> 1
                return (id == 0) ? resPackages[0] : resPackages[index];
            }
        }
        return pkg;
    }

    private static void decodeResResource(ResResource res, File inDir, AXmlResourceParser xmlParser, Map<String, Set<String>> nonValueReferences) throws AndrolibException, IOException {
        ResFileValue fileValue = (ResFileValue) res.getValue();
        String inFileName = fileValue.getStrippedPath();
        String typeName = res.getResSpec().getType().getName();

        try {
            File inFile = new File(inDir, inFileName);
            if (!FileUtil.isLegalFile(inFile)) {
//                Log.d(TAG, "Can not find %s", inFile.getAbsolutePath());
                return;
            }

            if (!inFileName.endsWith(".xml")) {
//                Log.d(TAG, "Not xml file %s, type %s", inFileName, typeName);
                return;
            }

            FileInputStream inputStream = new FileInputStream(inFile);
            XmlPullResourceRefDecoder xmlDecoder = new XmlPullResourceRefDecoder(xmlParser);
            xmlDecoder.decode(inputStream, null);
            String resource = ApkConstants.R_PREFIX + typeName + "." + inFile.getName().substring(0, inFile.getName().lastIndexOf('.'));
            if (!nonValueReferences.containsKey(resource)) {
                nonValueReferences.put(resource, xmlDecoder.getResourceRefSet());
            } else {
                nonValueReferences.get(resource).addAll(xmlDecoder.getResourceRefSet());
            }
        } catch (AndrolibException ex) {
            Log.e(TAG, ex.getMessage());
        }
    }

    private static void decodeResValues(ResValuesFile resValuesFile, XmlPullParser xmlParser, ExtMXSerializer serializer, Set<String> references) throws IOException, AndrolibException {

//        Log.d(TAG, resValuesFile.getPath());
        ByteArrayOutputStream outStream = new ByteArrayOutputStream();
        serializer.setOutput((outStream), null);
        serializer.startDocument(null, null);
        serializer.startTag(null, "resources");

        for (ResResource res : resValuesFile.listResources()) {
            if (resValuesFile.isSynthesized(res)) {
                continue;
            }
            ((ResValuesXmlSerializable) res.getValue()).serializeToResValuesXml(serializer, res);
        }
        serializer.endTag(null, "resources");
        serializer.newLine();
        serializer.endDocument();
        serializer.flush();
        outStream.close();
        ByteArrayInputStream inputStream = new ByteArrayInputStream(outStream.toByteArray());
        XmlPullResourceRefDecoder xmlDecoder = new XmlPullResourceRefDecoder(xmlParser);
        xmlDecoder.decode(inputStream, null);
        references.addAll(xmlDecoder.getResourceRefSet());
    }

    public static void decodeResourcesRef(File manifestFile, File arscFile, File resDir, Map<String, Set<String>> nonValueReferences, Set<String> valueReferences) throws IOException, AndrolibException, XmlPullParserException {
        if (!FileUtil.isLegalFile(manifestFile)) {
            Log.w(TAG, "File %s is illegal!", ApkConstants.MANIFEST_FILE_NAME);
            return;
        }
        if (!FileUtil.isLegalFile(arscFile)) {
            Log.w(TAG, "File %s is illegal!", ApkConstants.ARSC_FILE_NAME);
            return;
        }
        if (resDir != null && resDir.exists() && resDir.isDirectory()) {
            //decode arsc file
            ResTable resTable = new ResTable();
            decodeArscFile(arscFile, resTable);

            AXmlResourceParser aXmlResourceParser = createAXmlParser(arscFile);
            XmlPullParser xmlPullParser = XmlPullParserFactory.newInstance().newPullParser();
            ExtMXSerializer serializer = createXmlSerializer();
            for (ResPackage pkg : resTable.listMainPackages()) {
                aXmlResourceParser.getAttrDecoder().setCurrentPackage(pkg);
                for (ResResource resSource : pkg.listFiles()) {
                    decodeResResource(resSource, resDir, aXmlResourceParser, nonValueReferences);
                }

                for (ResValuesFile valuesFile : pkg.listValuesFiles()) {
                    decodeResValues(valuesFile, xmlPullParser, serializer, valueReferences);
                }
            }

            //decode manifest file
            XmlPullResourceRefDecoder xmlDecoder = new XmlPullResourceRefDecoder(aXmlResourceParser);
            InputStream inputStream = new FileInputStream(manifestFile);
            xmlDecoder.decode(inputStream, null);
            valueReferences.addAll(xmlDecoder.getResourceRefSet());
        } else {
            Log.w(TAG, "Res dir is illegal!");
        }
    }

    private static ExtMXSerializer createXmlSerializer() {
        ExtMXSerializer serializer = new ExtMXSerializer();
        serializer.setProperty(PROPERTY_SERIALIZER_INDENTATION, "   ");
        serializer.setProperty(PROPERTY_SERIALIZER_LINE_SEPARATOR, System.lineSeparator());
        serializer.setProperty(PROPERTY_DEFAULT_ENCNDING, "utf-8");
        serializer.setDisabledAttrEscape(true);
        return serializer;
    }



}
