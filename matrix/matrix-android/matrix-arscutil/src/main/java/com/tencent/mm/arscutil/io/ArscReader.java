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
import com.tencent.mm.arscutil.ArscUtil;
import com.tencent.mm.arscutil.data.ArscConstants;
import com.tencent.mm.arscutil.data.ResChunk;
import com.tencent.mm.arscutil.data.ResConfig;
import com.tencent.mm.arscutil.data.ResEntry;
import com.tencent.mm.arscutil.data.ResMapValue;
import com.tencent.mm.arscutil.data.ResPackage;
import com.tencent.mm.arscutil.data.ResStringBlock;
import com.tencent.mm.arscutil.data.ResTable;
import com.tencent.mm.arscutil.data.ResType;
import com.tencent.mm.arscutil.data.ResTypeSpec;
import com.tencent.mm.arscutil.data.ResValue;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.List;

/**
 * Created by jinqiuchen on 18/7/29.
 */

public class ArscReader {

    private static final String TAG = "ArscUtil.ArscReader";

    LittleEndianInputStream dataInput;
    private ResTable globalResTable;

    public ArscReader(String arscFile) throws FileNotFoundException {
        dataInput = new LittleEndianInputStream(arscFile);
        Log.i(TAG, "read From %s", arscFile);
    }

    public ResTable readResourceTable() throws IOException {
        Log.d(TAG, "=============ResTable==============");
        long headStart = 0;
        ResTable resTable = new ResTable();
        this.globalResTable = resTable;
        resTable.setStart(headStart);
        resTable.setType(dataInput.readShort());
        Log.d(TAG, "table type %d", resTable.getType());
        resTable.setHeadSize(dataInput.readShort());
        //Log.d(TAG, "head size %d", resTable.getHeadSize());
        resTable.setChunkSize(dataInput.readInt());
        //Log.d(TAG, "chunk size %f KB", resTable.getChunkSize() / 1024.0f);
        resTable.setPackageCount(dataInput.readInt());
        Log.d(TAG, "package count %d", resTable.getPackageCount());
        int headPaddingSize = (int) (resTable.getHeadSize() + headStart - dataInput.getFilePointer());
        if (headPaddingSize > 0) {
        	byte[] headPadding = new byte[headPaddingSize];
        	dataInput.read(headPadding);
        	resTable.setHeadPadding(headPadding);
        }
        resTable.setGlobalStringPool(readStringBlock());
        Log.d(TAG, "global string pool pos %d", dataInput.getFilePointer());
        if (resTable.getPackageCount() > 0) {
            ResPackage[] packages = new ResPackage[resTable.getPackageCount()];
            for (int i = 0; i < resTable.getPackageCount(); i++) {
                packages[i] = readPackage();
            }
            resTable.setPackages(packages);
        }
        int chunkPaddingSize = (int) (resTable.getChunkSize() + headStart - dataInput.getFilePointer());
        if (chunkPaddingSize > 0) {
        	byte[] chunkPadding = new byte[chunkPaddingSize];
            dataInput.read(chunkPadding);
            resTable.setChunkPadding(chunkPadding);
        }
        dataInput.close();
        return resTable;
    }

