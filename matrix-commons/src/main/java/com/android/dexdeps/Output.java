/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.dexdeps;

import java.io.PrintStream;
import java.util.List;

/**
 * Generate fancy output.
 */
@SuppressWarnings("PMD")
public class Output {
    private static final String IN0 = "";
    private static final String IN1 = "  ";
    private static final String IN2 = "    ";
    private static final String IN3 = "      ";
    private static final String IN4 = "        ";

    private static final PrintStream out = System.out;

    private static void generateHeader0(String fileName, String format) {
        if (format.equals("brief")) {
            if (fileName != null) {
                out.println("File: " + fileName);
            }
        } else if (format.equals("xml")) {
            if (fileName != null) {
                out.println(IN0 + "<external file=\"" + fileName + "\">");
            } else {
                out.println(IN0 + "<external>");
            }
        } else {
            /* should've been trapped in arg handler */
            throw new RuntimeException("unknown output format");
        }
    }

    public static void generateFirstHeader(String fileName, String format) {
        generateHeader0(fileName, format);
    }

    public static void generateHeader(String fileName, String format) {
        out.println();
        generateHeader0(fileName, format);
    }

    public static void generateFooter(String format) {
        if (format.equals("brief")) {
            // Nothing to do.
        } else if (format.equals("xml")) {
            out.println("</external>");
        } else {
            /* should've been trapped in arg handler */
            throw new RuntimeException("unknown output format");
        }
    }

    public static void generate(DexData dexData, String format,
            boolean justClasses) {
        if (format.equals("brief")) {
            printBrief(dexData, justClasses);
        } else if (format.equals("xml")) {
            printXml(dexData, justClasses);
        } else {
            /* should've been trapped in arg handler */
            throw new RuntimeException("unknown output format");
        }
    }

    /**
     * Prints the data in a simple human-readable format.
     */
    public static void printBrief(DexData dexData, boolean justClasses) {
        ClassRef[] externClassRefs = dexData.getExternalReferences();

        printClassRefs(externClassRefs, justClasses);

        if (!justClasses) {
            printFieldRefs(externClassRefs);
            printMethodRefs(externClassRefs);
        }
    }

    /**
     * Prints the list of classes in a simple human-readable format.
     */
    public static void printClassRefs(ClassRef[] classes, boolean justClasses) {
        if (!justClasses) {
            out.println("Classes:");
        }

        for (int i = 0; i < classes.length; i++) {
            ClassRef ref = classes[i];

            out.println(descriptorToDot(ref.getName()));
        }
    }

    /**
     * Prints the list of fields in a simple human-readable format.
     */
    public static void printFieldRefs(ClassRef[] classes) {
        out.println("\nFields:");
        for (int i = 0; i < classes.length; i++) {
            FieldRef[] fields = classes[i].getFieldArray();

            for (int j = 0; j < fields.length; j++) {
                FieldRef ref = fields[j];

                out.println(descriptorToDot(ref.getDeclClassName())
                        + "." + ref.getName() + " : " + ref.getTypeName());
            }
        }
    }

    /**
     * Prints the list of methods in a simple human-readable format.
     */
    public static void printMethodRefs(ClassRef[] classes) {
        out.println("\nMethods:");
        for (int i = 0; i < classes.length; i++) {
            MethodRef[] methods = classes[i].getMethodArray();

            for (int j = 0; j < methods.length; j++) {
                MethodRef ref = methods[j];

                out.println(descriptorToDot(ref.getDeclClassName())
                        + "." + ref.getName() + " : " + ref.getDescriptor());
            }
        }
    }

    /**
     * Prints the output in XML format.
     *
     * We shouldn't need to XML-escape the field/method info.
     */
    public static void printXml(DexData dexData, boolean justClasses) {
        ClassRef[] externClassRefs = dexData.getExternalReferences();

        /*
         * Iterate through externClassRefs.  For each class, dump all of
         * the matching fields and methods.
         */
        String prevPackage = null;
        for (int i = 0; i < externClassRefs.length; i++) {
            ClassRef cref = externClassRefs[i];
            String declClassName = cref.getName();
            String className = classNameOnly(declClassName);
            String packageName = packageNameOnly(declClassName);

            /*
             * If we're in a different package, emit the appropriate tags.
             */
            if (!packageName.equals(prevPackage)) {
                if (prevPackage != null) {
                    out.println(IN1 + "</package>");
                }

                out.println(IN1 + "<package name=\"" + packageName + "\">");

                prevPackage = packageName;
            }

            out.println(IN2 + "<class name=\"" + className + "\">");
            if (!justClasses) {
                printXmlFields(cref);
                printXmlMethods(cref);
            }
            out.println(IN2 + "</class>");
        }

        if (prevPackage != null) {
            out.println(IN1 + "</package>");
        }
    }

