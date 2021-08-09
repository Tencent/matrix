package sample.tencent.matrix.hooks;

/**
 * Created by Yves on 2019-08-08
 */
public class JNIObj {

    private static final String TAG = "Matrix.JNIObj";

    static {
        init();
    }

    private static void init() {
        try {
            System.loadLibrary("native-lib");
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public native void reallocTest();

    public native void doMmap();

    public native static void mallocTest();
}
