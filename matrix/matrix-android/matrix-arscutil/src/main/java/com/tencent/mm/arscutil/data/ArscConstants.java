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

package com.tencent.mm.arscutil.data;

/**
 * Created by jinqiuchen on 18/7/29.
 */

public class ArscConstants {

    public static final short RES_TABLE_TYPE_SPEC_TYPE = 514;
    public static final short RES_TABLE_TYPE_TYPE = 513;

    public static final int NO_ENTRY_INDEX = 0xFFFFFFFF;

    public static final int RES_STRING_POOL_SORTED_FLAG = 0x1;
    public static final int RES_STRING_POOL_UTF8_FLAG = 0x0100;

    public static final short RES_TABLE_ENTRY_FLAG_COMPLEX = 0x0001;
    public static final short RES_TABLE_ENTRY_FLAG_PUBLIC = 0x0002;

    public static final int RES_VALUE_DATA_TYPE_NULL = 0; //The 'data' is either 0 or 1, specifying this resource is either undefined or empty.
    public static final int RES_VALUE_DATA_TYPE_REFERENCE = 0x01; //The 'data' holds a ResTable_ref, a reference to another resource table entry.
    public static final int RES_VALUE_DATA_TYPE_ATTRIBUTE = 0x02; //The 'data' holds an attribute resource identifier.
    public static final int RES_VALUE_DATA_TYPE_STRING = 0x03; //The 'data' holds an index into the containing resource table's global value string pool.
    public static final int RES_VALUE_DATA_TYPE_FLOAT = 0x04; //The 'data' holds a single-precision floating point number.
    public static final int RES_VALUE_DATA_TYPE_DIMENSION = 0x05; //The 'data' holds a complex number encoding a dimension value.
    public static final int RES_VALUE_DATA_TYPE_FRACTION = 0x06; //The 'data' holds a complex number encoding a fraction of a container.
    public static final int RES_VALUE_DATA_TYPE_DYNAMIC_REFERENCE = 0x07; //The 'data' holds a dynamic ResTable_ref, which needs to be resolved before it can be used like a TYPE_REFERENCE.
    public static final int RES_VALUE_DATA_TYPE_DYNAMIC_ATTRIBUTE = 0x08; // The 'data' holds an attribute resource identifier, which needs to be resolved before it can be used like a TYPE_ATTRIBUTE.

    public static final int RES_VALUE_DATA_TYPE_FIRST_INT = 0x10;

    public static final int RES_VALUE_DATA_TYPE_INT_DEC = 0x10; //The 'data' is a raw integer value of the form n..n.
    public static final int RES_VALUE_DATA_TYPE_INT_HEX = 0x11; // The 'data' is a raw integer value of the form 0xn..n.
    public static final int RES_VALUE_DATA_TYPE_INT_BOOLEAN = 0x12; // The 'data' is either 0 or 1, for input "false" or "true" respectively.

    public static final int RES_VALUE_DATA_TYPE_FIRST_COLOR_INT = 0x1c;

    public static final int RES_VALUE_DATA_TYPE_INT_COLOR_ARGB8 = 0x1c; // The 'data' is a raw integer value of the form #aarrggbb.
    public static final int RES_VALUE_DATA_TYPE_INT_COLOR_RGB8 = 0x1d; // The 'data' is a raw integer value of the form #rrggbb.
    public static final int RES_VALUE_DATA_TYPE_INT_COLOR_ARGB4 = 0x1e; //The 'data' is a raw integer value of the form #argb.
    public static final int RES_VALUE_DATA_TYPE_INT_COLOR_RGB4 = 0x1f;     // The 'data' is a raw integer value of the form #rgb.

    public static final int RES_VALUE_DATA_TYPE_LAST_COLOR_INT = 0x1f;

    public static final int RES_VALUE_DATA_TYPE_LAST_INT = 0x1f;
}
