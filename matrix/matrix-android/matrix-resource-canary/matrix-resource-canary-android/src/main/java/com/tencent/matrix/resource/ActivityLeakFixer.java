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

package com.tencent.matrix.resource;

import android.app.Activity;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.text.TextWatcher;
import android.util.Pair;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ListAdapter;
import android.widget.ProgressBar;
import android.widget.TextView;

import com.tencent.matrix.util.MatrixLog;

import java.lang.reflect.Field;
import java.util.ArrayList;

/**
 * Created by tangyinsheng on 2017/6/20.
 */

public final class ActivityLeakFixer {
    private static final String TAG = "Matrix.ActivityLeakFixer";

    private static Pair<ViewGroup, ArrayList<View>> sGroupAndOutChildren;

    /**
     * In Android P, ViewLocationHolder has an mRoot field that is not cleared in its clear() method.
     * Introduced in https://github.com/aosp-mirror/platform_frameworks_base/commit
     * /86b326012813f09d8f1de7d6d26c986a909d
     *
     * This leaks triggers very often when accessibility is on. To fix this leak we need to clear
     * the ViewGroup.ViewLocationHolder.sPool pool. Unfortunately Android P prevents accessing that
     * field through reflection. So instead, we call [ViewGroup#addChildrenForAccessibility] with
     * a view group that has 32 children (32 being the pool size), which as result fills in the pool
     * with 32 dumb views that reference a dummy context instead of an activity context.
     *
     * This fix empties the pool on every activity destroy and every AndroidX fragment view destroy.
     * You can support other cases where views get detached by calling directly
     * [ViewLocationHolderLeakFix.clearStaticPool].
     */
    public static void fixViewLocationHolderLeakApi28(Context destContext) {
        if (Build.VERSION.SDK_INT != Build.VERSION_CODES.P) {
            return;
        }

        try {
            Context application = destContext.getApplicationContext();
            if (sGroupAndOutChildren == null) {
                ViewGroup sViewGroup = new FrameLayout(application);
                // ViewLocationHolder.MAX_POOL_SIZE = 32
                for (int i = 0; i < 32; i++) {
                    View childView = new View(application);
                    sViewGroup.addView(childView);
                }
                sGroupAndOutChildren = new Pair<>(sViewGroup, new ArrayList<View>());
            }

            sGroupAndOutChildren.first.addChildrenForAccessibility(sGroupAndOutChildren.second);
        } catch (Throwable e) {
            MatrixLog.printErrStackTrace(TAG, e, "fixViewLocationHolderLeakApi28 err");
        }
    }



    public static void fixInputMethodManagerLeak(Context destContext) {
        final long startTick = System.currentTimeMillis();

        do {
            if (destContext == null) {
                break;
            }

            final InputMethodManager imm = (InputMethodManager) destContext.getSystemService(Context.INPUT_METHOD_SERVICE);
            if (imm == null) {
                break;
            }

            final String[] viewFieldNames = new String[]{"mCurRootView", "mServedView", "mNextServedView"};
            for (String viewFieldName : viewFieldNames) {
                try {
                    final Field paramField = imm.getClass().getDeclaredField(viewFieldName);
                    if (!paramField.isAccessible()) {
                        paramField.setAccessible(true);
                    }
                    final Object obj = paramField.get(imm);
                    if (obj instanceof View) {
                        final View view = (View) obj;
                        // Context held by InputMethodManager is what we want to split from reference chain.
                        if (view.getContext() == destContext) {
                            // Break the gc path.
                            paramField.set(imm, null);
                        } else {
                            // The first time we meet the context we don't want to split indicates that the rest context
                            // is not need to be concerned, so we break the loop in this case.
                            MatrixLog.i(TAG, "fixInputMethodManagerLeak break, context is not suitable, get_context=" + view.getContext() + " dest_context=" + destContext);
                            break;
                        }
                    }
                } catch (Throwable thr) {
                    MatrixLog.e(TAG, "failed to fix InputMethodManagerLeak, %s", thr.toString());
                }
            }
        } while (false);

        MatrixLog.i(TAG, "fixInputMethodManagerLeak done, cost: %s ms.", System.currentTimeMillis() - startTick);
    }

