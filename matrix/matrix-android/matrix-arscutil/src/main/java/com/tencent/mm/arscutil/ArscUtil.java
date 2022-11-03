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

package com.tencent.mm.arscutil;

import com.tencent.matrix.javalib.util.Log;
import com.tencent.mm.arscutil.data.ArscConstants;
import com.tencent.mm.arscutil.data.ResChunk;
import com.tencent.mm.arscutil.data.ResEntry;
import com.tencent.mm.arscutil.data.ResPackage;
import com.tencent.mm.arscutil.data.ResStringBlock;
import com.tencent.mm.arscutil.data.ResTable;
import com.tencent.mm.arscutil.data.ResType;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.CharBuffer;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Created by jinqiuchen on 18/7/29.
 */

public class ArscUtil {

    private static final String TAG = "ArscUtil.ArscUtil";

    public static String toUTF16String(byte[] buffer) {
        CharBuffer charBuffer = ByteBuffer.wrap(buffer).order(ByteOrder.LITTLE_ENDIAN).asCharBuffer();
        int index = 0;
        for (; index < charBuffer.length(); index++) {
            if (charBuffer.get() == 0x00) {
                break;
            }
        }
        charBuffer.limit(index).position(0);
        return charBuffer.toString();
    }

    public static int getPackageId(int resourceId) {
        return (resourceId & 0xFF000000) >> 24;
    }

    public static int getResourceTypeId(int resourceId) {
        return (resourceId & 0x00FF0000) >> 16;
    }

    public static int getResourceEntryId(int resourceId) {
        return resourceId & 0x0000FFFF;
    }

    public static ResPackage findResPackage(ResTable resTable, int packageId) {
        ResPackage resPackage = null;
        for (ResPackage pkg : resTable.getPackages()) {
            if (pkg.getId() == packageId) {
                resPackage = pkg;
                break;
            }
        }
        return resPackage;
    }

    public static List<ResType> findResType(ResPackage resPackage, int resourceId) {
        ResType resType = null;
        int typeId = (resourceId & 0X00FF0000) >> 16;
        int entryId = resourceId & 0x0000FFFF;
        List<ResType> resTypeList = new ArrayList<ResType>();
        List<ResChunk> resTypeArray = resPackage.getResTypeArray();
        if (resTypeArray != null) {
            for (int i = 0; i < resTypeArray.size(); i++) {
                if (resTypeArray.get(i).getType() == ArscConstants.RES_TABLE_TYPE_TYPE
                        && ((ResType) resTypeArray.get(i)).getId() == typeId) {
                    int entryCount = ((ResType) resTypeArray.get(i)).getEntryCount();
                    if (entryId < entryCount) {
                        int offset = ((ResType) resTypeArray.get(i)).getEntryOffsets().get(entryId);
                        if (offset != ArscConstants.NO_ENTRY_INDEX) {
                            resType = ((ResType) resTypeArray.get(i));
                            resTypeList.add(resType);
                        }
                    }
                }
            }
        }
        return resTypeList;
    }

    public static void removeResource(ResTable resTable, int resourceId, String resourceName) throws IOException {
        ResPackage resPackage = findResPackage(resTable, getPackageId(resourceId));
        if (resPackage != null) {
            List<ResType> resTypeList = findResType(resPackage, resourceId);
            int resNameStringPoolIndex = -1;
            for (ResType resType : resTypeList) {
                int entryId = getResourceEntryId(resourceId);
                resNameStringPoolIndex = resType.getEntryTable().get(entryId).getStringPoolIndex();
                resType.removeEntry(entryId);
                resType.refresh();
            }
            if (resNameStringPoolIndex != -1) {
                Log.d(TAG, "try to remove %s (%H), find resource %s", resourceName, resourceId, ResStringBlock.resolveStringPoolEntry(resPackage.getResNamePool().getStrings().get(resNameStringPoolIndex).array(), resPackage.getResNamePool().getCharSet()));
            }
            resPackage.shrinkResNameStringPool();
            resPackage.refresh();
            resTable.refresh();
        }
    }

