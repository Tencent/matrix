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

package com.tencent.matrix.resource.hproflib.model;

import java.util.HashMap;
import java.util.Map;

/**
 * Created by tangyinsheng on 2017/6/27.
 */

public enum Type {
    OBJECT(2, 0), // Pointer sizes are dependent on the hprof file, so set it to 0 for now.
    BOOLEAN(4, 1),
    CHAR(5, 2),
    FLOAT(6, 4),
    DOUBLE(7, 8),
    BYTE(8, 1),
    SHORT(9, 2),
    INT(10, 4),
    LONG(11, 8);

    private static Map<Integer, Type> sTypeMap = new HashMap<>();

    private int mId;

    private int mSize;

    static {
        for (Type type : Type.values()) {
            sTypeMap.put(type.mId, type);
        }
    }

    Type(int type, int size) {
        mId = type;
        mSize = size;
    }

    public static Type getType(int id) {
        return sTypeMap.get(id);
    }

    public int getSize(int idSize) {
        return mSize != 0 ? mSize : idSize;
    }

    public int getTypeId() {
        return mId;
    }

    public static String getClassNameOfPrimitiveArray(Type type) {
        switch (type) {
            case BOOLEAN: return "boolean[]";
            case CHAR: return "char[]";
            case FLOAT: return "float[]";
            case DOUBLE: return "double[]";
            case BYTE: return "byte[]";
            case SHORT: return "short[]";
            case INT: return "int[]";
            case LONG: return "long[]";
            default: throw new IllegalArgumentException("OBJECT type is not a primitive type");
        }
    }
}
