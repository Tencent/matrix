package com.tencent.matrix.iocanary.detect;

/*
 * Copyright (C) 2010 The Android Open Source Project
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


import com.tencent.matrix.util.MatrixLog;

/**
 * CloseGuard is a mechanism for flagging implicit finalizer cleanup of
 * resources that should have been cleaned up by explicit close
 * methods (aka "explicit termination methods" in Effective Java).
 * <p>
 * A simple example: <pre>   {@code
 *   class Foo {
 * <p>
 *       private final CloseGuard guard = CloseGuard.get();
 * <p>
 *       ...
 * <p>
 *       public Foo() {
 *           ...;
 *           guard.open("cleanup");
 *       }
 * <p>
 *       public void cleanup() {
 *          guard.close();
 *          ...;
 *       }
 * <p>
 *       protected void finalize() throws Throwable {
 *           try {
 *               // Note that guard could be null if the constructor threw.
 *               if (guard != null) {
 *                   guard.warnIfOpen();
 *               }
 *               cleanup();
 *           } finally {
 *               super.finalize();
 *           }
 *       }
 *   }
 * }</pre>
 * <p>
 * In usage where the resource to be explicitly cleaned up are
 * allocated after object construction, CloseGuard protection can
 * be deferred. For example: <pre>   {@code
 *   class Bar {
 * <p>
 *       private final CloseGuard guard = CloseGuard.get();
 * <p>
 *       ...
 * <p>
 *       public Bar() {
 *           ...;
 *       }
 * <p>
 *       public void connect() {
 *          ...;
 *          guard.open("cleanup");
 *       }
 * <p>
 *       public void cleanup() {
 *          guard.close();
 *          ...;
 *       }
 * <p>
 *       protected void finalize() throws Throwable {
 *           try {
 *               // Note that guard could be null if the constructor threw.
 *               if (guard != null) {
 *                   guard.warnIfOpen();
 *               }
 *               cleanup();
 *           } finally {
 *               super.finalize();
 *           }
 *       }
 *   }
 * }</pre>
 * <p>
 * When used in a constructor calls to {@code open} should occur at
 * the end of the constructor since an exception that would cause
 * abrupt termination of the constructor will mean that the user will
 * not have a reference to the object to cleanup explicitly. When used
 * in a method, the call to {@code open} should occur just after
 * resource acquisition.
 *
 * @hide
 */
@SuppressWarnings("PMD")
public final class MatrixCloseGuard {
    private static final String TAG = "Matrix.CloseGuard";

    /**
     * Instance used when CloseGuard is disabled to avoid allocation.
     */
    private static final MatrixCloseGuard NOOP = new MatrixCloseGuard();

    /**
     * Enabled by default so we can catch issues early in VM startup.
     * Note, however, that Android disables this early in its startup,
     * but enables it with DropBoxing for system apps on debug builds.
     */
    private static volatile boolean sENABLED = true;

    /**
     * Hook for customizing how CloseGuard issues are reported.
     */
    private static volatile Reporter sREPORTER = new DefaultReporter();

    /**
     * Returns a CloseGuard instance. If CloseGuard is enabled, {@code
     * #open(String)} can be used to set up the instance to warn on
     * failure to close. If CloseGuard is disabled, a non-null no-op
     * instance is returned.
     */
    public static MatrixCloseGuard get() {
        if (!sENABLED) {
            return NOOP;
        }
        return new MatrixCloseGuard();
    }

    /**
     * Used to enable or disable CloseGuard. Note that CloseGuard only
     * warns if it is enabled for both allocation and finalization.
     */
    public static void setEnabled(boolean enabled) {
        sENABLED = enabled;
    }

    /**
     * Used to replace default Reporter used to warn of CloseGuard
     * violations. Must be non-null.
     */
    public static void setReporter(Reporter reporter) {
        if (reporter == null) {
            throw new NullPointerException("reporter == null");
        }
        sREPORTER = reporter;
    }

    /**
     * Returns non-null CloseGuard.Reporter.
     */
    public static Reporter getReporter() {
        return sREPORTER;
    }

    private MatrixCloseGuard() {
    }

    /**
     * If CloseGuard is enabled, {@code open} initializes the instance
     * with a warning that the caller should have explicitly called the
     * {@code closer} method instead of relying on finalization.
     *
     * @param closer non-null name of explicit termination method
     * @throws NullPointerException if closer is null, regardless of
     *                              whether or not CloseGuard is enabled
     */
    public void open(String closer) {
        // always perform the check for valid API usage...
        if (closer == null) {
            throw new NullPointerException("closer == null");
        }
        // ...but avoid allocating an allocationSite if disabled
        if (this == NOOP || !sENABLED) {
            return;
        }
        String message = "Explicit termination method '" + closer + "' not called";
        allocationSite = new Throwable(message);
    }

    private Throwable allocationSite;

    /**
     * Marks this CloseGuard instance as closed to avoid warnings on
     * finalization.
     */
    public void close() {
        allocationSite = null;
    }

    /**
     * If CloseGuard is enabled, logs a warning if the caller did not
     * properly cleanup by calling an explicit close method
     * before finalization. If CloseGuard is disabled, no action is
     * performed.
     */
    public void warnIfOpen() {
        if (allocationSite == null || !sENABLED) {
            return;
        }

        String message =
            ("A resource was acquired at attached stack trace but never released. "
                + "See java.io.Closeable for information on avoiding resource leaks.");

        sREPORTER.report(message, allocationSite);
    }

    /**
     * Interface to allow customization of reporting behavior.
     */
    public interface Reporter {
        void report(String message, Throwable allocationSite);
    }

    /**
     * Default Reporter which reports CloseGuard violations to the log.
     */
    private static final class DefaultReporter implements Reporter {
        @Override
        public void report(String message, Throwable allocationSite) {
            MatrixLog.e(TAG, message, allocationSite);
        }
    }
}