    public static boolean replaceFileResource(ResTable resTable, int sourceResId, String sourceFile, int targetResId, String targetFile) throws IOException {
        int sourcePkgId = getPackageId(sourceResId);
        int targetPkgId = getPackageId(targetResId);
        Log.d(TAG, "try to replace %H(%s) with %H(%s)", sourceResId, sourceFile, targetResId, targetFile);
        if (sourcePkgId == targetPkgId) {
            ResPackage resPackage = findResPackage(resTable, sourcePkgId);
            if (resPackage != null) {
                List<ResType> targetResTypeList = findResType(resPackage, targetResId);
                int targetFileIndex = -1;
                //find the index of targetFile in the string pool
                for (ResType targetResType : targetResTypeList) {
                    int entryId = getResourceEntryId(targetResId);
                    ResEntry resEntry = targetResType.getEntryTable().get(entryId);
                    boolean isComplex = (resEntry.getFlag() & ArscConstants.RES_TABLE_ENTRY_FLAG_COMPLEX) != 0;
                    if (!isComplex && resEntry.getResValue() != null) {
                        if (resEntry.getResValue().getDataType() == ArscConstants.RES_VALUE_DATA_TYPE_STRING) {
                            String filePath = ResStringBlock.resolveStringPoolEntry(resTable.getGlobalStringPool().getStrings().get(resEntry.getResValue().getData()).array(), resTable.getGlobalStringPool().getCharSet());
                            if (filePath.equals(targetFile)) {
                                targetFileIndex = resEntry.getResValue().getData();
                                break;
                            } else {
                                Log.w(TAG, "find target file %s, %s was expected", filePath, targetFile);
                                continue;
                            }
                        }
                    }
                }
                if (targetFileIndex == -1) {
                    Log.w(TAG, "can not find target file %s in resource %H", targetFile, targetResId);
                    return false;
                }
                //find the index of sourceFile in the string pool, and then replace it with the index of targetFile
                int sourceFileIndex = -1;
                List<ResType> sourceResTypeList = findResType(resPackage, sourceResId);
                for (ResType sourceResType : sourceResTypeList) {
                    int entryId = getResourceEntryId(sourceResId);
                    ResEntry resEntry = sourceResType.getEntryTable().get(entryId);
                    boolean isComplex = (resEntry.getFlag() & ArscConstants.RES_TABLE_ENTRY_FLAG_COMPLEX) != 0;
                    if (!isComplex && resEntry.getResValue() != null) {
                        if (resEntry.getResValue().getDataType() == ArscConstants.RES_VALUE_DATA_TYPE_STRING) {
                            String filePath = ResStringBlock.resolveStringPoolEntry(resTable.getGlobalStringPool().getStrings().get(resEntry.getResValue().getData()).array(), resTable.getGlobalStringPool().getCharSet());
                            if (filePath.equals(sourceFile)) {
                                sourceFileIndex = resEntry.getResValue().getData();
                                resEntry.getResValue().setData(targetFileIndex);
                                sourceResType.refresh();
                            } else {
                                Log.w(TAG, "find source file %s, %s was expected", filePath, sourceFile);
                                continue;
                            }
                        }
                    }
                }
                if (sourceFileIndex != -1) {
                    return true;
                }
            }
        } else {
            Log.w(TAG, "sourcePkgId %d != targetPkgId %d, quit replace!", sourcePkgId, targetPkgId);
        }
        return false;
    }

