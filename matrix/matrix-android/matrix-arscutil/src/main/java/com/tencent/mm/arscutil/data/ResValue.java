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

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Created by jinqiuchen on 18/7/29.
 */

public class ResValue {

    private short size;                    // 2 bytes，大小
    private byte res0;					   // 保留字段，始终为0
    private byte dataType;		   			// 数据类型
    private int data;					   // 数据的内容，根据dataType解析，例如：对于TYPE_STRING，data就是global string pool中的index


    public short getSize() {
        return size;
    }

    public void setSize(short size) {
        this.size = size;
    }
    
    public void setResvered(byte res) {
    	this.res0 = res;
    }
    
    public void setDataType(byte dataType) {
    	this.dataType = dataType;
    }
    
    public byte getDataType() {
    	return this.dataType;
    }
    
    public int getData() {
    	return data;
    }
    
    public void setData(int data) {
    	this.data = data;
    }
    
    public String printData() {
    	switch (dataType) {
    		case ArscConstants.RES_VALUE_DATA_TYPE_NULL:
    			return "[Null]";
    		case ArscConstants.RES_VALUE_DATA_TYPE_REFERENCE:
    			return "reference:" + data;
    		case ArscConstants.RES_VALUE_DATA_TYPE_STRING:
    			return "string:" + data;
    		case ArscConstants.RES_VALUE_DATA_TYPE_FLOAT:
    			return "float:" + Float.intBitsToFloat(data);
    		case ArscConstants.RES_VALUE_DATA_TYPE_INT_DEC:
    			return "integer:" + String.format("%d", data);
    		case ArscConstants.RES_VALUE_DATA_TYPE_INT_HEX:
    			return "integer:" + String.format("%x", data);
    		case ArscConstants.RES_VALUE_DATA_TYPE_INT_BOOLEAN:
    			return "boolean:" + String.format("%b", data);
    		case ArscConstants.RES_VALUE_DATA_TYPE_INT_COLOR_ARGB8:
    			return "color:" + String.format("#%8x", data);
    		case ArscConstants.RES_VALUE_DATA_TYPE_INT_COLOR_RGB8:
    			return "color:" + String.format("#%6x", data);
    		case ArscConstants.RES_VALUE_DATA_TYPE_INT_COLOR_ARGB4:
    			return "color:" + String.format("#%4x", data);
    		case ArscConstants.RES_VALUE_DATA_TYPE_INT_COLOR_RGB4:
    			return "color:" + String.format("#%3x", data);
    		default:
    			return "other:" + data;
    			
    	}
    }
    

    public byte[] toBytes() throws IOException {
        ByteBuffer byteBuffer = ByteBuffer.allocate(size);
        byteBuffer.order(ByteOrder.LITTLE_ENDIAN);
        byteBuffer.clear();
        byteBuffer.putShort(size);
        byteBuffer.put(res0);
        byteBuffer.put(dataType);
        byteBuffer.putInt(data);
        byteBuffer.flip();
        return byteBuffer.array();
    }
}
