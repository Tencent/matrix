package com.tencent.matrix;

import android.app.Activity;
import android.app.Application;

import com.tencent.matrix.listeners.IAppForeground;
import com.tencent.matrix.lifecycle.owners.MatrixProcessLifecycleOwner;
import com.tencent.matrix.util.MatrixUtil;

/**
 * use {@link MatrixProcessLifecycleOwner} instead
 */
@Deprecated
public enum AppActiveMatrixDelegate {

    INSTANCE;

    private static final String TAG = "Matrix.AppActiveDelegate";

    public void init(Application application) {
    }

    public String getCurrentFragmentName() {
        return MatrixProcessLifecycleOwner.INSTANCE.getCurrentFragmentName();
    }

    /**
     * must set after {@link Activity#onStart()}
     *
     * @param fragmentName
     */
    public void setCurrentFragmentName(String fragmentName) {
        MatrixProcessLifecycleOwner.INSTANCE.setCurrentFragmentName(fragmentName);
    }

    public String getVisibleScene() {
        return MatrixProcessLifecycleOwner.INSTANCE.getVisibleScene();
    }

    @Deprecated
    public boolean isAppForeground() {
        return MatrixProcessLifecycleOwner.INSTANCE.isProcessForeground();
    }

    /**
     * use {@link MatrixProcessLifecycleOwner} instead:
     * @param listener
     */
    @Deprecated
    public void addListener(IAppForeground listener) {
        MatrixProcessLifecycleOwner.INSTANCE.addListener(listener);
    }

    /**
     * use {@link MatrixProcessLifecycleOwner} instead:
     * @param listener
     */
    @Deprecated
    public void removeListener(IAppForeground listener) {
        MatrixProcessLifecycleOwner.INSTANCE.removeListener(listener);
    }

    @Deprecated
    public static String getTopActivityName() {
        return MatrixUtil.getTopActivityName();
    }
}