    private ResPackage readPackage() throws IOException {
        Log.d(TAG, "=============ResPackage==============");
        long headStart = dataInput.getFilePointer();
        Log.d(TAG, "package start %d", headStart);
        ResPackage resPackage = new ResPackage();
        resPackage.setStart(headStart);
        resPackage.setType(dataInput.readShort());
        Log.d(TAG, "package type %d", resPackage.getType());
        resPackage.setHeadSize(dataInput.readShort());
        //Log.d(TAG, "head size %d", resPackage.getHeadSize());
        resPackage.setChunkSize(dataInput.readInt());
        //Log.d(TAG, "chunk size %d", resPackage.getChunkSize());
        resPackage.setId(dataInput.readInt());
        Log.d(TAG, "package id %d", resPackage.getId());
        byte[] buffer = new byte[256];
        dataInput.read(buffer);
        resPackage.setName(buffer);
        Log.d(TAG, "package name %s", ArscUtil.toUTF16String(buffer));
        resPackage.setResTypePoolOffset(dataInput.readInt());
        Log.d(TAG, "resType pool offset %d", resPackage.getResTypePoolOffset());
        resPackage.setLastPublicType(dataInput.readInt());
        //Log.d(TAG, "lastPublicType index %d", resPackage.getLastPublicType());
        resPackage.setResNamePoolOffset(dataInput.readInt());
        Log.d(TAG, "resName pool offset %d", resPackage.getResNamePoolOffset());
        resPackage.setLastPublicName(dataInput.readInt());
        //Log.d(TAG, "lastPublicName index %d", resPackage.getLastPublicName());
        int headPaddingSize = (int) (resPackage.getHeadSize() + headStart - dataInput.getFilePointer());
        if (headPaddingSize > 0) {
        	byte[] headPadding = new byte[headPaddingSize];
            dataInput.read(headPadding);
            resPackage.setHeadPadding(headPadding);
        }
        if (resPackage.getResTypePoolOffset() > 0) {
            dataInput.seek(headStart + resPackage.getResTypePoolOffset());
            ResStringBlock resTypePool = readStringBlock();
            resPackage.setResTypePool(resTypePool);
        }
        if (resPackage.getResNamePoolOffset() > 0) {
            dataInput.seek(headStart + resPackage.getResNamePoolOffset());
            ResStringBlock resNamePool = readStringBlock();
            resPackage.setResNamePool(resNamePool);
        }
        List<ResChunk> resTypeList = new ArrayList<ResChunk>();
        while (dataInput.getFilePointer() < (resPackage.getStart() + resPackage.getChunkSize())) {
            int type = dataInput.readShort();
            if (type == ArscConstants.RES_TABLE_TYPE_SPEC_TYPE) {
                dataInput.seek(dataInput.getFilePointer() - 2);
                ResTypeSpec resTypeSpec = readResTypeSpec();
                resTypeList.add(resTypeSpec);
            } else if (type == ArscConstants.RES_TABLE_TYPE_TYPE) {
                dataInput.seek(dataInput.getFilePointer() - 2);
                ResType resType = readResType(resPackage);
                resTypeList.add(resType);
            }
        }
        resPackage.setResTypeArray(resTypeList);
        int chunkPaddingSize = (int) (resPackage.getChunkSize() + headStart - dataInput.getFilePointer());
        if (chunkPaddingSize > 0) {
        	 byte[] chunkPadding = new byte[chunkPaddingSize];
             dataInput.read(chunkPadding);
             resPackage.setChunkPadding(chunkPadding);
        }
        return resPackage;
    }

    private ResTypeSpec readResTypeSpec() throws IOException {
        Log.d(TAG, "==============ResTypeSpec=============");
        long headStart = dataInput.getFilePointer();
        ResTypeSpec resTypeSpec = new ResTypeSpec();
        resTypeSpec.setStart(headStart);
        resTypeSpec.setType(dataInput.readShort());
        Log.d(TAG, "resTypeSpec type %d", resTypeSpec.getType());
        resTypeSpec.setHeadSize(dataInput.readShort());
        //Log.d(TAG, "resTypeSpec header size %d", resTypeSpec.getHeadSize());
        resTypeSpec.setChunkSize(dataInput.readInt());
        //Log.d(TAG, "resTypeSpec chunk size %d", resTypeSpec.getChunkSize());
        resTypeSpec.setId(dataInput.readByte());
        Log.d(TAG, "resTypeSpec type id %d", resTypeSpec.getId());
        resTypeSpec.setReserved0(dataInput.readByte());
        resTypeSpec.setReserved1(dataInput.readShort());
        resTypeSpec.setEntryCount(dataInput.readInt());
        Log.d(TAG, "resTypeSpec entry count %d", resTypeSpec.getEntryCount());
        int headPaddingSize = (int) (resTypeSpec.getHeadSize() + headStart - dataInput.getFilePointer());
        if (headPaddingSize > 0) {
        	 byte[] headPadding = new byte[headPaddingSize];
             dataInput.read(headPadding);
             resTypeSpec.setHeadPadding(headPadding);
        }
        if (resTypeSpec.getChunkSize() - resTypeSpec.getHeadSize() > 0) {
            byte[] buffer = new byte[resTypeSpec.getChunkSize() - resTypeSpec.getHeadSize()];
            dataInput.read(buffer);
            resTypeSpec.setConfigFlags(buffer);
        }
        int chunkPaddingSize = (int) (resTypeSpec.getChunkSize() + headStart - dataInput.getFilePointer());
        if (chunkPaddingSize > 0) {
        	byte[] chunkPadding = new byte[chunkPaddingSize];
            dataInput.read(chunkPadding);
            resTypeSpec.setChunkPadding(chunkPadding);
        }
        return resTypeSpec;
    }