    public static void replaceResEntryName(ResTable resTable, Map<Integer, String> resIdProguard) {
        Set<ResPackage> updatePackages = new HashSet<>();
        for (int resId : resIdProguard.keySet()) {
            ResPackage resPackage = findResPackage(resTable, getPackageId(resId));
            if (resPackage != null) {
                //new proguard string block
                if (resPackage.getResProguardPool() == null) {
                    ResStringBlock resProguardBlock = new ResStringBlock();

                    resProguardBlock.setType(resPackage.getResNamePool().getType());
                    resProguardBlock.setStart(resPackage.getResNamePool().getStart());
                    resProguardBlock.setHeadSize(resPackage.getResNamePool().getHeadSize());
                    resProguardBlock.setHeadPadding(resPackage.getResNamePool().getHeadPadding());
                    resProguardBlock.setChunkPadding(resPackage.getResNamePool().getChunkPadding());

                    resProguardBlock.setStyleCount(resPackage.getResNamePool().getStyleCount());
                    resProguardBlock.setFlag(resPackage.getResNamePool().getFlag());
                    resProguardBlock.setStringStart(resPackage.getResNamePool().getStringStart());
                    resProguardBlock.setStyleOffsets(resPackage.getResNamePool().getStyleOffsets());
                    resProguardBlock.setStyles(resPackage.getResNamePool().getStyles());
                    resProguardBlock.setStrings(new ArrayList<>());
                    resProguardBlock.setStringOffsets(new ArrayList<>());
                    resProguardBlock.setStringIndexMap(new HashMap<>());
                    resPackage.setResProguardPool(resProguardBlock);
                }

                List<ResType> resTypeList = findResType(resPackage, resId);
                for (ResType resType : resTypeList) {
                    int entryId = getResourceEntryId(resId);
                    ResEntry resEntry = resType.getEntryTable().get(entryId);
                    resEntry.setEntryName(resIdProguard.get(resId));

                    if (!resPackage.getResProguardPool().getStringIndexMap().containsKey(resEntry.getEntryName())) {
                        resPackage.getResProguardPool().getStrings().add(ByteBuffer.wrap(ResStringBlock.encodeStringPoolEntry(resEntry.getEntryName(), resPackage.getResProguardPool().getCharSet())));
                        resPackage.getResProguardPool().setStringCount(resPackage.getResProguardPool().getStrings().size());
                        resPackage.getResProguardPool().getStringIndexMap().put(resEntry.getEntryName(), resPackage.getResProguardPool().getStringCount() - 1);
                    }
                }
                updatePackages.add(resPackage);
            }
        }

        for (ResPackage resPackage : updatePackages) {
            List<ResChunk> resTypeArray = resPackage.getResTypeArray();
            if (resTypeArray != null) {
                for (ResChunk resChunk : resTypeArray) {
                    if (resChunk.getType() == ArscConstants.RES_TABLE_TYPE_TYPE) {
                        ResType resType = ((ResType) resChunk);
                        for (ResEntry resEntry : resType.getEntryTable()) {
                            if (resEntry != null) {
                                if (!resPackage.getResProguardPool().getStringIndexMap().containsKey(resEntry.getEntryName())) {
                                    resPackage.getResProguardPool().getStrings().add(ByteBuffer.wrap(ResStringBlock.encodeStringPoolEntry(resEntry.getEntryName(), resPackage.getResProguardPool().getCharSet())));
                                    resPackage.getResProguardPool().setStringCount(resPackage.getResProguardPool().getStrings().size());
                                    resPackage.getResProguardPool().getStringIndexMap().put(resEntry.getEntryName(), resPackage.getResProguardPool().getStringCount() - 1);
                                }
                                resEntry.setStringPoolIndex(resPackage.getResProguardPool().getStringIndexMap().get(resEntry.getEntryName()));
                            }
                        }
                    }
                }
            }
            resPackage.refresh();
        }
        resTable.refresh();
    }

    public static boolean replaceResFileName(ResTable resTable, int resId, String srcFileName, String targetFileName) {
        Log.d(TAG, "try to replace resource (%H) file %s with %s", resId, srcFileName, targetFileName);
        ResPackage resPackage = findResPackage(resTable, getPackageId(resId));
        boolean result = false;
        if (resPackage != null) {
            List<ResType> resTypeList = findResType(resPackage, resId);
            for (ResType resType : resTypeList) {
                int entryId = getResourceEntryId(resId);
                ResEntry resEntry = resType.getEntryTable().get(entryId);
                if (resEntry.getResValue().getDataType() == ArscConstants.RES_VALUE_DATA_TYPE_STRING) {
                    String filePath = ResStringBlock.resolveStringPoolEntry(resTable.getGlobalStringPool().getStrings().get(resEntry.getResValue().getData()).array(), resTable.getGlobalStringPool().getCharSet());
                    if (filePath.equals(srcFileName)) {
                        resTable.getGlobalStringPool().getStrings().set(resEntry.getResValue().getData(), ByteBuffer.wrap(ResStringBlock.encodeStringPoolEntry(targetFileName, resTable.getGlobalStringPool().getCharSet())));
                        result = true;
                        break;
                    }
                }
            }
            if (result) {
                resTable.getGlobalStringPool().refresh();
                resTable.refresh();
            } else {
                Log.w(TAG, "srcFile %s not referenced by resource (%H)", srcFileName, resId);
            }
        }
        return result;
    }

}
