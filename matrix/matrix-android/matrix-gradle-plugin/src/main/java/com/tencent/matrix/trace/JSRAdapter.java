package com.tencent.matrix.trace;

import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.commons.JSRInlinerAdapter;

public class JSRAdapter extends JSRInlinerAdapter {
    JSRAdapter(int api, MethodVisitor mv, int access, String name, String desc, String signature, String[] exceptions) {
        super(api, mv, access, name, desc, signature, exceptions);
    }
}
