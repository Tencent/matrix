package com.tencent.matrix;

import android.app.Activity;
import android.app.Application;

import com.tencent.matrix.listeners.IAppForeground;
import com.tencent.matrix.lifecycle.owners.ProcessUILifecycleOwner;
import com.tencent.matrix.util.MatrixUtil;

/**
 * use {@link ProcessUILifecycleOwner} instead
 */
@Deprecated
public enum AppActiveMatrixDelegate {

    INSTANCE;

    private static final String TAG = "Matrix.AppActiveDelegate";

    public void init(Application application) {
    }

    public String getCurrentFragmentName() {
        return ProcessUILifecycleOwner.INSTANCE.getCurrentFragmentName();
    }

    /**
     * must set after {@link Activity#onStart()}
     *
     * @param fragmentName
     */
    public void setCurrentFragmentName(String fragmentName) {
        ProcessUILifecycleOwner.INSTANCE.setCurrentFragmentName(fragmentName);
    }

    public String getVisibleScene() {
        return ProcessUILifecycleOwner.INSTANCE.getVisibleScene();
    }

    @Deprecated
    public boolean isAppForeground() {
        return ProcessUILifecycleOwner.INSTANCE.isProcessForeground();
    }

    /**
     * use {@link ProcessUILifecycleOwner} instead:
     * @param listener
     */
    @Deprecated
    public void addListener(IAppForeground listener) {
        ProcessUILifecycleOwner.INSTANCE.addListener(listener);
    }

    /**
     * use {@link ProcessUILifecycleOwner} instead:
     * @param listener
     */
    @Deprecated
    public void removeListener(IAppForeground listener) {
        ProcessUILifecycleOwner.INSTANCE.removeListener(listener);
    }

    @Deprecated
    public static String getTopActivityName() {
        return MatrixUtil.getTopActivityName();
    }
}
