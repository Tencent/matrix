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

package com.tencent.matrix.trace.listeners;

import android.os.Build;
import android.view.FrameMetrics;

import androidx.annotation.RequiresApi;

/**
 * Use {@link ISceneFrameListener} to analyze frame metrics of specified scene, or use
 * {@link IDropFrameListener} to only analyze dropped frame.
 */
@RequiresApi(Build.VERSION_CODES.N)
public interface IFrameListener {
    void onFrameMetricsAvailable(String sceneName, FrameMetrics frameMetrics, float droppedFrames, float refreshRate);
}
