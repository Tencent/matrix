package com.tencent.mm.performance.jni.pthread;

import android.text.TextUtils;

import com.tencent.mm.performance.jni.AbsHook;
import com.tencent.mm.performance.jni.HookManager;
import com.tencent.mm.performance.jni.LibWxPerfManager;

import java.util.HashSet;
import java.util.Set;

/**
 * Created by Yves on 2020-03-11
 */
public class PthreadHook implements AbsHook {

    static {
        LibWxPerfManager.INSTANCE.init();
    }

    public static final PthreadHook INSTANCE = new PthreadHook();

    private Set<String> mHookSoSet            = new HashSet<>();
    private Set<String> mIgnoreSoSet          = new HashSet<>();
//    private Set<String> mHookParentThreadName = new HashSet<>();
    private Set<String> mHookThreadName       = new HashSet<>();

    private PthreadHook() {
    }

    public PthreadHook addHookSo(String regex) {
        if (TextUtils.isEmpty(regex)) {
            throw new IllegalArgumentException("so regex = " + regex);
        }
        mHookSoSet.add(regex);
        return this;
    }

    public PthreadHook addHookSo(String... regexArr) {
        for (String regex : regexArr) {
            addHookSo(regex);
        }
        return this;
    }

    public PthreadHook addIgnoreSo(String regex) {
        if (TextUtils.isEmpty(regex)) {
            return this;
        }
        mIgnoreSoSet.add(regex);
        return this;
    }

    public PthreadHook addIgnoreSo(String... regexArr) {
        for (String regex : regexArr) {
            addIgnoreSo(regex);
        }
        return this;
    }

    public PthreadHook addHookThread(String regex) {
        if (TextUtils.isEmpty(regex)) {
            throw new IllegalArgumentException("thread regex should NOT be empty");
        }
        mHookThreadName.add(regex);
        return this;
    }

    public PthreadHook addHookThread(String... regexArr) {
        for (String regex : regexArr) {
            addHookThread(regex);
        }
        return this;
    }

//    public PthreadHook addHookParentThread(String regex) {
//        if (TextUtils.isEmpty(regex)) {
//            throw new IllegalArgumentException("parent thread regen should NOT be empty");
//        }
//        mHookParentThreadName.add(regex);
//        return this;
//    }

//    public PthreadHook addHookParentThread(String... regexArr) {
//        for (String regex : regexArr) {
//            addHookParentThread(regex);
//        }
//        return this;
//    }

    /**
     * notice: it is an exclusive interface
     */
    public void hook() throws HookManager.HookFailedException {
        HookManager.INSTANCE
                .clearHooks()
                .addHook(this)
                .commitHooks();
    }

    public void dump(String path) {
        dumpNative(path);
    }

//    public static String getStack() {
//        return stackTraceToString(new Throwable().getStackTrace());
//    }
//
//    private static String stackTraceToString(final StackTraceElement[] arr) {
//        if (arr == null) {
//            return "";
//        }
//
//        StringBuilder sb = new StringBuilder();
//
//        for (StackTraceElement stackTraceElement : arr) {
//            String className = stackTraceElement.getClassName();
//            // remove unused stacks
//            if (className.contains("java.lang.Thread")) {
//                continue;
//            }
//
//            sb.append(stackTraceElement).append('\n');
//        }
//        return sb.toString();
//    }


    @Override
    public void onConfigure() {
//        addHookParentThreadNameNative(mHookParentThreadName.toArray(new String[0]));
        addHookThreadNameNative(mHookThreadName.toArray(new String[0]));
    }

    @Override
    public void onHook() {
        addHookSoNative(mHookSoSet.toArray(new String[0]));
        addIgnoreSoNative(mIgnoreSoSet.toArray(new String[0]));
    }

    private native void addHookSoNative(String[] hookSoList);

    private native void addIgnoreSoNative(String[] hookSoList);

//    private native void addHookParentThreadNameNative(String[] threadNames);

    private native void addHookThreadNameNative(String[] threadNames);

    private native void dumpNative(String path);

}
