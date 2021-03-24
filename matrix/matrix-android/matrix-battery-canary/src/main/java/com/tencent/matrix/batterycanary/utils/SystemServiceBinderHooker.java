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
import android.support.annotation.Nullable;
import android.support.annotation.RestrictTo;

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
@RestrictTo(RestrictTo.Scope.LIBRARY)
public class SystemServiceBinderHooker {
    private static final String TAG = "Matrix.battery.SystemServiceBinderHooker";

    public interface HookCallback {
        void onServiceMethodInvoke(Method method, Object[] args);
        @Nullable Object onServiceMethodIntercept(Object receiver, Method method, Object[] args) throws Throwable;
    }

    private final String mServiceName;
    private final String mServiceClass;
    private final HookCallback mHookCallback;

    @Nullable private IBinder mOriginServiceBinder;
    @Nullable private IBinder mDelegateServiceBinder;

    public SystemServiceBinderHooker(final String serviceName, final String serviceClass, final HookCallback hookCallback) {
        mServiceName = serviceName;
        mServiceClass = serviceClass;
        mHookCallback = hookCallback;
    }

    @SuppressWarnings({"PrivateApi", "unchecked", "rawtypes"})
    public boolean doHook() {
        MatrixLog.i(TAG, "doHook: serviceName:%s, serviceClsName:%s", mServiceName, mServiceClass);
        try {
            BinderProxyHandler binderProxyHandler = new BinderProxyHandler(mServiceName, mServiceClass, mHookCallback);
            IBinder delegateBinder = binderProxyHandler.createProxyBinder();

            Class<?> serviceManagerCls = Class.forName("android.os.ServiceManager");
            Field cacheField = serviceManagerCls.getDeclaredField("sCache");
            cacheField.setAccessible(true);
            Map<String, IBinder> cache = (Map) cacheField.get(null);
            cache.put(mServiceName, delegateBinder);

            mDelegateServiceBinder = delegateBinder;
            mOriginServiceBinder = binderProxyHandler.getOriginBinder();
            return true;

        } catch (Throwable e) {
            MatrixLog.e(TAG, "#doHook exp: " + e.getLocalizedMessage());
        }
        return false;
    }

    @SuppressWarnings({"PrivateApi", "unchecked", "rawtypes"})
    public boolean doUnHook() {
        if (mOriginServiceBinder == null) {
            MatrixLog.w(TAG, "#doUnHook mOriginServiceBinder null");
            return false;
        }
        if (mDelegateServiceBinder == null) {
            MatrixLog.w(TAG, "#doUnHook mDelegateServiceBinder null");
            return false;
        }

        try {
            IBinder currentBinder = BinderProxyHandler.getCurrentBinder(mServiceName);
            if (mDelegateServiceBinder != currentBinder) {
                MatrixLog.w(TAG, "#doUnHook mDelegateServiceBinder != currentBinder");
                return false;
            }

            Class<?> serviceManagerCls = Class.forName("android.os.ServiceManager");
            Field cacheField = serviceManagerCls.getDeclaredField("sCache");
            cacheField.setAccessible(true);
            Map<String, IBinder> cache = (Map) cacheField.get(null);
            cache.put(mServiceName, mOriginServiceBinder);
            return true;
        } catch (Throwable e) {
            MatrixLog.e(TAG, "#doUnHook exp: " + e.getLocalizedMessage());
        }
        return false;
    }


    static final class BinderProxyHandler implements InvocationHandler {
        private final IBinder mOriginBinder;
        private final Object mServiceManagerProxy;

        BinderProxyHandler(String serviceName, String serviceClass, HookCallback callback) throws Exception {
            mOriginBinder = getCurrentBinder(serviceName);
            mServiceManagerProxy = createServiceManagerProxy(serviceClass, mOriginBinder, callback);
        }

        @Override
        public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
            if ("queryLocalInterface".equals(method.getName())) {
                return mServiceManagerProxy;
            }
            return method.invoke(mOriginBinder, args);
        }

        public IBinder getOriginBinder() {
            return mOriginBinder;
        }

        @SuppressWarnings({"PrivateApi"})
        public IBinder createProxyBinder() throws Exception  {
            Class<?> serviceManagerCls = Class.forName("android.os.ServiceManager");
            ClassLoader classLoader = serviceManagerCls.getClassLoader();
            if (classLoader == null) {
                throw new IllegalStateException("Can not get ClassLoader of " + serviceManagerCls.getName());
            }
            return (IBinder) Proxy.newProxyInstance(
                    classLoader,
                    new Class<?>[]{IBinder.class},
                    this
            );
        }

        @SuppressWarnings({"PrivateApi"})
        static IBinder getCurrentBinder(String serviceName) throws Exception {
            Class<?> serviceManagerCls = Class.forName("android.os.ServiceManager");
            Method getService = serviceManagerCls.getDeclaredMethod("getService", String.class);
            return  (IBinder) getService.invoke(null, serviceName);
        }

        @SuppressWarnings({"PrivateApi"})
        private static Object createServiceManagerProxy(String serviceClassName, IBinder originBinder, final HookCallback callback) throws Exception  {
            Class<?> serviceManagerCls = Class.forName(serviceClassName);
            Class<?> serviceManagerStubCls = Class.forName(serviceClassName + "$Stub");
            ClassLoader classLoader = serviceManagerStubCls.getClassLoader();
            if (classLoader == null) {
                throw new IllegalStateException("get service manager ClassLoader fail!");
            }
            Method asInterfaceMethod = serviceManagerStubCls.getDeclaredMethod("asInterface", IBinder.class);
            final Object originManagerService = asInterfaceMethod.invoke(null, originBinder);
            return Proxy.newProxyInstance(classLoader,
                    new Class[]{IBinder.class, IInterface.class, serviceManagerCls},
                    new InvocationHandler() {
                        @Override
                        public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
                            if (callback != null) {
                                callback.onServiceMethodInvoke(method, args);
                                Object result = callback.onServiceMethodIntercept(originManagerService, method, args);
                                if (result != null) {
                                    return result;
                                }
                            }
                            return method.invoke(originManagerService, args);
                        }
                    }
            );
        }
    }
}