    private ResType readResType(ResPackage resPackage) throws IOException {
        Log.d(TAG, "=============ResType==============");
        long headStart = dataInput.getFilePointer();
        ResType resType = new ResType();
        resType.setStart(headStart);
        resType.setType(dataInput.readShort());
        Log.d(TAG, "resType type %d", resType.getType());
        resType.setHeadSize(dataInput.readShort());
        //Log.d(TAG, "resType header size %d", resType.getHeadSize());
        resType.setChunkSize(dataInput.readInt());
        //Log.d(TAG, "resType chunk size %d", resType.getChunkSize());
        resType.setId(dataInput.readByte());
        //Log.d(TAG, "resType type id %d", resType.getId());
        resType.setReserved0(dataInput.readByte());
        resType.setReserved1(dataInput.readShort());
        resType.setEntryCount(dataInput.readInt());
        //Log.d(TAG, "resType entry count %d", resType.getEntryCount());
        resType.setEntryTableOffset(dataInput.readInt());
        //Log.d(TAG, "resType entryTable offset %d", resType.getEntryTableOffset());
        resType.setResConfigFlags(readResConfig());
        int headPaddingSize = (int) (resType.getHeadSize() + headStart - dataInput.getFilePointer());
        if (headPaddingSize > 0) {
        	 byte[] headPadding = new byte[headPaddingSize];
             dataInput.read(headPadding);
             resType.setHeadPadding(headPadding);
        }
        if (resType.getEntryCount() > 0) {
            List<Integer> resEntryOffsets = new ArrayList<Integer>();
            for (int i = 0; i < resType.getEntryCount(); i++) {
                resEntryOffsets.add(dataInput.readInt());
            }
            resType.setEntryOffsets(resEntryOffsets);
        }
        dataInput.seek(headStart + resType.getEntryTableOffset());
        List<ResEntry> entryTable = new ArrayList<ResEntry>();
        for (int i = 0; i < resType.getEntryCount(); i++) {
            if (resType.getEntryOffsets().get(i) != ArscConstants.NO_ENTRY_INDEX) {
                entryTable.add(readResEntry(resPackage,
                        headStart + resType.getEntryTableOffset() + resType.getEntryOffsets().get(i)));
            } else {
                entryTable.add(null);
            }
        }
        resType.setEntryTable(entryTable);
        int chunkPaddingSize = (int) (resType.getChunkSize() + headStart - dataInput.getFilePointer());
        if (chunkPaddingSize > 0) {
        	byte[] chunkPadding = new byte[chunkPaddingSize];
            dataInput.read(chunkPadding);
            resType.setChunkPadding(chunkPadding);
        }
        return resType;
    }

    @SuppressWarnings("PMD")
    private ResEntry readResEntry(ResPackage resPackage, long start) throws IOException {
        Log.d(TAG, "==============ResEntry=============");
        dataInput.seek(start);
        ResEntry resEntry = new ResEntry();
        resEntry.setSize(dataInput.readShort());
        //Log.d(TAG, "resEntry size %d", resEntry.getSize());
        resEntry.setFlag(dataInput.readShort());
        Log.d(TAG, "resEntry flag %d", resEntry.getFlag());
        resEntry.setStringPoolIndex(dataInput.readInt());

        Log.d(TAG, "entryName %s", ArscUtil.resolveStringPoolEntry(resPackage.getResNamePool().getStrings().get(resEntry.getStringPoolIndex()).array(), resPackage.getResNamePool().getCharSet()));
        if ((resEntry.getFlag() & ArscConstants.RES_TABLE_ENTRY_FLAG_COMPLEX) == 0) {
            resEntry.setResValue(readResValue());
        } else {
            resEntry.setParent(dataInput.readInt());
            resEntry.setPairCount(dataInput.readInt());
            if (resEntry.getPairCount() > 0) {
                List<ResMapValue> mapValues = new ArrayList<ResMapValue>();
                for (int i = 0; i < resEntry.getPairCount(); i++) {
                    mapValues.add(readResMapValue());
                }
                resEntry.setResMapValues(mapValues);
            }
        }
        return resEntry;
    }

    private ResValue readResValue() throws IOException {
    	Log.d(TAG,"============ResValue===============");
        ResValue resValue = new ResValue();
        resValue.setSize(dataInput.readShort());
        //Log.d(TAG, "resValue size %d", resValue.getSize());
        resValue.setResvered(dataInput.readByte());
        resValue.setDataType(dataInput.readByte());
        Log.d(TAG, "resValue data type %d", resValue.getDataType());
        resValue.setData(dataInput.readInt());
        //Log.d(TAG, "resValue data %d", resValue.getData());
              
        if (resValue.getDataType() == ArscConstants.RES_VALUE_DATA_TYPE_STRING) {
        	Log.d(TAG,  "resValue string %s", ArscUtil.resolveStringPoolEntry(globalResTable.getGlobalStringPool().getStrings().get(resValue.getData()).array(), globalResTable.getGlobalStringPool().getCharSet()));
        } else {
        	Log.d(TAG, "resValue %s", resValue.printData());
        }
       
        return resValue;
    }

