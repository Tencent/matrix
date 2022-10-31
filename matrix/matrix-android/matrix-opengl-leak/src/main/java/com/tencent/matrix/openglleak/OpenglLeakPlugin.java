package com.tencent.matrix.openglleak;

import android.app.Application;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.os.RemoteException;
import com.tencent.matrix.openglleak.comm.FuncNameString;
import com.tencent.matrix.openglleak.detector.IOpenglIndexDetector;
import com.tencent.matrix.openglleak.detector.OpenglIndexDetectorService;
import com.tencent.matrix.openglleak.hook.OpenGLHook;
import com.tencent.matrix.openglleak.statistics.resource.ResRecordManager;
import com.tencent.matrix.openglleak.utils.ActivityRecorder;
import com.tencent.matrix.openglleak.utils.EGLHelper;
import com.tencent.matrix.openglleak.utils.GlLeakHandlerThread;
import com.tencent.matrix.plugin.Plugin;
import com.tencent.matrix.plugin.PluginListener;
import com.tencent.matrix.util.MatrixLog;

import java.util.Map;

public class OpenglLeakPlugin extends Plugin {

    private static final String TAG = "matrix.OpenglLeakPlugin";

    public static OpenglReportCallback sCallback;

    private Context context;

    public OpenglLeakPlugin(Context c) {
        context = c;
    }

    public void registerReportCallback(OpenglReportCallback callback) {
        if (sCallback != null) {
            MatrixLog.e(TAG, "OpenglReportCallback register again, May be overwrite !!!");
        }
        sCallback = callback;
    }


    @Override
    public void init(Application app, PluginListener listener) {
        super.init(app, listener);

        ActivityRecorder.getInstance().start(app);
        GlLeakHandlerThread.getInstance().start();
    }

    @Override
    public void start() {
        super.start();

        new Thread(new Runnable() {
            @Override
            public void run() {
                startImpl();
            }
        }).start();
    }