    public static boolean sSupportSplit = false;

    public static void unbindDrawables(Activity ui) {
        final long startTick = System.currentTimeMillis();
        if (ui != null && ui.getWindow() != null && ui.getWindow().peekDecorView() != null) {
            View viewRoot = ui.getWindow().peekDecorView().getRootView();
            try {
                unbindDrawablesAndRecycle(viewRoot);
                if (Build.VERSION.SDK_INT >= 31 && sSupportSplit) {
                    viewRoot = ui.getWindow().getDecorView().findViewById(android.R.id.content);
                }
                if (viewRoot instanceof ViewGroup) {
                    ((ViewGroup) viewRoot).removeAllViews();
                }
            } catch (Throwable thr) {
                MatrixLog.w(TAG, "caught unexpected exception when unbind drawables.", thr);
            }
        } else {
            MatrixLog.i(TAG, "unbindDrawables, ui or ui's window is null, skip rest works.");
        }
        MatrixLog.i(TAG, "unbindDrawables done, cost: %s ms.", System.currentTimeMillis() - startTick);
    }

    private static void unbindDrawablesAndRecycle(View view) {
        if (view == null) {
            return;
        }
        if (view.getContext() == null) {
            return;
        }

        recycleView(view);

        if (view instanceof ImageView) {
            recycleImageView((ImageView) view);
        }

        if (view instanceof TextView) {
            recycleTextView((TextView) view);
        }

        if (view instanceof ProgressBar) {
            recycleProgressBar((ProgressBar) view);
        }

        if (view instanceof android.widget.ListView) {
            recycleListView((android.widget.ListView) view);
        }

        if (view instanceof FrameLayout) {
            recycleFrameLayout((FrameLayout) view);
        }

        if (view instanceof LinearLayout) {
            recycleLinearLayout((LinearLayout) view);
        }

        if (view instanceof ViewGroup) {
            recycleViewGroup((ViewGroup) view);
        }

//        cleanContextOfView(view);
    }

    /**
     * asynchronous call may be crash
     * @param view
     */
    private static void cleanContextOfView(View view) {
        try {
            final Field mContextField = View.class.getDeclaredField("mContext");
            mContextField.setAccessible(true);
            mContextField.set(view, null);
        } catch (Throwable ignored) {
            // Ignored.
        }
    }

    private static void recycleView(View view) {
        if (view == null) {
            return;
        }

        boolean isClickable = view.isClickable();
        boolean isLongClickable = view.isLongClickable();

        try {
            view.setOnClickListener(null);
        } catch (Throwable ignored) {
            // Ignored.
        }

        try {
            view.setOnCreateContextMenuListener(null);
        } catch (Throwable ignored) {
            // Ignored.
        }

        try {
            view.setOnFocusChangeListener(null);
        } catch (Throwable ignored) {
            // Ignored.
        }

        try {
            view.setOnKeyListener(null);
        } catch (Throwable ignored) {
            // Ignored.
        }

        try {
            view.setOnLongClickListener(null);
        } catch (Throwable ignored) {
            // Ignored.
        }

        try {
            view.setOnClickListener(null);
        } catch (Throwable ignored) {
            // Ignored.
        }

        try {
            view.setOnTouchListener(null);
        } catch (Throwable ignored) {
            // Ignored.
        }

        if (view.getBackground() != null) {
            view.addOnAttachStateChangeListener(new View.OnAttachStateChangeListener() {
                @Override
                public void onViewAttachedToWindow(View v) {

                }

                @Override
                public void onViewDetachedFromWindow(View v) {
                    try {
                        v.getBackground().setCallback(null);
                        v.setBackgroundDrawable(null);
                    } catch (Throwable ignored) {
                        // Ignored.
                    }
                    try {
                        v.destroyDrawingCache();
                    } catch (Throwable thr) {
                        // Ignored.
                    }
                    v.removeOnAttachStateChangeListener(this);
                }
            });
        }

        view.setClickable(isClickable);
        view.setLongClickable(isLongClickable);
    }

