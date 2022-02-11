package com.tencent.matrix.openglleak.statistics.resource;

import android.annotation.SuppressLint;

import com.tencent.matrix.openglleak.hook.OpenGLHook;
import com.tencent.matrix.openglleak.utils.AutoWrapBuilder;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.AtomicInteger;

public class ResRecordManager {

    private static final ResRecordManager mInstance = new ResRecordManager();

    private final List<Callback> mCallbackList = new LinkedList<>();
    private final List<OpenGLInfo> mInfoList = new LinkedList<>();
    private final List<Long> mReleaseContext = new LinkedList<>();

    private ResRecordManager() {

    }

    public static ResRecordManager getInstance() {
        return mInstance;
    }

    public void gen(final OpenGLInfo gen) {
        if (gen == null) {
            return;
        }

        synchronized (mInfoList) {
            mInfoList.add(gen);
        }

        synchronized (mCallbackList) {
            for (Callback cb : mCallbackList) {
                if (null != cb) {
                    cb.gen(gen);
                }
            }
        }
    }

    public void delete(final OpenGLInfo del) {
        if (del == null) {
            return;
        }

        synchronized (mInfoList) {
            // 之前可能释放过
            int index = mInfoList.indexOf(del);
            if (-1 == index) {
                return;
            }

            OpenGLInfo info = mInfoList.get(index);
            if (null == info) {
                return;
            }

            AtomicInteger counter = info.getCounter();
            counter.set(counter.get() - 1);
            if (counter.get() == 0) {
                OpenGLHook.releaseNative(info.getNativeStackPtr());
            }

            // 释放 memory info
            MemoryInfo memoryInfo = info.getMemoryInfo();
            if (null != memoryInfo) {
                long memNativePtr = memoryInfo.getNativeStackPtr();
                if (memNativePtr != 0) {
                    OpenGLHook.releaseNative(memNativePtr);
                    memoryInfo.releaseNativeStackPtr();
                }
            }

            mInfoList.remove(del);
        }

        synchronized (mCallbackList) {
            for (Callback cb : mCallbackList) {
                if (null != cb) {
                    cb.delete(del);
                }
            }
        }
    }

    public OpenGLInfo findOpenGLInfo(OpenGLInfo.TYPE type, long eglContextId, int openGLInfoId) {
        synchronized (mInfoList) {
            for (OpenGLInfo item : mInfoList) {
                if (item == null) {
                    break;
                }

                if (type == item.getType() && item.getEglContextNativeHandle() == eglContextId && item.getId() == openGLInfoId) {
                    return item;
                }
            }
        }
        return null;
    }

    public String getNativeStack(OpenGLInfo item) {
        synchronized (mInfoList) {
            // 之前可能释放过
            int index = mInfoList.indexOf(item);
            if (-1 == index) {
                return "res already released, can not get native stack";
            }

            String ret = "";

            OpenGLInfo info = mInfoList.get(index);
            if (info == null) {
                return ret;
            }
            long nativeStackPtr = info.getNativeStackPtr();
            if (nativeStackPtr != 0L) {
                ret = OpenGLHook.dumpNativeStack(nativeStackPtr);
            }

            return ret;
        }
    }

    protected void registerCallback(Callback callback) {
        if (null == callback) {
            return;
        }

        synchronized (mCallbackList) {
            if (mCallbackList.contains(callback)) {
                return;
            }

            mCallbackList.add(callback);
        }
    }

    protected void unregisterCallback(Callback callback) {
        if (null == callback) {
            return;
        }
        synchronized (mCallbackList) {
            mCallbackList.remove(callback);
        }
    }

    public boolean isGLInfoRelease(OpenGLInfo item) {
        synchronized (mInfoList) {
            return !mInfoList.contains(item);
        }
    }

    public void clear() {
        synchronized (mInfoList) {
            mInfoList.clear();
        }
        synchronized (mReleaseContext) {
            mReleaseContext.clear();
        }
    }

    public boolean isEglContextReleased(OpenGLInfo info) {
        synchronized (mReleaseContext) {
            long eglContextNativeHandle = info.getEglContextNativeHandle();
            if (0L == eglContextNativeHandle) {
                return true;
            }

            for (long item : mReleaseContext) {
                if (item == eglContextNativeHandle) {
                    return true;
                }
            }

            boolean alive = OpenGLHook.isEglContextAlive(info.getEglContextNativeHandle());
            if (!alive) {
                mReleaseContext.add(info.getEglContextNativeHandle());
            }
            return !alive;
        }
    }

    public List<OpenGLInfo> getAllItem() {
        List<OpenGLInfo> retList = new LinkedList<>();

        synchronized (mInfoList) {
            for (OpenGLInfo item : mInfoList) {
                if (null != item) {
                    retList.add(item);
                }
            }
        }

        return retList;
    }