    private void executeHook(IBinder iBinder) {
        MatrixLog.e(TAG, "onServiceConnected");
        IOpenglIndexDetector ipc = IOpenglIndexDetector.Stub.asInterface(iBinder);

        try {
            // 通过实验进程获取 index
            Map<String, Integer> map = ipc.seekIndex();
            if (map == null) {
                MatrixLog.e(TAG, "indexMap null");
                return;
            }
            MatrixLog.e(TAG, "indexMap:" + map);

            // 初始化
            EGLHelper.initOpenGL();
            OpenGLHook.getInstance().init();
            MatrixLog.e(TAG, "init env");

            int hookResult = map.get(FuncNameString.GL_GEN_TEXTURES) * map.get(FuncNameString.GL_DELETE_TEXTURES)
                    * map.get(FuncNameString.GL_GEN_BUFFERS) * map.get(FuncNameString.GL_DELETE_BUFFERS)
                    * map.get(FuncNameString.GL_GEN_FRAMEBUFFERS) * map.get(FuncNameString.GL_DELETE_FRAMEBUFFERS)
                    * map.get(FuncNameString.GL_GEN_RENDERBUFFERS) * map.get(FuncNameString.GL_DELETE_RENDERBUFFERS)
                    * map.get(FuncNameString.GL_TEX_IMAGE_2D) * map.get(FuncNameString.GL_BIND_TEXTURE)
                    * map.get(FuncNameString.GL_BIND_BUFFER) * map.get(FuncNameString.GL_BIND_FRAMEBUFFER)
                    * map.get(FuncNameString.GL_BIND_RENDERBUFFER) * map.get(FuncNameString.GL_TEX_IMAGE_3D)
                    * map.get(FuncNameString.GL_RENDER_BUFFER_STORAGE) * map.get(FuncNameString.GL_BUFFER_DATA);
            MatrixLog.e(TAG, "hookResult = " + hookResult);
            if (hookResult == 0) {
                if (OpenglLeakPlugin.sCallback != null) {
                    OpenglLeakPlugin.sCallback.onHookFail();
                }
            } else {
                if (OpenglLeakPlugin.sCallback != null) {
                    OpenglLeakPlugin.sCallback.onHookSuccess();
                }
            }

            // hook
            OpenGLHook.getInstance().hook(FuncNameString.GL_GEN_TEXTURES, map.get(FuncNameString.GL_GEN_TEXTURES));
            OpenGLHook.getInstance().hook(FuncNameString.GL_DELETE_TEXTURES, map.get(FuncNameString.GL_DELETE_TEXTURES));
            OpenGLHook.getInstance().hook(FuncNameString.GL_GEN_BUFFERS, map.get(FuncNameString.GL_GEN_BUFFERS));
            OpenGLHook.getInstance().hook(FuncNameString.GL_DELETE_BUFFERS, map.get(FuncNameString.GL_DELETE_BUFFERS));
            OpenGLHook.getInstance().hook(FuncNameString.GL_GEN_FRAMEBUFFERS, map.get(FuncNameString.GL_GEN_FRAMEBUFFERS));
            OpenGLHook.getInstance().hook(FuncNameString.GL_DELETE_FRAMEBUFFERS, map.get(FuncNameString.GL_DELETE_FRAMEBUFFERS));
            OpenGLHook.getInstance().hook(FuncNameString.GL_GEN_RENDERBUFFERS, map.get(FuncNameString.GL_GEN_RENDERBUFFERS));
            OpenGLHook.getInstance().hook(FuncNameString.GL_DELETE_RENDERBUFFERS, map.get(FuncNameString.GL_DELETE_RENDERBUFFERS));
            OpenGLHook.getInstance().hook(FuncNameString.GL_TEX_IMAGE_2D, map.get(FuncNameString.GL_TEX_IMAGE_2D));
            OpenGLHook.getInstance().hook(FuncNameString.GL_TEX_IMAGE_3D, map.get(FuncNameString.GL_TEX_IMAGE_3D));
            OpenGLHook.getInstance().hook(FuncNameString.GL_BIND_TEXTURE, map.get(FuncNameString.GL_BIND_TEXTURE));
            OpenGLHook.getInstance().hook(FuncNameString.GL_BIND_BUFFER, map.get(FuncNameString.GL_BIND_BUFFER));
//            OpenGLHook.getInstance().hook(FuncNameString.GL_BIND_FRAMEBUFFER, map.get(FuncNameString.GL_BIND_FRAMEBUFFER));
            OpenGLHook.getInstance().hook(FuncNameString.GL_BIND_RENDERBUFFER, map.get(FuncNameString.GL_BIND_RENDERBUFFER));
            OpenGLHook.getInstance().hook(FuncNameString.GL_BUFFER_DATA, map.get(FuncNameString.GL_BUFFER_DATA));
            OpenGLHook.getInstance().hook(FuncNameString.GL_RENDER_BUFFER_STORAGE, map.get(FuncNameString.GL_RENDER_BUFFER_STORAGE));
//            OpenGLHook.getInstance().hookEglCreate();
//            OpenGLHook.getInstance().hookEglDestory();
            MatrixLog.e(TAG, "hook finish");
        } catch (Throwable e) {
            e.printStackTrace();
        } finally {
            // 销毁实验进程
            try {
                MatrixLog.i(TAG, "destory test process");
                ipc.destory();
            } catch (RemoteException e) {
                e.printStackTrace();
            }
        }
    }

    private void startImpl() {
        Intent service = new Intent(context, OpenglIndexDetectorService.class);
        boolean result = false;
        try {
            result = context.bindService(service, new ServiceConnection() {
                @Override
                public void onServiceConnected(ComponentName componentName, final IBinder iBinder) {
                    new Thread(new Runnable() {
                        @Override
                        public void run() {
                            executeHook(iBinder);
                        }
                    }).start();
                }

                @Override
                public void onServiceDisconnected(ComponentName componentName) {
                    context.unbindService(this);
                }
            }, Context.BIND_AUTO_CREATE);
        } catch (Exception e) {
            MatrixLog.d(TAG, "bindService error = " + e.getCause());
        }

        MatrixLog.d(TAG, "bindService result = " + result);
        if (result) {
            if (OpenglLeakPlugin.sCallback != null) {
                OpenglLeakPlugin.sCallback.onExpProcessSuccess();
            }
        } else {
            if (OpenglLeakPlugin.sCallback != null) {
                OpenglLeakPlugin.sCallback.onExpProcessFail();
            }
        }

    }

    @Override
    public void stop() {
        super.stop();
    }

    @Override
    public void destroy() {
        super.destroy();
    }

    @Override
    public String getTag() {
        return "OpenglLeak";
    }

    public void setNativeStackDump(boolean open) {
        OpenGLHook.getInstance().setNativeStackDump(open);
    }

    public void setJavaStackDump(boolean open) {
        OpenGLHook.getInstance().setJavaStackDump(open);
    }

    public void clear() {
        ResRecordManager.getInstance().clear();
    }

}
