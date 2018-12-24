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

package com.tencent.mm.arscutil.io;

import com.tencent.matrix.javalib.util.Log;
import com.tencent.mm.arscutil.data.ResTable;

import java.io.File;
import java.io.IOException;

/**
 * Created by jinqiuchen on 18/7/29.
 */

public class ArscWriter {

    private static final String TAG = "ArscUtil.ArscWriter";

    LittleEndianOutputStream dataOutput;

    public ArscWriter(String arscFile) throws IOException {
        File file = new File(arscFile);
        Log.i(TAG, "write to %s", arscFile);
        if (file.exists()) {
            file.delete();
        }
        file.getParentFile().mkdirs();
        file.createNewFile();
        dataOutput = new LittleEndianOutputStream(arscFile);
    }

    public void writeResTable(ResTable resTable) throws Exception {
        dataOutput.write(resTable.toBytes());
        dataOutput.close();
    }
}
