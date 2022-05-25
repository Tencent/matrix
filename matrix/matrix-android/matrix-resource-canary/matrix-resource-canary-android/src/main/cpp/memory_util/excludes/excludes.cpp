#include "excludes.h"

#include <sys/system_properties.h>

#ifndef __ANDROID_API_I_MR1__
#define __ANDROID_API_I_MR1__ 15
#endif

#define MANUFACTURER_SAMSUNG "samsung"
#define MANUFACTURER_LENOVO "LENOVO"
#define MANUFACTURER_VIVO "vivo"
#define MANUFACTURER_NVIDIA "NVIDIA"
#define MANUFACTURER_LG "LGE"
#define MANUFACTURER_MEIZU "Meizu"
#define MANUFACTURER_MOTOROLA "motorola"
#define MANUFACTURER_HUAWEI "HUAWEI"
#define MANUFACTURER_SHARP "SHARP"
#define MANUFACTURER_ONEPLUS "OnePlus"
#define MANUFACTURER_RAZER "Razer"

bool exclude_default_references(HprofAnalyzer &analyzer) {

    const int sdk_version = android_get_device_api_level();
    if (sdk_version < 0) return false;

    const std::string manufacturer = ({
        char value[PROP_VALUE_MAX];
        if (__system_property_get("ro.product.manufacturer", value) < -1) return false;
        std::string result(value);
        result;
    });

    /*
     * reason:
     *
     * Android Q added a new android.app.IRequestFinishCallback$Stub class. android.app.Activity
     * creates an implementation of that interface as an anonymous subclass. That anonymous subclass
     * has a reference to the activity. Another process is keeping the
     * android.app.IRequestFinishCallback$Stub reference alive long after Activity.onDestroyed() has
     * been called, causing the activity to leak.
     *
     * Fix: You can "fix" this leak by overriding Activity.onBackPressed() and calling
     * Activity.finishAfterTransition() instead of super if the activity is task root and the
     * fragment stack is empty.
     *
     * Tracked here: https://issuetracker.google.com/issues/139738913
     */
    if (sdk_version == __ANDROID_API_Q__) {
        analyzer.ExcludeInstanceFieldReference("android.app.Activity$1", "this$0");
    }

    /*
     * reason:
     *
     * Android AOSP sometimes keeps a reference to a destroyed activity as a nextIdle client record
     * in the android.app.ActivityThread.mActivities map.
     *
     * Not sure what's going on there, input welcome.
     */
    if (sdk_version >= __ANDROID_API_K__ && sdk_version <= __ANDROID_API_O_MR1__) {
        analyzer.ExcludeInstanceFieldReference(
                "android.app.ActivityThread$ActivityClientRecord", "nextIdle");
    }

    /*
     * reason:
     * 
     * Editor inserts a special span, which has a reference to the EditText. That span is a
     * NoCopySpan, which makes sure it gets dropped when creating a new SpannableStringBuilder from
     * a given CharSequence. TextView.onSaveInstanceState() does a copy of its mText before saving
     * it in the bundle. Prior to KitKat, that copy was done using the SpannableString constructor,
     * instead of SpannableStringBuilder. The SpannableString constructor does not drop NoCopySpan
     * spans. So we end up with a saved state that holds a reference to the textview and therefore
     * the entire view hierarchy & activity context.
     *
     * Fix: https://github.com/android/platform_frameworks_base/commit/af7dcdf35a37d7a7dbaad7d9869c1c91bce2272b
     *
     * To fix this, you could override TextView.onSaveInstanceState(), and then use reflection to
     * access TextView.SavedState.mText and clear the NoCopySpan spans.
     */
    if (sdk_version <= __ANDROID_API_K__) {
        analyzer.ExcludeInstanceFieldReference(
                "android.widget.Editor$EasyEditSpanController", "this$0");
        analyzer.ExcludeInstanceFieldReference(
                "android.widget.Editor$SpanController", "this$0");
    }

    /*
     * reason:
     * 
     * MediaSessionLegacyHelper is a static singleton that is lazily instantiated and keeps a
     * reference to the context it's given the first time MediaSessionLegacyHelper.getHelper() is
     * called. This leak was introduced in android-5.0.1_r1 and fixed in Android 5.1.0_r1 by calling
     * context.getApplicationContext().
     *
     * Fix: https://github.com/android/platform_frameworks_base/commit/9b5257c9c99c4cb541d8e8e78fb04f008b1a9091
     *
     * To fix this, you could call MediaSessionLegacyHelper.getHelper() early in
     * Application.onCreate() and pass it the application context.
     */
    if (sdk_version == __ANDROID_API_L__) {
        analyzer.ExcludeStaticFieldReference(
                "android.media.session.MediaSessionLegacyHelper", "sInstance");
    }

    /*
     * reason:
     * 
     * TextLine.sCached is a pool of 3 TextLine instances. TextLine.recycle() has had at least two
     * bugs that created memory leaks by not correctly clearing the recycled TextLine instances.
     * 
     * The first was fixed in android-5.1.0_r1:
     * https://github.com/android/platform_frameworks_base/commit/893d6fe48d37f71e683f722457bea646994a10
     * 
     * The second was fixed, not released yet:
     * https://github.com/android/platform_frameworks_base/commit/b3a9bc038d3a218b1dbdf7b5668e3d6c12be5e
     * 
     * To fix this, you could access TextLine.sCached and clear the pool every now and then (e.g. on
     * activity destroy).
     */
    if (sdk_version <= __ANDROID_API_L_MR1__) {
        analyzer.ExcludeStaticFieldReference("android.text.TextLine", "sCached");
    }

    /*
     * reason:
     * 
     * Prior to ART, a thread waiting on a blocking queue will leak the last dequeued object as a
     * stack local reference. So when a HandlerThread becomes idle, it keeps a local reference to
     * the last message it received. That message then gets recycled and can be used again. As long
     * as all messages are recycled after being used, this won't be a problem, because these
     * references are cleared when being recycled. However, dialogs create template Message
     * instances to be copied when a message needs to be sent. These Message templates holds
     * references to the dialog listeners, which most likely leads to holding a reference onto the
     * activity in some way. Dialogs never recycle their template Message, assuming these Message
     * instances will get GCed when the dialog is GCed.
     * 
     * The combination of these two things creates a high potential for memory leaks as soon as you
     * use dialogs. These memory leaks might be temporary, but some handler threads sleep for a long
     * time.
     * 
     * To fix this, you could post empty messages to the idle handler threads from time to time.
     * This won't be easy because you cannot access all handler threads, but a library that is
     * widely used should consider doing this for its own handler threads.
     */
    analyzer.ExcludeInstanceFieldReference("android.os.Message", "obj");
    if (sdk_version <= __ANDROID_API_L__) {
        analyzer.ExcludeInstanceFieldReference("android.os.Message", "next");
        analyzer.ExcludeInstanceFieldReference("android.os.Message", "target");
    }

    /*
     * reason:
     * 
     * When we detach a view that receives keyboard input, the InputMethodManager leaks a reference
     * to it until a new view asks for keyboard input.
     *
     * Tracked here: https://code.google.com/p/android/issues/detail?id=171190
     *
     * Hack: https://gist.github.com/pyricau/4df64341cc978a7de414
     */
    if (sdk_version >= __ANDROID_API_I_MR1__ && sdk_version <= __ANDROID_API_O_MR1__) {
        analyzer.ExcludeInstanceFieldReference(
                "android.view.inputmethod.InputMethodManager", "mNextServedView");
        analyzer.ExcludeInstanceFieldReference(
                "android.view.inputmethod.InputMethodManager", "mServedView");
        analyzer.ExcludeInstanceFieldReference(
                "android.view.inputmethod.InputMethodManager", "mServedInputConnection");
    }

    /*
     * reason:
     * 
     * The singleton InputMethodManager is holding a reference to mCurRootView long after the
     * activity has been destroyed.
     * 
     * Observed on ICS MR1: https://github.com/square/leakcanary/issues/1#issuecomment-100579429
     * 
     * Hack: https://gist.github.com/pyricau/4df64341cc978a7de414
     */
    if ((sdk_version >= __ANDROID_API_I_MR1__ && sdk_version <= __ANDROID_API_M__)
        ||
        (manufacturer == MANUFACTURER_HUAWEI &&
         sdk_version >= __ANDROID_API_M__ && sdk_version <= __ANDROID_API_P__)) {
        analyzer.ExcludeInstanceFieldReference(
                "android.view.inputmethod.InputMethodManager", "mCurRootView");
    }

    /*
     * reason:
     * 
     * Android Q Beta has a leak where InputMethodManager.mImeInsetsConsumer isn't set to null when
     * the activity is destroyed.
     */
    if (sdk_version == __ANDROID_API_P__) {
        analyzer.ExcludeInstanceFieldReference(
                "android.view.inputmethod.InputMethodManager", "mImeInsetsConsumer");
    }

    /*
     * reason:
     * 
     * In Android Q Beta InputMethodManager keeps its EditableInputConnection after the activity has
     * been destroyed.
     */
    if (sdk_version >= __ANDROID_API_P__ && sdk_version <= __ANDROID_API_Q__) {
        analyzer.ExcludeInstanceFieldReference(
                "android.view.inputmethod.InputMethodManager", "mCurrentInputConnection");
    }

    /*
     * reason:
     * 
     * LayoutTransition leaks parent ViewGroup through ViewTreeObserver.OnPreDrawListener when
     * triggered, this leaks stays until the window is destroyed. 
     * 
     * Tracked here: https://code.google.com/p/android/issues/detail?id=171830
     */
    if (sdk_version >= __ANDROID_API_I__ && sdk_version <= __ANDROID_API_L_MR1__) {
        analyzer.ExcludeInstanceFieldReference(
                "android.animation.LayoutTransition$1", "val$parent");
    }

    /*
     * reason:
     * 
     * SpellCheckerSessionListenerImpl.mHandler is leaking destroyed Activity when the
     * SpellCheckerSession is closed before the service is connected.
     *
     * Tracked here: https://code.google.com/p/android/issues/detail?id=172542
     */
    if (sdk_version >= __ANDROID_API_J__ && sdk_version <= __ANDROID_API_N__) {
        analyzer.ExcludeInstanceFieldReference(
                "android.view.textservice.SpellCheckerSession$1", "this$0");
    }

    /*
     * reason:
     * 
     * SpellChecker holds on to a detached view that points to a destroyed activity. mSpellRunnable
     * is being enqueued, and that callback should be removed when closeSession() is called. Maybe
     * closeSession() wasn't called, or maybe it was called after the view was detached.
     */
    if (sdk_version == __ANDROID_API_L_MR1__) {
        analyzer.ExcludeInstanceFieldReference("android.widget.SpellChecker$1", "this$0");
    }

    /*
     * reason:
     * 
     * ActivityChooserModel holds a static reference to the last set ActivityChooserModelPolicy 
     * which can be an activity context.
     * 
     * Tracked here: https://code.google.com/p/android/issues/detail?id=172659
     * 
     * Hack: https://gist.github.com/andaag/b05ab66ed0f06167d6e0
     */
    analyzer.ExcludeInstanceFieldReference(
            "android.widget.ActivityChooserModel",
            "mActivityChoserModelPolicy");
    if (sdk_version > __ANDROID_API_I__ && sdk_version <= __ANDROID_API_L_MR1__) {
        analyzer.ExcludeInstanceFieldReference(
                "android.support.v7.internal.widget.ActivityChooserModel",
                "mActivityChoserModelPolicy");
    }

    /*
     * reason:
     * 
     * MediaProjectionCallback is held by another process, and holds on to MediaProjection which has
     * an activity as its context.
     */
    if (sdk_version >= __ANDROID_API_L_MR1__ && sdk_version <= __ANDROID_API_P__) {
        analyzer.ExcludeInstanceFieldReference(
                "android.media.projection.MediaProjection$MediaProjectionCallback",
                "this$0");
    }

    /*
     * reason:
     * 
     * Prior to Android 5, SpeechRecognizer.InternalListener was a non static inner class and leaked
     * the SpeechRecognizer which leaked an activity context.
     *
     * Fixed in AOSP: https://github.com/android/platform_frameworks_base/commit/b37866db469e81aca534ff6186bdafd44352329b
     */
    if (sdk_version < __ANDROID_API_L__) {
        analyzer.ExcludeInstanceFieldReference(
                "android.speech.SpeechRecognizer$InternalListener", "this$0");
    }

    /*
     * reason:
     *
     * AccountManager$AmsTask$Response is a stub and is held in memory by native code, probably
     * because the reference to the response in the other process hasn't been cleared.
     * 
     * AccountManager$AmsTask is holding on to the activity reference to use for launching a new
     * sub-Activity.
     * 
     * Tracked here: https://code.google.com/p/android/issues/detail?id=173689
     * 
     * Fix: Pass a null activity reference to the AccountManager methods and then deal with the
     * returned future to to get the result and correctly start an activity when it's available.
     */
    if (sdk_version <= __ANDROID_API_O_MR1__) {
        analyzer.ExcludeInstanceFieldReference(
                "android.accounts.AccountManager$AmsTask$Response", "this$1");
    }

    /*
     * reason:
     * 
     * The static method MediaScannerConnection.scanFile() takes an activity context but the service
     * might not disconnect after the activity has been destroyed.
     *
     * Tracked here: https://code.google.com/p/android/issues/detail?id=173788
     *
     * Fix: Create an instance of MediaScannerConnection yourself and pass in the application
     * context. Call connect() and disconnect() manually.
     */
    if (sdk_version <= __ANDROID_API_L_MR1__) {
        analyzer.ExcludeInstanceFieldReference(
                "android.media.MediaScannerConnection", "mContext");
    }

    /*
     * reason:
     * 
     * UserManager has a static sInstance field that creates an instance and caches it the first
     * time UserManager.get() is called. This instance is created with the outer context (which is
     * an activity base context).
     *
     * Tracked here: https://code.google.com/p/android/issues/detail?id=173789
     *
     * Introduced by: https://github.com/android/platform_frameworks_base/commit/27db46850b708070452c0ce49daf5f79503fbde6
     *
     * Fix: trigger a call to UserManager.get() in Application.onCreate(), so that the UserManager
     * instance gets cached with a reference to the application context.
     */
    if (sdk_version >= __ANDROID_API_J__ && sdk_version <= __ANDROID_API_N_MR1__) {
        analyzer.ExcludeInstanceFieldReference(
                "android.os.UserManager", "mContext");
    }

    /*
     * reason:
     * 
     * android.appwidget.AppWidgetHost$Callbacks is a stub and is held in memory native code. The
     * reference to the `mContext` was not being cleared, which caused the Callbacks instance to
     * retain this reference
     * 
     * Fixed in AOSP: https://github.com/android/platform_frameworks_base/commit/7a96f3c917e0001ee739b65da37b2fadec7d7765
     */
    if (sdk_version < __ANDROID_API_L_MR1__) {
        analyzer.ExcludeInstanceFieldReference(
                "android.appwidget.AppWidgetHost$Callbacks", "this$0");
    }

    /*
     * reason:
     * 
     * Prior to Android M, VideoView required audio focus from AudioManager and never abandoned it,
     * which leaks the Activity context through the AudioManager. The root of the problem is that
     * AudioManager uses whichever context it receives, which in the case of the VideoView example
     * is an Activity, even though it only needs the application's context. The issue is fixed in
     * Android M, and the AudioManager now uses the application's context.
     * 
     * Tracked here: https://code.google.com/p/android/issues/detail?id=152173
     * 
     * Fix: https://gist.github.com/jankovd/891d96f476f7a9ce24e2
     */
    if (sdk_version <= __ANDROID_API_M__) {
        analyzer.ExcludeInstanceFieldReference(
                "android.media.AudioManager$1", "this$0");
    }

    /*
     * reason:
     * 
     * The EditText Blink of the Cursor is implemented using a callback and Messages, which trigger
     * the display of the Cursor. If an AlertDialog or DialogFragment that contains a blinking
     * cursor is detached, a message is posted with a delay after the dialog has been closed and as
     * a result leaks the Activity.
     * 
     * This can be fixed manually by calling TextView.setCursorVisible(false) in the dismiss()
     * method of the dialog.
     * 
     * Tracked here: https://code.google.com/p/android/issues/detail?id=188551
     * 
     * Fixed in AOSP: https://android.googlesource.com/platform/frameworks/base/+/5b734f2430e9f26c769d6af8ea5645e390fcf5af%5E%21/
     */
    if (sdk_version <= __ANDROID_API_L_MR1__) {
        analyzer.ExcludeInstanceFieldReference(
                "android.widget.Editor$Blink", "this$0");
    }

    /*
     * reason:
     * 
     * ConnectivityManager has a sInstance field that is set when the first ConnectivityManager
     * instance is created. ConnectivityManager has a mContext field. When calling
     * activity.getSystemService(Context.CONNECTIVITY_SERVICE), the first ConnectivityManager
     * instance is created with the activity context and stored in sInstance. That activity context
     * then leaks forever.
     *
     * Until this is fixed, app developers can prevent this leak by making sure the
     * ConnectivityManager is first created with an App Context. E.g. in some static init do:
     * context.getApplicationContext().getSystemService(Context.CONNECTIVITY_SERVICE)
     *
     * Tracked here: https://code.google.com/p/android/issues/detail?id=198852
     *
     * Introduced here: https://github.com/android/platform_frameworks_base/commit/e0bef71662d81caaaa0d7214fb0bef5d39996a69
     */
    if (sdk_version <= __ANDROID_API_M__) {
        analyzer.ExcludeInstanceFieldReference(
                "android.net.ConnectivityManager", "sInstance");
    }

    /*
     * reason:
     * 
     * AccessibilityNodeInfo has a static sPool of AccessibilityNodeInfo. When AccessibilityNodeInfo
     * instances are released back in the pool, AccessibilityNodeInfo.clear() does not clear the
     * mOriginalText field, which causes spans to leak which in turns causes TextView.ChangeWatcher
     * to leak and the whole view hierarchy.
     * 
     * Introduced here: https://android.googlesource.com/platform/frameworks/base/+/193520e3dff5248ddcf8435203bf99d2ba667219%5E%21/core/java/android/view/accessibility/AccessibilityNodeInfo.java
     */
    if (sdk_version >= __ANDROID_API_O__ && sdk_version <= __ANDROID_API_O_MR1__) {
        analyzer.ExcludeInstanceFieldReference(
                "android.view.accessibility.AccessibilityNodeInfo", "mOriginalText");
    }

    /*
     * reason:
     * 
     * AssistStructure (google assistant / autofill) holds on to text spannables on the screen.
     * TextView.ChangeWatcher and android.widget.Editor end up in spans and typically hold on to the
     * view hierarchy.
     */
    if (sdk_version >= __ANDROID_API_N__ && sdk_version <= __ANDROID_API_Q__) {
        analyzer.ExcludeInstanceFieldReference(
                "android.app.assist.AssistStructure$ViewNodeText", "mText");
    }

    /*
     * reason:
     * 
     * AccessibilityIterators holds on to text layouts which can hold on to spans
     * TextView.ChangeWatcher and android.widget.Editor end up in spans and typically hold on to the
     * view hierarchy.
     */
    if (sdk_version == __ANDROID_API_O_MR1__) {
        analyzer.ExcludeInstanceFieldReference(
                "android.widget.AccessibilityIterators$LineTextSegmentIterator", "mLayout");
    }

    /*
     * reason:
     * 
     * BiometricPrompt holds on to a FingerprintManager which holds on to a destroyed activity.
     */
    if (sdk_version == __ANDROID_API_P__) {
        analyzer.ExcludeInstanceFieldReference(
                "android.hardware.biometrics.BiometricPrompt", "mFingerprintManager");
    }

    /*
     * reason:
     * 
     * android.widget.Magnifier.InternalPopupWindow registers a frame callback
     * on android.view.ThreadedRenderer.SimpleRenderer which holds it as a native reference.
     * android.widget.Editor$InsertionHandleView registers an OnOperationCompleteCallback on
     * Magnifier.InternalPopupWindow. These references are held after the activity has been
     * destroyed.
     */
    if (sdk_version == __ANDROID_API_P__) {
        analyzer.ExcludeInstanceFieldReference(
                "android.widget.Magnifier$InternalPopupWindow", "mCallback");
    }

    /*
     * reason:
     * 
     * When BackdropFrameRenderer.releaseRenderer() is called, there's an unknown case where
     * mRenderer becomes null but mChoreographer doesn't and the thread doesn't stop and ends up
     * leaking mDecorView which itself holds on to a destroyed activity.
     */
    if (sdk_version >= __ANDROID_API_N__ && sdk_version <= __ANDROID_API_O__) {
        analyzer.ExcludeInstanceFieldReference(
                "com.android.internal.policy.BackdropFrameRenderer", "mDecorView");
    }

    /*
     * reason:
     * 
     * In Android P, ViewLocationHolder has an mRoot field that is not cleared in its clear()
     * method.
     * 
     * Introduced in https://github.com/aosp-mirror/platform_frameworks_base/commit/86b326012813f09d8f1de7d6d26c986a909d
     * 
     * Bug report: https://issuetracker.google.com/issues/112792715
     */
    if (sdk_version == __ANDROID_API_P__) {
        analyzer.ExcludeInstanceFieldReference(
                "android.view.ViewGroup$ViewLocationHolder", "mRoot");
    }

    /*
     * reason:
     * 
     * Android Q Beta added AccessibilityNodeIdManager which stores all views from their 
     * onAttachedToWindow() call, until detached. Unfortunately it's possible to trigger the view
     * framework to call detach before attach (by having a view removing itself from its parent in
     * onAttach, which then causes AccessibilityNodeIdManager to keep children view forever. Future
     * releases of Q will hold weak references.
     */
    if (sdk_version >= __ANDROID_API_P__ && sdk_version <= __ANDROID_API_Q__) {
        analyzer.ExcludeInstanceFieldReference(
                "android.view.accessibility.AccessibilityNodeIdManager", "mIdsToViews");
    }

    /*
     * reason:
     * 
     * TextToSpeech.shutdown() does not release its references to context objects. Furthermore,
     * TextToSpeech instances cannot be garbage collected due to other process keeping the
     * references, resulting the context objects leaked.
     * 
     * Developers might be able to mitigate the issue by passing application context to TextToSpeech
     * constructor.
     * 
     * Tracked at: https://github.com/square/leakcanary/issues/1210 and https://issuetracker.google.com/issues/129250419
     */
    if (sdk_version == __ANDROID_API_N__) {
        analyzer.ExcludeInstanceFieldReference("android.speech.tts.TextToSpeech", "mContext");
        analyzer.ExcludeInstanceFieldReference("android.speech.tts.TtsEngines", "mContext");
    }

    /*
     * reason:
     * 
     * ControlledInputConnectionWrapper is held by a global variable in native code. 
     */
    analyzer.ExcludeNativeGlobalReference(
            "android.view.inputmethod.InputMethodManager$ControlledInputConnectionWrapper");

    /*
     * reason:
     * 
     * Toast.TN is held by a global variable in native code due to an IPC call to show the toast.
     */
    analyzer.ExcludeNativeGlobalReference("android.widget.Toast$TN");

    /*
     * reason:
     * 
     * In Android 11 DP 2 ApplicationPackageManager.HasSystemFeatureQuery was an inner class.
     * 
     * Introduced in https://cs.android.com/android/_/android/platform/frameworks/base/+/89608118192580ffca026b5dacafa637a556d578
     * 
     * Fixed in https://cs.android.com/android/_/android/platform/frameworks/base/+/1f771846c51148b7cb6283e6dc82a216ffaa5353
     * 
     * Related blog: https://dev.to/pyricau/beware-packagemanager-leaks-223g
     */
    if (sdk_version == __ANDROID_API_Q__) {
        analyzer.ExcludeInstanceFieldReference(
                "android.app.ApplicationPackageManager$HasSystemFeatureQuery", "this$0");
    }

    /*
     * reason:
     * 
     * SpenGestureManager has a static mContext field that leaks a reference to the activity. Yes,
     * a STATIC mContext field.
     */
    if (manufacturer == MANUFACTURER_SAMSUNG && sdk_version == __ANDROID_API_K__) {
        analyzer.ExcludeStaticFieldReference(
                "com.samsung.android.smartclip.SpenGestureManager", "mContext");
    }

    /*
     * reason:
     * 
     * ClipboardUIManager is a static singleton that leaks an activity context.
     *
     * Fix: trigger a call to ClipboardUIManager.getInstance() in Application.onCreate(), so that
     * the ClipboardUIManager instance gets cached with a reference to the application context.
     *
     * Example: https://gist.github.com/cypressious/91c4fb1455470d803a602838dfcd5774
     */
    if (manufacturer == MANUFACTURER_SAMSUNG &&
        sdk_version >= __ANDROID_API_K__ && sdk_version <= __ANDROID_API_L__) {
        analyzer.ExcludeInstanceFieldReference(
                "android.sec.clipboard.ClipboardUIManager", "mContext");
    }

    /*
     * reason:
     *
     * SemClipboardManager is held in memory by an anonymous inner class implementation of
     * android.os.Binder, thereby leaking an activity context.
     */
    if (manufacturer == MANUFACTURER_SAMSUNG &&
        sdk_version >= __ANDROID_API_K__ && sdk_version <= __ANDROID_API_N__) {
        analyzer.ExcludeInstanceFieldReference(
                "com.samsung.android.content.clipboard.SemClipboardManager", "mContext");
    }

    /*
     * reason:
     * 
     * SemClipboardManager inner classes are held by native references due to IPC calls.
     */
    if (manufacturer == MANUFACTURER_SAMSUNG &&
        sdk_version >= __ANDROID_API_K__ && sdk_version <= __ANDROID_API_P__) {
        analyzer.ExcludeNativeGlobalReference(
                "com.samsung.android.content.clipboard.SemClipboardManager$1");
        analyzer.ExcludeNativeGlobalReference(
                "com.samsung.android.content.clipboard.SemClipboardManager$3");
    }

    /*
     * reason:
     * 
     * android.sec.clipboard.ClipboardExManager$IClipboardDataPasteEventImpl$1 is a native callback
     * that holds IClipboardDataPasteEventImpl which holds ClipboardExManager which has a destroyed
     * activity as mContext.
     */
    if (manufacturer == MANUFACTURER_SAMSUNG && sdk_version == __ANDROID_API_M__) {
        analyzer.ExcludeInstanceFieldReference(
                "android.sec.clipboard.ClipboardExManager", "mContext");
    }

    /*
     * reason:
     * 
     * android.sec.clipboard.ClipboardExManager$IClipboardDataPasteEventImpl$1 is a native callback
     * that holds IClipboardDataPasteEventImpl which holds ClipboardExManager which holds
     * PersonaManager which has a destroyed activity as mContext.
     */
    if (manufacturer == MANUFACTURER_SAMSUNG && sdk_version == __ANDROID_API_M__) {
        analyzer.ExcludeInstanceFieldReference(
                "android.sec.clipboard.ClipboardExManager", "mPersonaManager");
    }

    /*
     * reason:
     * 
     * TextView$IClipboardDataPasteEventImpl$1 is held by a native ref, and
     * IClipboardDataPasteEventImpl ends up leaking a detached textview.
     */
    if (manufacturer == MANUFACTURER_SAMSUNG && sdk_version == __ANDROID_API_L_MR1__) {
        analyzer.ExcludeInstanceFieldReference(
                "android.widget.TextView$IClipboardDataPasteEventImpl", "this$0");
    }

    /*
     * reason:
     *
     * SemEmergencyManager is a static singleton that leaks a DecorContext.
     */
    if (manufacturer == MANUFACTURER_SAMSUNG &&
        sdk_version >= __ANDROID_API_K__ && sdk_version <= __ANDROID_API_N__) {
        analyzer.ExcludeInstanceFieldReference(
                "com.samsung.android.emergencymode.SemEmergencyManager", "mContext");
    }

    /*
     * reason:
     * 
     * Unknown. From LeakCanary.
     */
    if (manufacturer == MANUFACTURER_SAMSUNG && sdk_version == __ANDROID_API_N__) {
        analyzer.ExcludeInstanceFieldReference(
                "com.samsung.android.knox.SemPersonaManager", "mContext");
    }

    /*
     * reason:
     * 
     * Unknown. From LeakCanary.
     */
    if (manufacturer == MANUFACTURER_SAMSUNG &&
        sdk_version >= __ANDROID_API_P__ && sdk_version <= __ANDROID_API_Q__) {
        analyzer.ExcludeInstanceFieldReference(
                "android.app.SemAppIconSolution", "mContext");
    }

    /*
     * reason:
     * 
     * AwResource#setResources() is called with resources that hold a reference to the activity
     * context (instead of the application context) and doesn't clear it.
     * 
     * Not sure what's going on there, input welcome.
     */
    if (manufacturer == MANUFACTURER_SAMSUNG && sdk_version == __ANDROID_API_K__) {
        analyzer.ExcludeStaticFieldReference(
                "com.android.org.chromium.android_webview.AwResource", "sResources");
    }

    /*
     * reason:
     * 
     * mLastHoveredView is a static field in TextView that leaks the last hovered view.
     */
    if (manufacturer == MANUFACTURER_SAMSUNG &&
        sdk_version >= __ANDROID_API_K__ && sdk_version <= __ANDROID_API_Q__) {
        analyzer.ExcludeStaticFieldReference("android.widget.TextView", "mLastHoveredView");
    }

    /*
     * reason:
     * 
     * android.app.LoadedApk.mResources has a reference to
     * android.content.res.Resources.mPersonaManager which has a reference to
     * android.os.PersonaManager.mContext which is an activity.
     */
    if (manufacturer == MANUFACTURER_SAMSUNG && sdk_version == __ANDROID_API_K__) {
        analyzer.ExcludeInstanceFieldReference("android.os.PersonaManager", "mContext");
    }

    /*
     * reason:
     * 
     * In AOSP the Resources class does not have a context. Here we have ZygoteInit.mResources
     * (static field) holding on to a Resources instance that has a context that is the activity.
     * 
     * Observed here: https://github.com/square/leakcanary/issues/1#issue-74450184
     */
    if (manufacturer == MANUFACTURER_SAMSUNG && sdk_version == __ANDROID_API_K__) {
        analyzer.ExcludeInstanceFieldReference("android.content.res.Resources", "mContext");
    }

    /*
     * reason:
     * 
     * In AOSP the ViewConfiguration class does not have a context. Here we have
     * ViewConfiguration.sConfigurations (static field) holding on to a ViewConfiguration instance
     * that has a context that is the activity.
     * 
     * Observed here: https://github.com/square/leakcanary/issues/1#issuecomment-100324683
     */
    if (manufacturer == MANUFACTURER_SAMSUNG && sdk_version == __ANDROID_API_K__) {
        analyzer.ExcludeInstanceFieldReference("android.view.ViewConfiguration", "mContext");
    }

    /*
     * reason:
     * 
     * Samsung added a static mContext_static field to AudioManager, holds a reference to the
     * activity.
     *
     * Observed here: https://github.com/square/leakcanary/issues/32
     */
    if (manufacturer == MANUFACTURER_SAMSUNG && sdk_version == __ANDROID_API_K__) {
        analyzer.ExcludeStaticFieldReference("android.media.AudioManager", "mContext_static");
    }

    /*
     * reason:
     * 
     * Samsung added a static mContext field to ActivityManager, holds a reference to the activity.
     *
     * Observed here: https://github.com/square/leakcanary/issues/177
     *
     * Fix in comment: https://github.com/square/leakcanary/issues/177#issuecomment-222724283
     */
    if (manufacturer == MANUFACTURER_SAMSUNG &&
        sdk_version >= __ANDROID_API_L_MR1__ && sdk_version <= __ANDROID_API_M__) {
        analyzer.ExcludeStaticFieldReference("android.app.ActivityManager", "mContext");
    }

    /*
     * reason:
     * 
     * Samsung added a static mTargetView field to TextView which holds on to detached views.
     */
    if (manufacturer == MANUFACTURER_SAMSUNG && sdk_version == __ANDROID_API_O_MR1__) {
        analyzer.ExcludeStaticFieldReference("android.widget.TextView", "mTargetView");
    }

    /*
     * reason:
     * 
     * DecorView isn't leaking but its mDecorViewSupport field holds a MultiWindowDecorSupport which
     * has a mWindow field which holds a leaking PhoneWindow. DecorView.mDecorViewSupport doesn't
     * exist in AOSP.
     * 
     * Filed here: https://github.com/square/leakcanary/issues/1819
     */
    if (manufacturer == MANUFACTURER_SAMSUNG &&
        sdk_version >= __ANDROID_API_O__ && sdk_version <= __ANDROID_API_Q__) {
        analyzer.ExcludeInstanceFieldReference(
                "com.android.internal.policy.MultiWindowDecorSupport", "mWindow");
    }

    /*
     * reason:
     * 
     * Lenovo specific leak. SystemSensorManager stores a reference to context in a static field in
     * its constructor. Found on LENOVO 4.4.2.
     * 
     * Fix: use application context to get SensorManager.
     */
    if ((manufacturer == MANUFACTURER_LENOVO && sdk_version == __ANDROID_API_K__) ||
        (manufacturer == MANUFACTURER_VIVO && sdk_version == __ANDROID_API_L_MR1__)) {
        analyzer.ExcludeStaticFieldReference(
                "android.hardware.SystemSensorManager", "mAppContextImpl");
    }

    /*
     * reason:
     * 
     * Not sure exactly what ControllerMapper is about, but there is an anonymous HeapDumpHandler in
     * ControllerMapper.MapperClient.ServiceClient, which leaks ControllerMapper.MapperClient which
     * leaks the activity context.
     */
    if (manufacturer == MANUFACTURER_NVIDIA && sdk_version == __ANDROID_API_K__) {
        analyzer.ExcludeInstanceFieldReference(
                "com.nvidia.ControllerMapper.MapperClient$ServiceClient", "this$0");
    }

    /*
     * reason:
     *
     * A static helper for EditText bubble popups leaks a reference to the latest focused view.
     */
    if (manufacturer == MANUFACTURER_LG &&
        sdk_version >= __ANDROID_API_K__ && sdk_version <= __ANDROID_API_L_MR1__) {
        analyzer.ExcludeStaticFieldReference(
                "android.widget.BubblePopupHelper", "sHelper");
    }

    /*
     * reason:
     * 
     * LGContext is a static singleton that leaks an activity context.
     */
    if (manufacturer == MANUFACTURER_LG && sdk_version == __ANDROID_API_L__) {
        analyzer.ExcludeInstanceFieldReference("com.lge.systemservice.core.LGContext", "mContext");
    }

    /*
     * reason:
     * 
     * SmartCoverManager$CallbackRegister is a callback held by a native ref, and SmartCoverManager
     * ends up leaking an activity context.
     */
    if (manufacturer == MANUFACTURER_LG && sdk_version == __ANDROID_API_O_MR1__) {
        analyzer.ExcludeInstanceFieldReference(
                "com.lge.systemservice.core.SmartCoverManager", "mContext");
    }

    /*
     * reason:
     *
     * Instrumentation would leak com.android.internal.app.RecommendActivity (in framework.jar) in
     * Meizu FlymeOS 4.5 and above, which is based on Android 5.0 and above
     */
    if (manufacturer == MANUFACTURER_MEIZU &&
        sdk_version >= __ANDROID_API_L__ && sdk_version <= __ANDROID_API_L_MR1__) {
        analyzer.ExcludeStaticFieldReference(
                "android.app.Instrumentation", "mRecommendActivity");
    }

    /*
     * reason:
     * 
     * DevicePolicyManager keeps a reference to the context it has been created with instead of
     * extracting the application context. In this Motorola build, DevicePolicyManager has an inner
     * SettingsObserver class that is a content observer, which is held into memory by a binder
     * transport object.
     */
    if (manufacturer == MANUFACTURER_MOTOROLA &&
        sdk_version >= __ANDROID_API_K__ && sdk_version <= __ANDROID_API_L_MR1__) {
        analyzer.ExcludeInstanceFieldReference(
                "android.app.admin.DevicePolicyManager$SettingsObserver",
                "this$0");
    }

    /*
     * reason:
     * 
     * GestureBoostManager is a static singleton that leaks an activity context.
     *
     * Fix: https://github.com/square/leakcanary/issues/696#issuecomment-296420756
     */
    if (manufacturer == MANUFACTURER_HUAWEI &&
        sdk_version >= __ANDROID_API_N__ && sdk_version <= __ANDROID_API_N_MR1__) {
        analyzer.ExcludeStaticFieldReference(
                "android.gestureboost.GestureBoostManager", "mContext");
    }

    /*
     * reason:
     * 
     * ExtendedStatusBarManager is held in a static sInstance field and has a mContext field which
     * references a decor context which references a destroyed activity.
     */
    if (manufacturer == MANUFACTURER_SHARP && sdk_version == __ANDROID_API_Q__) {
        analyzer.ExcludeStaticFieldReference("android.app.ExtendedStatusBarManager", "sInstance");
    }

    /*
     * reason:
     * 
     * OemSceneCallBlocker has a sContext static field which holds on to an activity instance.
     */
    if (manufacturer == MANUFACTURER_ONEPLUS && sdk_version == __ANDROID_API_P__) {
        analyzer.ExcludeStaticFieldReference("com.oneplus.util.OemSceneCallBlocker", "sContext");
    }

    /*
     * reason:
     * 
     * In AOSP, TextKeyListener instances are held in a TextKeyListener.sInstances static array.
     * The Razer implementation added a mContext field, creating activity leaks.
     */
    if (manufacturer == MANUFACTURER_RAZER && sdk_version == __ANDROID_API_P__) {
        analyzer.ExcludeInstanceFieldReference("android.text.method.TextKeyListener", "mContext");
    }

    /*
       General Exclude References
                                  */

    /*
     * Non-strong references.
     */
    analyzer.ExcludeInstanceFieldReference("java.lang.ref.WeakReference", "referent");
// we should treat soft reference as strong reference, because
// "all soft references to softly-reachable objects are guaranteed to have been cleared before the virtual machine throws an OutOfMemoryError."
// see https://developer.android.com/reference/java/lang/ref/SoftReference
//    analyzer.ExcludeInstanceFieldReference("java.lang.ref.SoftReference", "referent");
    analyzer.ExcludeInstanceFieldReference("java.lang.ref.PhantomReference", "referent");
    analyzer.ExcludeInstanceFieldReference("java.lang.ref.Finalizer", "prev");
    analyzer.ExcludeInstanceFieldReference("java.lang.ref.Finalizer", "element");
    analyzer.ExcludeInstanceFieldReference("java.lang.ref.Finalizer", "next");
    analyzer.ExcludeInstanceFieldReference("java.lang.ref.FinalizerReference", "prev");
    analyzer.ExcludeInstanceFieldReference("java.lang.ref.FinalizerReference", "element");
    analyzer.ExcludeInstanceFieldReference("java.lang.ref.FinalizerReference", "next");
    analyzer.ExcludeInstanceFieldReference("sun.misc.Cleaner", "prev");
    analyzer.ExcludeInstanceFieldReference("sun.misc.Cleaner", "next");

    /*
     * Finalizer watch dog daemon.
     */
    analyzer.ExcludeThreadReference("FinalizerWatchdogDaemon");

    /*
     * Main thread.
     */
    analyzer.ExcludeThreadReference("main");

    /*
     * Event receiver message queue.
     */
    analyzer.ExcludeInstanceFieldReference(
            "android.view.Choreographer$FrameDisplayEventReceiver",
            "mMessageQueue");

    return true;
}