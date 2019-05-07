package com.tencent.matrix.trace.view;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import com.tencent.matrix.trace.R;

public class FloatFrameView extends LinearLayout {

    public TextView fpsView;
    public TextView fpsView5;
    public TextView fpsView10;

    public FloatFrameView(Context context) {
        super(context);
        initView(context);
    }

    public FloatFrameView(Context context, AttributeSet attrs) {
        super(context, attrs);
        initView(context);
    }


    private void initView(Context context) {
        setLayoutParams(new ViewGroup.LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));
        LayoutInflater.from(context).inflate(R.layout.float_frame_view, this);
        fpsView = findViewById(R.id.fps_view);
        fpsView5 = findViewById(R.id.fps_view_5);
        fpsView10 = findViewById(R.id.fps_view_10);
    }

}