    private ResMapValue readResMapValue() throws IOException {
        Log.d(TAG, "==============ResMapValue=============");
        ResMapValue resValue = new ResMapValue();
        resValue.setName(dataInput.readInt());
        resValue.setResValue(readResValue());
        return resValue;
    }

    private ResConfig readResConfig() throws IOException {
        Log.d(TAG, "==============ResConfig=============");
        ResConfig config = new ResConfig();
        config.setSize(dataInput.readInt());
        //Log.d(TAG, "resConfig size %d", config.getSize());
        if (config.getSize() > 4) {
            byte[] buffer = new byte[config.getSize() - 4];
            dataInput.read(buffer);
            config.setContent(buffer);
        }
        return config;
    }

    private ResStringBlock readStringBlock() throws IOException {
        Log.d(TAG, "==============ResStringBlock=============");
        long headStart = dataInput.getFilePointer();
        ResStringBlock stringPool = new ResStringBlock();
        stringPool.setStart(headStart);
        stringPool.setType(dataInput.readShort());
        Log.d(TAG, "stringPool type %d", stringPool.getType());
        stringPool.setHeadSize(dataInput.readShort());
        //Log.d(TAG, "stringPool head size %d", stringPool.getHeadSize());
        stringPool.setChunkSize(dataInput.readInt());
        //Log.d(TAG, "stringPool chunk size %d", stringPool.getChunkSize());
        stringPool.setStringCount(dataInput.readInt());
        Log.d(TAG, "stringPool string count %d", stringPool.getStringCount());
        stringPool.setStyleCount(dataInput.readInt());
        Log.d(TAG, "stringPool style count %d", stringPool.getStyleCount());
        stringPool.setFlag(dataInput.readInt());
        Log.d(TAG, "stringPool flag %d", stringPool.getFlag());
        stringPool.setStringStart(dataInput.readInt());
        Log.d(TAG, "stringPool string start %d", stringPool.getStringStart());
        stringPool.setStyleStart(dataInput.readInt());
        Log.d(TAG, "stringPool style start %d", stringPool.getStyleStart());
        int headPaddingSize = (int) (stringPool.getHeadSize() + headStart - dataInput.getFilePointer());
        if (headPaddingSize > 0) {
        	 byte[] headPadding = new byte[headPaddingSize];
             dataInput.read(headPadding);
             stringPool.setHeadPadding(headPadding);
        }
        dataInput.seek(headStart + stringPool.getHeadSize());
        if (stringPool.getStringCount() > 0) {
            List<Integer> stringOffsets = new ArrayList<Integer>();
            for (int i = 0; i < stringPool.getStringCount(); i++) {
                stringOffsets.add(dataInput.readInt());
            }
            stringPool.setStringOffsets(stringOffsets);
        }
        if (stringPool.getStyleCount() > 0) {
            List<Integer> styleOffsets = new ArrayList<Integer>();
            for (int i = 0; i < stringPool.getStyleCount(); i++) {
                styleOffsets.add(dataInput.readInt());
            }
            stringPool.setStyleOffsets(styleOffsets);
        }
        dataInput.seek(headStart + stringPool.getStringStart());
        if (stringPool.getStringCount() > 0) {
            List<ByteBuffer> strings = new ArrayList<ByteBuffer>();
            for (int i = 0; i < stringPool.getStringCount(); i++) {
                byte[] buffer = null;
                if (i < stringPool.getStringCount() - 1) {
                    buffer = new byte[stringPool.getStringOffsets().get(i + 1) - stringPool.getStringOffsets().get(i)];
                } else {
                    if (stringPool.getStyleCount() > 0) {
                        buffer = new byte[stringPool.getStyleStart() - (stringPool.getStringOffsets().get(i) + stringPool.getStringStart())];
                    } else {
                        buffer = new byte[stringPool.getChunkSize() - stringPool.getStringStart() - stringPool.getStringOffsets().get(i)];
                    }
                }
                dataInput.read(buffer);
                strings.add(ByteBuffer.allocate(buffer.length));
                strings.get(i).order(ByteOrder.LITTLE_ENDIAN);
                strings.get(i).clear();
                strings.get(i).put(buffer);
            }
            stringPool.setStrings(strings);
        }
        if (stringPool.getStyleCount() > 0) {
            byte[] styleBytes = new byte[stringPool.getChunkSize() - stringPool.getStyleStart()];
            dataInput.read(styleBytes);
            stringPool.setStyles(styleBytes);
        }

        int chunkPaddingSize = (int) (stringPool.getChunkSize() + headStart - dataInput.getFilePointer());
        if (chunkPaddingSize > 0) {
        	byte[] chunkPadding = new byte[chunkPaddingSize];
            dataInput.read(chunkPadding);
            stringPool.setChunkPadding(chunkPadding);
        }
        return stringPool;
    }
}
