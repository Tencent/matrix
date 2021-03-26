/*
 * Copyright (C) 2015 The Android Open Source Project
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
package com.tencent.matrix.resource.analyzer.model;

import com.tencent.matrix.resource.analyzer.utils.BitmapDecoder;

import java.awt.Dimension;

public class HprofBitmapProvider implements BitmapDecoder.BitmapDataProvider {
    private final byte[] mBuffer;
    private final int mWidth;
    private final int mHeight;

    public HprofBitmapProvider(byte[] buffer, int width, int height) {
        mBuffer = buffer;
        mWidth = width;
        mHeight = height;
    }

    @Override
    public String getBitmapConfigName() {
        int area = mWidth * mHeight;
        int pixelSize = mBuffer.length / area;

        if (area > mBuffer.length) {
            return null;
        }

        switch (pixelSize) {
            case 4:
                return "\"ARGB_8888\"";
            case 2:
                return "\"RGB_565\"";
            default:
                return "\"ALPHA_8\"";
        }
    }

    @Override
    public Dimension getDimension() {
        return mWidth < 0 || mHeight < 0 ? null : new Dimension(mWidth, mHeight);
    }

    @Override
    public boolean downsizeBitmap(Dimension newSize) {
        return true;
    }

    @Override
    public byte[] getPixelBytes(Dimension size) {
        return mBuffer;
    }
}
