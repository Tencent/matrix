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

import com.android.dexdeps.Output;
import com.tencent.matrix.javalib.util.Log;
import com.tencent.matrix.javalib.util.Util;

import org.jf.baksmali.Adaptors.ClassDefinition;
import org.jf.baksmali.BaksmaliOptions;
import org.jf.dexlib2.iface.ClassDef;
import org.jf.util.IndentingWriter;

import java.io.BufferedWriter;
import java.io.ByteArrayOutputStream;
import java.io.OutputStreamWriter;
import java.io.Writer;
import java.util.regex.Pattern;

/**
 * Created by jinqiuchen on 17/11/13.
 */

public class ApkUtil {

    private static final String TAG = "Matrix.ApkUtil";


    /*
     *  parse the descriptor of class to normal dot-split class name (XXX.XXX.XXX)
     */
    public static String getNormalClassName(String name) {
        if (!Util.isNullOrNil(name)) {
            String className = Output.descriptorToDot(name);
            if (className.endsWith("[]")) {                                                //enum or array
                className = className.substring(0, className.indexOf("[]"));
            }
            return className;
        }
        return "";
    }

    /*
     *  retrieve the package name from dot-split class name
     */
    public static String getPackageName(String className) {
        if (!Util.isNullOrNil(className)) {
            int index = className.lastIndexOf('.');
            if (index >= 0) {
                return className.substring(0, index);
            } else {
                Log.d(TAG, "default package class: %s", className);
                return "<default>";
            }
        }
        return "";
    }


    /*
     *  retrieve the class name without package prefix
     */

    public static String getPureClassName(String classname) {
        String name = "";
        if (!Util.isNullOrNil(classname)) {
            int index = classname.lastIndexOf('.');
            if (index != -1) {
                name = classname.substring(index + 1);
            } else {
                name = classname;
            }
        }
        return name;
    }

    /*
     *  determine if the class if R class
     */
    public static boolean isRClassName(String className) {
        Pattern pattern = Pattern.compile("^R\\$\\w+");
        return pattern.matcher(className).matches();
    }


    public static String[] disassembleClass(ClassDef classDef, BaksmaliOptions options) {
        /**
         * The path for the disassembly file is based on the package name
         * The class descriptor will look something like:
         * Ljava/lang/Object;
         * Where the there is leading 'L' and a trailing ';', and the parts of the
         * package name are separated by '/'
         */
        String classDescriptor = classDef.getType();

        //validate that the descriptor is formatted like we expect
        if (classDescriptor.charAt(0) != 'L'
                || classDescriptor.charAt(classDescriptor.length() - 1) != ';') {
            Log.e(TAG, "Unrecognized class descriptor - " + classDescriptor + " - skipping class");
            return null;
        }


        //create and initialize the top level string template
        ClassDefinition classDefinition = new ClassDefinition(options, classDef);

        //write the disassembly
        Writer writer = null;
        try {
            ByteArrayOutputStream baos = new ByteArrayOutputStream();

            BufferedWriter bufWriter = new BufferedWriter(new OutputStreamWriter(baos, "UTF8"));

            writer = new IndentingWriter(bufWriter);
            classDefinition.writeTo((IndentingWriter) writer);
            writer.flush();
            return baos.toString().split("\n");
        } catch (Exception ex) {
            Log.e(TAG, "\n\nError occurred while disassembling class " + classDescriptor.replace('/', '.') + " - skipping class");
            ex.printStackTrace();
            // noinspection ResultOfMethodCallIgnored
            return null;
        } finally {
            if (writer != null) {
                try {
                    writer.close();
                } catch (Throwable ex) {
                    ex.printStackTrace();
                }
            }
        }
    }
}
