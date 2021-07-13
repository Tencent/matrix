/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

package com.tencent.matrix.trace.items;

public class MethodItem {

    public int methodId;
    public int durTime;
    public int depth;
    public int count = 1;

    public MethodItem(int methodId, int durTime, int depth) {
        this.methodId = methodId;
        this.durTime = durTime;
        this.depth = depth;
    }

    @Override
    public String toString() {
        return depth + "," + methodId + "," + count + "," + durTime;
    }

    public void mergeMore(long cost) {
        count++;
        durTime += cost;
    }

    public String print() {
        StringBuffer inner = new StringBuffer();
        for (int i = 0; i < depth; i++) {
            inner.append('.');
        }
        return inner.toString() + methodId + " " + count + " " + durTime;
    }
}
