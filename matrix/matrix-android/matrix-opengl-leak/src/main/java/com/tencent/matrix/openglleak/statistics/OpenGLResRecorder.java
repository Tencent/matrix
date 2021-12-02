package com.tencent.matrix.openglleak.statistics;

import android.annotation.SuppressLint;
import android.text.TextUtils;
import android.util.Log;

import com.tencent.matrix.util.MatrixLog;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;

import com.tencent.matrix.openglleak.utils.ExecuteCenter;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

public class OpenGLResRecorder {

    private final List<OpenGLInfo> infoList = new ArrayList<>();

    private static final OpenGLResRecorder mInstance = new OpenGLResRecorder();


    private LeakMonitor.LeakListener mListener;

    private static final String TAG = "Matrix.OpenGLResRecorder";

    private OpenGLResRecorder() {
    }

    public static OpenGLResRecorder getInstance() {
        return mInstance;
    }

    public void setLeakListener(LeakMonitor.LeakListener mListener) {
        this.mListener = mListener;
    }

    public void gen(final OpenGLInfo oinfo) {
        ExecuteCenter.getInstance().post(new Runnable() {
            @Override
            public void run() {
                if (oinfo == null) {
                    return;
                }

                synchronized (infoList) {
                    infoList.add(oinfo);
                }
            }
        });
    }