    /**
     * Prints the externally-visible fields in XML format.
     */
    private static void printXmlFields(ClassRef cref) {
        FieldRef[] fields = cref.getFieldArray();
        for (int i = 0; i < fields.length; i++) {
            FieldRef fref = fields[i];

            out.println(IN3 + "<field name=\"" + fref.getName()
                    + "\" type=\"" + descriptorToDot(fref.getTypeName()) + "\"/>");
        }
    }

    /**
     * Prints the externally-visible methods in XML format.
     */
    private static void printXmlMethods(ClassRef cref) {
        MethodRef[] methods = cref.getMethodArray();
        for (int i = 0; i < methods.length; i++) {
            MethodRef mref = methods[i];
            String declClassName = mref.getDeclClassName();
            boolean constructor;

            constructor = mref.getName().equals("<init>");
            if (constructor) {
                // use class name instead of method name
                out.println(IN3 + "<constructor name=\""
                        + classNameOnly(declClassName) + "\">");
            } else {
                out.println(IN3 + "<method name=\"" + mref.getName()
                        + "\" return=\"" + descriptorToDot(mref.getReturnTypeName()) + "\">");
            }
            List<String> args = mref.getArgumentTypeNames();
            for (int j = 0; j < args.size(); j++) {
                out.println(IN4 + "<parameter type=\""
                        + descriptorToDot(args.get(j)) + "\"/>");
            }
            if (constructor) {
                out.println(IN3 + "</constructor>");
            } else {
                out.println(IN3 + "</method>");
            }
        }
    }


    /*
     * =======================================================================
     *      Utility functions
     * =======================================================================
     */

    /**
     * Converts a single-character primitive type into its human-readable
     * equivalent.
     */
    public static String primitiveTypeLabel(char typeChar) {
        /* primitive type; substitute human-readable name in */
        switch (typeChar) {
            case 'B':   return "byte";
            case 'C':   return "char";
            case 'D':   return "double";
            case 'F':   return "float";
            case 'I':   return "int";
            case 'J':   return "long";
            case 'S':   return "short";
            case 'V':   return "void";
            case 'Z':   return "boolean";
            default:
                /* huh? */
                System.err.println("Unexpected class char " + typeChar);
                assert false;
                return "UNKNOWN";
        }
    }

    /**
     * Converts a type descriptor to human-readable "dotted" form.  For
     * example, "Ljava/lang/String;" becomes "java.lang.String", and
     * "[I" becomes "int[].
     */
    public static String descriptorToDot(String descr) {
        int targetLen = descr.length();
        int offset = 0;
        int arrayDepth = 0;

        /* strip leading [s; will be added to end */
        while (targetLen > 1 && descr.charAt(offset) == '[') {
            offset++;
            targetLen--;
        }
        arrayDepth = offset;

        if (targetLen == 1) {
            descr = primitiveTypeLabel(descr.charAt(offset));
            offset = 0;
            targetLen = descr.length();
        } else {
            /* account for leading 'L' and trailing ';' */
            if (targetLen >= 2 && descr.charAt(offset) == 'L'
                    && descr.charAt(offset + targetLen - 1) == ';') {
                targetLen -= 2;     /* two fewer chars to copy */
                offset++;           /* skip the 'L' */
            }
        }

        char[] buf = new char[targetLen + arrayDepth * 2];

        /* copy class name over */
        int i;
        for (i = 0; i < targetLen; i++) {
            char ch = descr.charAt(offset + i);
            buf[i] = (ch == '/') ? '.' : ch;
        }

        /* add the appopriate number of brackets for arrays */
        while (arrayDepth-- > 0) {
            buf[i++] = '[';
            buf[i++] = ']';
        }
        assert i == buf.length;

        return new String(buf);
    }

    /**
     * Extracts the class name from a type descriptor.
     */
    public static String classNameOnly(String typeName) {
        String dotted = descriptorToDot(typeName);

        int start = dotted.lastIndexOf(".");
        if (start < 0) {
            return dotted;
        } else {
            return dotted.substring(start + 1);
        }
    }

    /**
     * Extracts the package name from a type descriptor, and returns it in
     * dotted form.
     */
    public static String packageNameOnly(String typeName) {
        String dotted = descriptorToDot(typeName);

        int end = dotted.lastIndexOf(".");
        if (end < 0) {
            /* lives in default package */
            return "";
        } else {
            return dotted.substring(0, end);
        }
    }
}
