package com.tencent.matrix;

import android.app.Activity;
import android.app.Application;

import com.tencent.matrix.listeners.IAppForeground;
import com.tencent.matrix.lifecycle.owners.MultiProcessLifecycleOwner;
import com.tencent.matrix.util.MatrixUtil;

/**
 * use {@link MultiProcessLifecycleOwner} instead
 */
@Deprecated
public enum AppActiveMatrixDelegate {

    INSTANCE;

    private static final String TAG = "Matrix.AppActiveDelegate";

    public void init(Application application) {
    }

    public String getCurrentFragmentName() {
        return MultiProcessLifecycleOwner.INSTANCE.getCurrentFragmentName();
    }

    /**
     * must set after {@link Activity#onStart()}
     *
     * @param fragmentName
     */
    public void setCurrentFragmentName(String fragmentName) {
        MultiProcessLifecycleOwner.INSTANCE.setCurrentFragmentName(fragmentName);
    }

    public String getVisibleScene() {
        return MultiProcessLifecycleOwner.INSTANCE.getVisibleScene();
    }

    @Deprecated
    public boolean isAppForeground() {
        return MultiProcessLifecycleOwner.INSTANCE.isProcessForeground();
    }

    /**
     * use {@link MultiProcessLifecycleOwner} instead:
     * @param listener
     */
    @Deprecated
    public void addListener(IAppForeground listener) {
        MultiProcessLifecycleOwner.INSTANCE.addListener(listener);
    }

    /**
     * use {@link MultiProcessLifecycleOwner} instead:
     * @param listener
     */
    @Deprecated
    public void removeListener(IAppForeground listener) {
        MultiProcessLifecycleOwner.INSTANCE.removeListener(listener);
    }

    @Deprecated
    public static String getTopActivityName() {
        return MatrixUtil.getTopActivityName();
    }
}
