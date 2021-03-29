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
package com.tencent.matrix.resource.analyzer.utils;

import java.awt.Dimension;
import java.awt.image.BufferedImage;
import java.util.HashMap;
import java.util.Map;

/**
 * Created by tangyinsheng on 2017/7/6.
 * <p>
 * This class is ported from Android Studio tools.
 */
public class BitmapDecoder {
    public static final String BITMAP_FQCN = "android.graphics.Bitmap";

    public static final String BITMAP_DRAWABLE_FQCN = "android.graphics.drawable.BitmapDrawable";

    public interface BitmapDataProvider {

        String getBitmapConfigName();


        Dimension getDimension();

        // Downsizes the bitmap, in-place, to the newSize.
        boolean downsizeBitmap(Dimension newSize);


        byte[] getPixelBytes(Dimension size);
    }

    private interface BitmapExtractor {
        BufferedImage getImage(int w, int h, byte[] data);
    }

    protected static final Map<String, BitmapExtractor> SUPPORTED_FORMATS = new HashMap<>();

    static {
        SUPPORTED_FORMATS.put("\"ARGB_8888\"", new ARGB8888_BitmapExtractor());
        SUPPORTED_FORMATS.put("\"RGB_565\"", new RGB565_BitmapExtractor());
        SUPPORTED_FORMATS.put("\"ALPHA_8\"", new ALPHA8_BitmapExtractor());
    }

    /**
     * Maximum height or width of image beyond which we scale it on the device before retrieving.
     */
    private static final int MAX_DIMENSION = 1024;


    public static BufferedImage getBitmap(BitmapDataProvider dataProvider) {
        String config = dataProvider.getBitmapConfigName();
        if (config == null) {
            throw new RuntimeException("Unable to determine bitmap configuration");
        }

        BitmapExtractor bitmapExtractor = SUPPORTED_FORMATS.get(config);
        if (bitmapExtractor == null) {
            throw new RuntimeException("Unsupported bitmap configuration: " + config);
        }

        Dimension size = dataProvider.getDimension();
        if (size == null) {
            throw new RuntimeException("Unable to determine image dimensions.");
        }

        // if the image is rather large, then scale it down
        if (size.width > MAX_DIMENSION || size.height > MAX_DIMENSION) {
            boolean couldDownsize = dataProvider.downsizeBitmap(size);
            if (!couldDownsize) {
                throw new RuntimeException("Unable to create scaled bitmap");
            }

            size = dataProvider.getDimension();
            if (size == null) {
                throw new RuntimeException("Unable to obtained scaled bitmap's dimensions");
            }
        }

        return bitmapExtractor.getImage(size.width, size.height, dataProvider.getPixelBytes(size));
    }

    private static class ARGB8888_BitmapExtractor implements BitmapExtractor {
        @Override
        public BufferedImage getImage(int width, int height, byte[] rgba) {
            @SuppressWarnings("UndesirableClassUsage")
            BufferedImage bufferedImage = new BufferedImage(width, height,
                BufferedImage.TYPE_INT_ARGB);

            for (int y = 0; y < height; y++) {
                int stride = y * width;
                for (int x = 0; x < width; x++) {
                    int i = (stride + x) * 4;
                    long rgb = 0;
                    rgb |= ((long) rgba[i] & 0xff) << 16; // r
                    rgb |= ((long) rgba[i + 1] & 0xff) << 8;  // g
                    rgb |= ((long) rgba[i + 2] & 0xff);       // b
                    rgb |= ((long) rgba[i + 3] & 0xff) << 24; // a
                    bufferedImage.setRGB(x, y, (int) (rgb & 0xffffffffL));
                }
            }

            return bufferedImage;
        }
    }

    private static class RGB565_BitmapExtractor implements BitmapExtractor {
        @Override
        public BufferedImage getImage(int width, int height, byte[] rgb) {
            int bytesPerPixel = 2;

            @SuppressWarnings("UndesirableClassUsage")
            BufferedImage bufferedImage = new BufferedImage(width, height,
                BufferedImage.TYPE_INT_ARGB);

            for (int y = 0; y < height; y++) {
                int stride = y * width;
                for (int x = 0; x < width; x++) {
                    int index = (stride + x) * bytesPerPixel;
                    int value = (rgb[index] & 0x00ff) | (rgb[index + 1] << 8) & 0xff00;
                    // RGB565 to RGB888
                    // Multiply by 255/31 to convert from 5 bits (31 max) to 8 bits (255)
                    int r = ((value >>> 11) & 0x1f) * 255 / 31;
                    int g = ((value >>> 5) & 0x3f) * 255 / 63;
                    int b = ((value) & 0x1f) * 255 / 31;
                    int a = 0xFF;
                    int rgba = a << 24 | r << 16 | g << 8 | b;
                    bufferedImage.setRGB(x, y, rgba);
                }
            }

            return bufferedImage;
        }
    }

    private static class ALPHA8_BitmapExtractor implements BitmapExtractor {
        @Override
        public BufferedImage getImage(int width, int height, byte[] rgb) {
            //noinspection UndesirableClassUsage
            BufferedImage bufferedImage = new BufferedImage(width, height,
                BufferedImage.TYPE_INT_ARGB);

            for (int y = 0; y < height; y++) {
                int stride = y * width;
                for (int x = 0; x < width; x++) {
                    int index = stride + x;
                    int value = rgb[index];
                    int rgba = value << 24 | 0xff << 16 | 0xff << 8 | 0xff;
                    bufferedImage.setRGB(x, y, rgba);
                }
            }

            return bufferedImage;
        }
    }
}