    public void delete(final OpenGLInfo del) {
        ExecuteCenter.getInstance().post(new Runnable() {
            @Override
            public void run() {
                if (del == null) {
                    return;
                }

                synchronized (infoList) {
                    Iterator<OpenGLInfo> iterator = infoList.iterator();
                    while (iterator.hasNext()) {
                        OpenGLInfo gen = iterator.next();
                        if (gen == null) {
                            continue;
                        }

                        if ((gen.getType() == del.getType()) && gen.getId() == del.getId()) {
                            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
                                if (gen.getEglContextNativeHandle() == del.getEglContextNativeHandle()) {
                                    gen.release();
                                    iterator.remove();
                                    break;
                                }
                            } else if (gen.getThreadId().equals(del.getThreadId())) {
                                gen.release();
                                iterator.remove();
                                break;
                            }
                        }
                    }
                }
            }
        });
    }

    public List<OpenGLInfo> getCopyList() {
        List<OpenGLInfo> ll = new ArrayList<>();

        synchronized (infoList) {
            for (OpenGLInfo item : infoList) {
                if (item == null) {
                    break;
                }

                OpenGLInfo clone = new OpenGLInfo(item);
                ll.add(clone);
            }
        }

        return ll;
    }

    public void remove(final OpenGLInfo info) {
        ExecuteCenter.getInstance().post(new Runnable() {
            @Override
            public void run() {
                if (infoList == null) {
                    return;
                }
                synchronized (infoList) {
                    infoList.remove(info);
                }
            }
        });
    }

    public void replace(final OpenGLInfo info) {
        ExecuteCenter.getInstance().post(new Runnable() {
            @Override
            public void run() {
                if (infoList == null) {
                    return;
                }
                synchronized (infoList) {
                    for (int i = 0; i < infoList.size(); i++) {
                        if (info == infoList.get(i)) {
                            infoList.set(i, info);
                            return;
                        }
                    }
                }
            }
        });
    }

    public List<Integer> getAllHashCode() {
        List<Integer> ll = new ArrayList<>();

        synchronized (infoList) {
            for (OpenGLInfo item : infoList) {
                if (item == null) {
                    break;
                }

                ll.add(item.hashCode());
            }
        }

        return ll;
    }

    @SuppressLint("LongLogTag")
    public void dumpToFile(String filePath) {
        File targetFile = new File(filePath);

        if (targetFile.exists()) {
            targetFile.delete();
        }

        try {
            targetFile.createNewFile();
        } catch (IOException e) {
            Log.e(TAG, "[OpenGLResRecorder]: createNewFile Fail, filePath = " + filePath);
        }

        try (BufferedWriter writer = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(targetFile)))) {
            writer.write(dumpToString());
        } catch (IOException e) {
            Log.e(TAG, "[OpenGLResRecorder]: Create Stream Fail");
        }

    }


    @SuppressLint("DefaultLocale")
    public String dumpToString() {
        StringBuilder result = new StringBuilder();
        List<OpenGLInfo> openGLInfos = getCopyList();
        if (openGLInfos == null || openGLInfos.size() == 0) {
            MatrixLog.e(TAG, "OpenGLResRecorder.getCopyList() is empty, break dump!!!");
            return "";
        }
        Map<Long, OpenGLInfo> infoMap = new HashMap<>();
        for (int i = 0; i < openGLInfos.size(); i++) {

            OpenGLInfo info = openGLInfos.get(i);
            int javaHash = info.getJavaStack().hashCode();
            int nativeHash = info.getNativeStack().hashCode();

            MemoryInfo memoryInfo = info.getMemoryInfo();
            int memoryJavaHash = memoryInfo == null ? 0 : memoryInfo.getJavaStack().hashCode();
            int memoryNativeHash = memoryInfo == null ? 0 : info.getNativeStack().hashCode();

            long infoHash = javaHash + nativeHash + memoryNativeHash + memoryJavaHash;

            OpenGLInfo oldInfo = infoMap.get(infoHash);
            if (oldInfo == null) {
                infoMap.put(infoHash, info);
            } else {
                // resource part
                boolean isSameType = info.getType() == oldInfo.getType();
                boolean isSameThread = info.getThreadId().equals(oldInfo.getThreadId());
                boolean isSameEglContext = info.getEglContextNativeHandle() == oldInfo.getEglContextNativeHandle();
                boolean isSameActivity = info.getActivityName().equals(oldInfo.getActivityName());

                if (isSameType && isSameThread && isSameEglContext && isSameActivity) {
                    oldInfo.incAllocRecord(info.getId());
                    MemoryInfo oldMemory = oldInfo.getMemoryInfo();
                    if (oldMemory != null) {
                        oldMemory.appendParamsInfos(info.getMemoryInfo());
                    }
                    infoMap.put(infoHash, oldInfo);
                }
            }
        }

        List<OpenGLInfo> textureList = new ArrayList<>();
        List<OpenGLInfo> bufferList = new ArrayList<>();
        List<OpenGLInfo> framebufferList = new ArrayList<>();
        List<OpenGLInfo> renderbufferList = new ArrayList<>();

        for (OpenGLInfo openGLInfo : infoMap.values()) {
            if (openGLInfo.getType() == OpenGLInfo.TYPE.TEXTURE) {
                textureList.add(openGLInfo);
            }
            if (openGLInfo.getType() == OpenGLInfo.TYPE.BUFFER) {
                bufferList.add(openGLInfo);
            }
            if (openGLInfo.getType() == OpenGLInfo.TYPE.FRAME_BUFFERS) {
                framebufferList.add(openGLInfo);
            }
            if (openGLInfo.getType() == OpenGLInfo.TYPE.RENDER_BUFFERS) {
                renderbufferList.add(openGLInfo);
            }
        }

        Comparator<OpenGLInfo> comparator = new Comparator<OpenGLInfo>() {
            @Override
            public int compare(OpenGLInfo o1, OpenGLInfo o2) {
                return o1.getAllocCount() - o2.getAllocCount();
            }
        };

        Collections.sort(textureList, comparator);
        Collections.sort(bufferList, comparator);
        Collections.sort(framebufferList, comparator);
        Collections.sort(renderbufferList, comparator);

        final String dottedLine = "-------------------------------------------------------------------------";
        final String waveLine = "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<";

        result.append(dottedLine)
                .append("\n")
                .append(String.format("\t\t\ttextures Count = %d\n", textureList.size()))
                .append(String.format("\t\t\tbuffer Count = %d\n", bufferList.size()))
                .append(String.format("\t\t\tframebuffer Count = %d\n", framebufferList.size()))
                .append(String.format("\t\t\trenderbuffer Count = %d\n", renderbufferList.size()))
                .append(dottedLine)
                .append("\n")
                .append(waveLine)
                .append("\n")
                .append("\t\t\ttexture part :\n")
                .append(waveLine)
                .append("\n")
                .append(getResListString(textureList))
                .append('\n')
                .append(waveLine)
                .append("\n")
                .append("\t\t\tbuffers part :\n")
                .append(waveLine)
                .append("\n")
                .append(getResListString(bufferList))
                .append("\n")
                .append(waveLine)
                .append("\n")
                .append("\t\t\trenderbuffer part :\n")
                .append(waveLine)
                .append("\n")
                .append(getResListString(renderbufferList))
                .append("\n");

        return result.toString();
    }

    @SuppressLint("DefaultLocale")
    private String getResListString(List<OpenGLInfo> resList) {
        if (resList == null || resList.size() == 0) {
            return "";
        }

        StringBuilder result = new StringBuilder();
        for (OpenGLInfo res : resList) {
            result.append(String.format(" alloc count = %d", res.getAllocCount()))
                    .append("\n")
                    .append(String.format(" id = %s", res.getAllocIdList()))
                    .append("\n")
                    .append(String.format(" activity = %s", res.getActivityName()))
                    .append("\n")
                    .append(String.format(" type = %s", res.getType()))
                    .append("\n")
                    .append(String.format(" eglContext = %s", res.getEglContextNativeHandle()))
                    .append("\n")
                    .append(String.format(" java stack = %s", res.getJavaStack()))
                    .append("\n")
                    .append(String.format(" native stack = %s", res.getNativeStack()))
                    .append("\n")
                    .append(res.getMemoryInfo() == null ? "" : getMemoryInfoStr(res.getMemoryInfo()))
                    .append("\n\n\n");
        }
        return result.toString();
    }

    private String getMemoryInfoStr(MemoryInfo memory) {
        return " " + memory.getParamsInfos() +
                "\n" +
                String.format(" memory java stack = %s", memory.getJavaStack()) +
                "\n" +
                String.format(" memory native stack = %s", memory.getNativeStack());
    }

    public void getNativeStack(OpenGLInfo info) {
        synchronized (infoList) {
            for (OpenGLInfo item : infoList) {
                if (item == null) {
                    break;
                }

                if (item.getType() == info.getType()) {
                    if (item.getThreadId().equals(info.getThreadId())) {
                        if (item.getId() == info.getId()) {
                            String stack = item.getNativeStack();
                            info.setNativeStack(stack);
                            return;
                        }
                    }
                }
            }
        }
    }

    public void setLeak(OpenGLInfo info) {
        synchronized (infoList) {
            Iterator<OpenGLInfo> iterator = infoList.iterator();

            while (iterator.hasNext()) {
                OpenGLInfo item = iterator.next();
                if (item == null) {
                    break;
                }

//                if (isNeedIgnore(info)) {
//                    item.release();
//                    iterator.remove();
//                    continue;
//                }

                if ((item.getType() == info.getType()) && (item.getThreadId().equals(info.getThreadId())) && (item.getId() == info.getId())) {
                    if (mListener != null) {
                        mListener.onLeak(item);
                    }

                    item.release();
                    iterator.remove();
                    return;
                }
            }
        }
    }

    public void setMaybeLeak(OpenGLInfo info) {
        synchronized (infoList) {
            for (OpenGLInfo item : infoList) {
                if (item == null) {
                    break;
                }

//                if (isNeedIgnore(info)) {
//                    continue;
//                }

                if ((item.getType() == info.getType()) && (item.getThreadId().equals(info.getThreadId())) && (item.getId() == info.getId())) {
                    item.setMaybeLeak(true);
                    info.setMaybeLeak(true);

                    long now = System.currentTimeMillis();
                    item.setMaybeLeakCheckTime(now);
                    info.setMaybeLeakCheckTime(now);

                    if (mListener != null) {
                        mListener.onMaybeLeak(item);
                    }

                    return;
                }
            }
        }
    }

    private boolean isNeedIgnore(OpenGLInfo info) {
        if (TextUtils.isEmpty(info.getJavaStack())) {
            boolean isIgnore = true;

            String nativeStack = info.getNativeStack();
            if (!TextUtils.isEmpty(nativeStack)) {
                String[] lines = nativeStack.split("\n");
                for (String line : lines) {
                    if (!TextUtils.isEmpty(line)) {
                        if (!line.contains("libmatrix-opengl-leak.so")
                                && !line.contains("libwechatbacktrace.so")
                                && !line.contains("libGLESv1_CM.so")
                                && !line.contains("libhwui.so")
                                && !line.contains("libutils.so")
                                && !line.contains("libandroid_runtime.so")
                                && !line.contains("libc.so")
                                && line.contains(".so")) {
                            isIgnore = false;
                            break;
                        }
                    }
                }
            }

            return isIgnore;
        }

        return false;
    }

    public OpenGLInfo getItemByHashCode(int hashCode) {
        synchronized (infoList) {
            for (OpenGLInfo item : infoList) {
                if (item == null) {
                    break;
                }

                if (item.hashCode() == hashCode) {
                    return item;
                }
            }
        }

        return null;
    }

    public OpenGLInfo getItemByEGLContextAndId(OpenGLInfo.TYPE type, long eglContext, int id) {
        synchronized (infoList) {
            for (OpenGLInfo item : infoList) {
                if (item == null) {
                    break;
                }

                if (type == item.getType() && item.getEglContextNativeHandle() == eglContext && item.getId() == id) {
                    return item;
                }
            }
        }

        return null;
    }

}