    private static void recycleImageView(ImageView iv) {
        if (iv == null) {
            return;
        }

        final Drawable d = iv.getDrawable();
        if (d != null) {
            d.setCallback(null);
        }

        iv.setImageDrawable(null);
    }

    private static void recycleTextView(TextView tv) {
        final Drawable[] ds = tv.getCompoundDrawables();
        for (Drawable d : ds) {
            if (d != null) {
                d.setCallback(null);
            }
        }
        tv.setCompoundDrawables(null, null, null, null);
        tv.setOnEditorActionListener(null);
        tv.setKeyListener(null);
        tv.setMovementMethod(null);
        if (tv instanceof EditText) {
            fixTextWatcherLeak(tv);
        }
//        cleanContextOfView(tv);
    }

    @SuppressWarnings("unchecked")
    private static void fixTextWatcherLeak(TextView tv) {
        if (tv == null) {
            return;
        }
        try {
            tv.setHint("");
            final Field mListenersField = TextView.class.getDeclaredField("mListeners");
            mListenersField.setAccessible(true);
            final Object mListeners = mListenersField.get(tv);
            if (mListeners instanceof ArrayList) {
                final ArrayList<TextWatcher> tws = (ArrayList<TextWatcher>) mListeners;
                tws.clear();
            }
        } catch (Throwable ignored) {
            // Ignored.
        }
    }

    private static void recycleProgressBar(ProgressBar pb) {
        final Drawable pd = pb.getProgressDrawable();
        if (pd != null) {
            pb.setProgressDrawable(null);
            pd.setCallback(null);
        }
        final Drawable id = pb.getIndeterminateDrawable();
        if (id != null) {
            pb.setIndeterminateDrawable(null);
            id.setCallback(null);
        }
    }

    private static void recycleListView(android.widget.ListView listView) {
        Drawable selector = listView.getSelector();
        if (selector != null) {
            selector.setCallback(null);
        }

        try {
            final ListAdapter la = listView.getAdapter();
            if (la != null) {
                listView.setAdapter(null);
            }
        } catch (Throwable ignored) {
            // Ignored.
        }

        try {
            listView.setOnScrollListener(null);
        } catch (Throwable ignored) {
            // Ignored.
        }

        try {
            listView.setOnItemClickListener(null);
        } catch (Throwable ignored) {
            // Ignored.
        }

        try {
            listView.setOnItemLongClickListener(null);
        } catch (Throwable ignored) {
            // Ignored.
        }

        try {
            listView.setOnItemSelectedListener(null);
        } catch (Throwable ignored) {
            // Ignored.
        }
    }

    private static void recycleFrameLayout(FrameLayout fl) {
        if (fl != null) {
            final Drawable fg = fl.getForeground();
            if (fg != null) {
                fg.setCallback(null);
                fl.setForeground(null);
            }
        }
    }

    private static void recycleLinearLayout(LinearLayout ll) {
        if (ll == null) {
            return;
        }

        if (Build.VERSION_CODES.HONEYCOMB <= Build.VERSION.SDK_INT) {
            // Above API 11, process Divider.
            Drawable dd = null;
            if (Build.VERSION_CODES.JELLY_BEAN <= Build.VERSION.SDK_INT) {
                dd = ll.getDividerDrawable();
            } else {
                // Below API 16, so fetch Divider by reflection.
                try {
                    final Field mDividerField = ll.getClass().getDeclaredField("mDivider");
                    mDividerField.setAccessible(true);
                    dd = (Drawable) mDividerField.get(ll);
                } catch (Throwable ignored) {
                    // Ignored.
                }
            }
            if (dd != null) {
                dd.setCallback(null);
                ll.setDividerDrawable(null);
            }
        }
    }

    private static void recycleViewGroup(ViewGroup vg) {
        final int childCount = vg.getChildCount();
        for (int i = 0; i < childCount; ++i) {
            unbindDrawablesAndRecycle(vg.getChildAt(i));
        }
    }
}
