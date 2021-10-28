package com.tencent.matrix.trace;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassWriter;

/**
 * Created by habbyge on 2019/4/25.
 *
 * fix:
 * java.lang.TypeNotPresentException: Type android/content/res/TypedArray not present
 *     at org.objectweb.asm.ClassWriter.getCommonSuperClass(ClassWriter.java:1025)
 *     at org.objectweb.asm.SymbolTable.addMergedType(SymbolTable.java:1202)
 *     at org.objectweb.asm.Frame.merge(Frame.java:1299)
 *     at org.objectweb.asm.Frame.merge(Frame.java:1197)
 *     at org.objectweb.asm.MethodWriter.computeAllFrames(MethodWriter.java:1610)
 *     at org.objectweb.asm.MethodWriter.visitMaxs(MethodWriter.java:1546)
 *     at org.objectweb.asm.tree.MethodNode.accept(MethodNode.java:769)
 *     at org.objectweb.asm.util.CheckMethodAdapter$1.visitEnd(CheckMethodAdapter.java:465)
 *     at org.objectweb.asm.MethodVisitor.visitEnd(MethodVisitor.java:783)
 *     at org.objectweb.asm.util.CheckMethodAdapter.visitEnd(CheckMethodAdapter.java:1036)
 *     at org.objectweb.asm.ClassReader.readMethod(ClassReader.java:1495)
 *     at org.objectweb.asm.ClassReader.accept(ClassReader.java:721)
 *
 */
class TraceClassWriter extends ClassWriter {
    private ClassLoader mClassLoader;
    public TraceClassWriter(int flags, ClassLoader classLoader) {
        super(flags);
        mClassLoader = classLoader;
    }

    public TraceClassWriter(ClassReader classReader, int flags, ClassLoader classLoader) {
        super(classReader, flags);
        mClassLoader = classLoader;
    }

    @Override
    protected String getCommonSuperClass(String type1, String type2) {
        try {
            return super.getCommonSuperClass(type1, type2);
        } catch (Exception e) {
            try {
                return getCommonSuperClassV1(type1, type2);
            } catch (Exception e1) {
                try {
                    return getCommonSuperClassV2(type1, type2);
                } catch (Exception e2) {
                    return getCommonSuperClassV3(type1, type2);
                }
            }
        }
    }

    private String getCommonSuperClassV1(String type1, String type2) {
        ClassLoader curClassLoader = getClass().getClassLoader();

        Class clazz1, clazz2;
        try {
            clazz1 = Class.forName(type1.replace('/', '.'), false, mClassLoader);
            clazz2 = Class.forName(type2.replace('/', '.'), false, mClassLoader);
        } catch (Exception e) {
            /*throw new RuntimeException(e.toString());*/
            /*e.printStackTrace();*/
            return "java/lang/Object";
        } catch (LinkageError error) {
            return "java/lang/Object";
        }
        //noinspection unchecked
        if (clazz1.isAssignableFrom(clazz2)) {
            return type1;
        }
        //noinspection unchecked
        if (clazz2.isAssignableFrom(clazz1)) {
            return type2;
        }
        if (clazz1.isInterface() || clazz2.isInterface()) {
            return "java/lang/Object";
        } else {
            //noinspection unchecked
            do {
                clazz1 = clazz1.getSuperclass();
            } while (!clazz1.isAssignableFrom(clazz2));
            return clazz1.getName().replace('.', '/');
        }
    }

    private String getCommonSuperClassV2(String type1, String type2) {
        ClassLoader curClassLoader = getClass().getClassLoader();

        Class clazz1, clazz2;
        try {
            clazz1 = Class.forName(type1.replace('/', '.'), false, curClassLoader);
            clazz2 = Class.forName(type2.replace('/', '.'), false, mClassLoader);
        } catch (Exception e) {
            /*throw new RuntimeException(e.toString());*/
            /*e.printStackTrace();*/
            return "java/lang/Object";
        } catch (LinkageError error) {
            return "java/lang/Object";
        }
        //noinspection unchecked
        if (clazz1.isAssignableFrom(clazz2)) {
            return type1;
        }
        //noinspection unchecked
        if (clazz2.isAssignableFrom(clazz1)) {
            return type2;
        }
        if (clazz1.isInterface() || clazz2.isInterface()) {
            return "java/lang/Object";
        } else {
            //noinspection unchecked
            do {
                clazz1 = clazz1.getSuperclass();
            } while (!clazz1.isAssignableFrom(clazz2));
            return clazz1.getName().replace('.', '/');
        }
    }

    private String getCommonSuperClassV3(String type1, String type2) {
        ClassLoader curClassLoader = getClass().getClassLoader();

        Class clazz1, clazz2;
        try {
            clazz1 = Class.forName(type1.replace('/', '.'), false, mClassLoader);
            clazz2 = Class.forName(type2.replace('/', '.'), false, curClassLoader);
        } catch (Exception e) {
            /*e.printStackTrace();*/
            /*throw new RuntimeException(e.toString());*/
            return "java/lang/Object";
        } catch (LinkageError error) {
            return "java/lang/Object";
        }
        //noinspection unchecked
        if (clazz1.isAssignableFrom(clazz2)) {
            return type1;
        }
        //noinspection unchecked
        if (clazz2.isAssignableFrom(clazz1)) {
            return type2;
        }
        if (clazz1.isInterface() || clazz2.isInterface()) {
            return "java/lang/Object";
        } else {
            //noinspection unchecked
            do {
                clazz1 = clazz1.getSuperclass();
            } while (!clazz1.isAssignableFrom(clazz2));
            return clazz1.getName().replace('.', '/');
        }
    }
}
