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

package com.tencent.matrix.apk.model.output;

import com.tencent.matrix.apk.model.result.TaskHtmlResult;
import com.tencent.matrix.apk.model.result.TaskJsonResult;
import com.tencent.matrix.apk.model.result.TaskResultRegistry;

import java.util.HashMap;
import java.util.Map;

/**
 * Created by williamjin on 17/9/1.
 */

public class MMTaskResultRegistry extends TaskResultRegistry {

    @Override
    public Map<String, Class<? extends TaskHtmlResult>> getHtmlResult() {
        Map<String, Class<? extends TaskHtmlResult>> map = new HashMap<>();
        map.put("mm.html", MMTaskHtmlResult.class);
        return map;
    }

    @Override
    public Map<String, Class<? extends TaskJsonResult>> getJsonResult() {
        Map<String, Class<? extends TaskJsonResult>> map = new HashMap<>();
        map.put("mm.json", MMTaskJsonResult.class);
        return map;
    }
}