    public void dumpGLToFile(String filePath) {
        File targetFile = new File(filePath);

        if (targetFile.exists()) {
            targetFile.delete();
        }

        try {
            targetFile.createNewFile();
        } catch (IOException ignored) {
        }

        try (BufferedWriter writer = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(targetFile)))) {
            writer.write(dumpGLToString());
        } catch (IOException ignored) {
        }
    }

    @SuppressLint("DefaultLocale")
    private String getResListString(List<OpenGLDumpInfo> resList) {
        AutoWrapBuilder result = new AutoWrapBuilder();
        for (OpenGLDumpInfo report : resList) {
            result.append(String.format(" alloc count = %d", report.getAllocCount()))
                    .append(String.format(" total size = %s", report.getTotalSize()))
                    .append(String.format(" id = %s", report.getAllocIdList()))
                    .append(String.format(" activity = %s", report.innerInfo.getActivityInfo().name))
                    .append(String.format(" type = %s", report.innerInfo.getType()))
                    .append(String.format(" eglContext = %s", report.innerInfo.getEglContextNativeHandle()))
                    .append(String.format(" java stack = %s", report.innerInfo.getJavaStack()))
                    .append(String.format(" native stack = %s", report.innerInfo.getNativeStack()))
                    .append(report.innerInfo.getMemoryInfo() == null ? "" : getMemoryInfoStr(report))
                    .wrap();
        }
        return result.toString();
    }

    private String getMemoryInfoStr(OpenGLDumpInfo reportInfo) {
        return reportInfo.getParamsInfos() +
                "\n" +
                String.format(" memory java stack = %s", reportInfo.innerInfo.getMemoryInfo().getJavaStack()) +
                "\n" +
                String.format(" memory native stack = %s", reportInfo.innerInfo.getMemoryInfo().getNativeStack());
    }

    @SuppressLint("DefaultLocale")
    public String dumpGLToString() {
        Map<Long, OpenGLDumpInfo> infoMap = new HashMap<>();
        for (int i = 0; i < mInfoList.size(); i++) {

            OpenGLInfo info = mInfoList.get(i);
            int javaHash = info.getJavaStack().hashCode();
            int nativeHash = info.getNativeStack().hashCode();

            MemoryInfo memoryInfo = info.getMemoryInfo();
            int memoryJavaHash = memoryInfo == null ? 0 : memoryInfo.getJavaStack().hashCode();
            int memoryNativeHash = memoryInfo == null ? 0 : memoryInfo.getNativeStack().hashCode();

            long infoHash = javaHash + nativeHash + memoryNativeHash + memoryJavaHash
                    + info.getEglContextNativeHandle() + info.getActivityInfo().hashCode() + info.getThreadId().hashCode();

            OpenGLDumpInfo oldInfo = infoMap.get(infoHash);
            if (oldInfo == null) {
                OpenGLDumpInfo openGLDumpInfo = new OpenGLDumpInfo(info);
                infoMap.put(infoHash, openGLDumpInfo);
            } else {
                // resource part
                boolean isSameType = info.getType() == oldInfo.innerInfo.getType();
                boolean isSameThread = info.getThreadId().equals(oldInfo.innerInfo.getThreadId());
                boolean isSameEglContext = info.getEglContextNativeHandle() == oldInfo.innerInfo.getEglContextNativeHandle();
                boolean isSameActivity = info.getActivityInfo().equals(oldInfo.innerInfo.getActivityInfo());

                if (isSameType && isSameThread && isSameEglContext && isSameActivity) {
                    oldInfo.incAllocRecord(info.getId());
                    if (oldInfo.innerInfo.getMemoryInfo() != null) {
                        oldInfo.appendParamsInfos(info.getMemoryInfo());
                        oldInfo.appendSize(info.getMemoryInfo().getSize());
                    }
                    infoMap.put(infoHash, oldInfo);
                }
            }
        }

        List<OpenGLDumpInfo> textureList = new ArrayList<>();
        List<OpenGLDumpInfo> bufferList = new ArrayList<>();
        List<OpenGLDumpInfo> framebufferList = new ArrayList<>();
        List<OpenGLDumpInfo> renderbufferList = new ArrayList<>();

        for (OpenGLDumpInfo reportInfo : infoMap.values()) {
            if (reportInfo.innerInfo.getType() == OpenGLInfo.TYPE.TEXTURE) {
                textureList.add(reportInfo);
            }
            if (reportInfo.innerInfo.getType() == OpenGLInfo.TYPE.BUFFER) {
                bufferList.add(reportInfo);
            }
            if (reportInfo.innerInfo.getType() == OpenGLInfo.TYPE.FRAME_BUFFERS) {
                framebufferList.add(reportInfo);
            }
            if (reportInfo.innerInfo.getType() == OpenGLInfo.TYPE.RENDER_BUFFERS) {
                renderbufferList.add(reportInfo);
            }
        }

        Comparator<OpenGLDumpInfo> comparator = new Comparator<OpenGLDumpInfo>() {
            @Override
            public int compare(OpenGLDumpInfo o1, OpenGLDumpInfo o2) {
                if (o2.getTotalSize() - o1.getTotalSize() > 0) {
                    return 1;
                } else if (o2.getTotalSize() - o1.getTotalSize() == 0) {
                    return 0;
                } else {
                    return -1;
                }
            }
        };

        Collections.sort(textureList, comparator);
        Collections.sort(bufferList, comparator);
        Collections.sort(framebufferList, comparator);
        Collections.sort(renderbufferList, comparator);

        AutoWrapBuilder builder = new AutoWrapBuilder();
        builder.appendDotted()
                .appendWithSpace(String.format("textures Count = %d", textureList.size()), 3)
                .appendWithSpace(String.format("buffer Count = %d", bufferList.size()), 3)
                .appendWithSpace(String.format("framebuffer Count = %d", framebufferList.size()), 3)
                .appendWithSpace(String.format("renderbuffer Count = %d", renderbufferList.size()), 3)
                .appendDotted()
                .appendWave()
                .appendWithSpace("texture part :", 3)
                .appendWave()
                .append(getResListString(textureList))
                .appendWave()
                .appendWithSpace("buffers part :", 3)
                .appendWave()
                .append(getResListString(bufferList))
                .appendWave()
                .appendWithSpace("renderbuffer part :", 3)
                .appendWave()
                .append(getResListString(renderbufferList))
                .wrap();

        return builder.toString();
    }

    interface Callback {
        void gen(OpenGLInfo res);

        void delete(OpenGLInfo res);
    }

}
