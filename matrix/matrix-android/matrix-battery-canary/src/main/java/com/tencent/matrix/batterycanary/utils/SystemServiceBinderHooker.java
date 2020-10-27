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

package com.tencent.matrix.batterycanary.utils;

import android.os.IBinder;
import android.os.IInterface;

import com.tencent.matrix.util.MatrixLog;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.Map;

/**
 * @author liyongjie
 *         Created by liyongjie on 2017/10/30.
 */

public class SystemServiceBinderHooker {
    private static final String TAG = "Matrix.SystemServiceBinderHooker";

    public interface HookCallback {
        void onServiceMethodInvoke(Method method, Object[] args);
    }

    private final String mServiceName;
    private final String mServiceClsName;
    private final HookCallback mHookCallback;

    private IBinder mServiceOriginBinder;

    public SystemServiceBinderHooker(final String serviceName, final String serviceClsName, final HookCallback hookCallback) {
        mServiceName = serviceName;
        mServiceClsName = serviceClsName;
        mHookCallback = hookCallback;
    }

    public boolean doHook() {
        MatrixLog.i(TAG, "doHook: serviceName:%s, serviceClsName:%s", mServiceName, mServiceClsName);
        try {
            Class<?> serviceManagerCls = Class.forName("android.os.ServiceManager");
            Method getService = serviceManagerCls.getDeclaredMethod("getService", String.class);
            mServiceOriginBinder = (IBinder) getService.invoke(null, mServiceName);

            ClassLoader classLoader = serviceManagerCls.getClassLoader();
            if (classLoader == null) {
                MatrixLog.e(TAG, "doHook exp classLoader null ");
                return false;
            }

            IBinder hookedBinder = (IBinder) Proxy.newProxyInstance(classLoader,
                    new Class<?>[]{IBinder.class},
                    new BinderHookHandler(mServiceOriginBinder, createHookServiceProxy()));

            Field cacheField = serviceManagerCls.getDeclaredField("sCache");
            cacheField.setAccessible(true);
            Map<String, IBinder> cache = (Map) cacheField.get(null);
            cache.put(mServiceName, hookedBinder);
            return true;
        } catch (Throwable e) {
            MatrixLog.e(TAG, "doHook exp : " + e.getLocalizedMessage());
        }

        return false;
    }

    public boolean doUnHook() {
        if (mServiceOriginBinder == null) {
            MatrixLog.i(TAG, "doUnHook sOriginPowerManagerService null");
            return false;
        }

        try {
            Class<?> serviceManagerCls = Class.forName("android.os.ServiceManager");
            Field cacheField = serviceManagerCls.getDeclaredField("sCache");
            cacheField.setAccessible(true);
            Map<String, IBinder> cache = (Map) cacheField.get(null);
            cache.put(mServiceName, mServiceOriginBinder);
            return true;
        } catch (Throwable e) {
            MatrixLog.e(TAG, "doUnHook exp : " + e.getLocalizedMessage());
        }
        return false;
    }

    private static final class BinderHookHandler implements InvocationHandler {
        private final IBinder mOriginBinder;
        private final Object mServiceProxy;

        BinderHookHandler(IBinder originBinder, final Object serviceProxy) {
            mOriginBinder = originBinder;
            mServiceProxy = serviceProxy;
        }

        @Override
        public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
            if ("queryLocalInterface".equals(method.getName())) {
                if (mServiceProxy != null) {
                    return mServiceProxy;
                }
            }
            return method.invoke(mOriginBinder, args);
        }
    }

    private Object createHookServiceProxy() {
        try {
            Class<?> clsIPowerManager = Class.forName(mServiceClsName);
            Class<?> clsIPowerManagerStubCls = Class.forName(String.format("%s$Stub", mServiceClsName));
            Method asInterfaceMethod = clsIPowerManagerStubCls.getDeclaredMethod("asInterface", IBinder.class);
            Object originPowerManagerService = asInterfaceMethod.invoke(null, mServiceOriginBinder);
            ClassLoader classLoader = clsIPowerManagerStubCls.getClassLoader();
            if (classLoader == null) {
                MatrixLog.e(TAG, "doHook exp classLoader null ");
                return false;
            }
            return Proxy.newProxyInstance(classLoader,
                    new Class[]{IBinder.class, IInterface.class, clsIPowerManager},
                    new ServiceHookHandler(originPowerManagerService, mHookCallback));
        } catch (Throwable e) {
            MatrixLog.w(TAG, "createPowerManagerServiceProxy exp:%s", e.getLocalizedMessage());
        }
        return null;
    }

    private static class ServiceHookHandler implements InvocationHandler {
        final Object mOriginSystemService;
        final HookCallback mCallback;

        ServiceHookHandler(Object originPowerManagerService, HookCallback callback) {
            mOriginSystemService = originPowerManagerService;
            mCallback = callback;
        }

        @Override
        public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
            if (mCallback != null) {
                mCallback.onServiceMethodInvoke(method, args);
            }

            return method.invoke(mOriginSystemService, args);
        }
    }
}
