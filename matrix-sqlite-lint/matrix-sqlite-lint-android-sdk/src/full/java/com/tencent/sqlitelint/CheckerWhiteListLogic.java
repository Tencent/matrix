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

package com.tencent.sqlitelint;

import android.content.Context;
import android.content.res.XmlResourceParser;

import com.tencent.sqlitelint.util.SLog;
import com.tencent.sqlitelint.util.SQLiteLintUtil;

import org.xmlpull.v1.XmlPullParserException;

import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

/**
 * @author liyongjie
 *         Created by liyongjie on 16/9/23.
 */

public final class CheckerWhiteListLogic {
    private static final String TAG = "SQLiteLint.CheckerWhiteListLogic";

    private static final String TAG_CHECKER = "checker";
    private static final String ATTRIBUTE_CHECKER_NAME = "name";
    private static final String TAG_WHITE_LIST_ELEMENT = "element";

    /**
     * @see com.tencent.sqlitelint.config.SQLiteLintConfig.ConcernDb#setWhiteListXml(int)
     *
     * @param context
     * @param concernedDbPath
     * @param xmlResId
     */
    public static void setWhiteList(Context context, final String concernedDbPath, final int xmlResId) {
        XmlResourceParser parser;

        try {
            parser = context.getResources().getXml(xmlResId);
        } catch (Exception e) {
            SLog.w(TAG, "buildWhiteListSet: getResources exp=%s", e.getLocalizedMessage());
            return;
        }

        if (parser == null) {
            SLog.w(TAG, "buildWhiteListSet: parser null");
            return;
        }

        try {
            int protectCnt = 0;
            int eventType = parser.getEventType();
            String enclosedCheckerName = null;
            Map<String, List<String>> whiteListMap = new HashMap<>();

            while (eventType != XmlResourceParser.END_DOCUMENT) {
                switch (eventType) {
                    case XmlResourceParser.START_DOCUMENT:
                        break;
                    case XmlResourceParser.START_TAG:
                        String tagName = parser.getName();

                        if (TAG_CHECKER.equalsIgnoreCase(tagName)) {
                            enclosedCheckerName = parser.getAttributeValue(null, ATTRIBUTE_CHECKER_NAME);
                        }

                        if (TAG_WHITE_LIST_ELEMENT.equalsIgnoreCase(tagName) && !SQLiteLintUtil.isNullOrNil(enclosedCheckerName)) {
                            String text = parser.nextText();
                            if (whiteListMap.get(enclosedCheckerName) == null) {
                                List<String> list = new ArrayList<>();
                                list.add(text);
                                whiteListMap.put(enclosedCheckerName, list);
                            } else {
                                whiteListMap.get(enclosedCheckerName).add(text);
                            }
                            //addToWhiteList(enclosedCheckerName, text);
                            SLog.v(TAG, "buildWhiteListMap: add to whiteList[%s]: %s", enclosedCheckerName, text);
                        }
                        break;
                    case XmlResourceParser.END_TAG:
                        break;
                    default:
                        SLog.w(TAG, "buildWhiteListMap: default branch , eventType:%d", eventType);

                        break;
                }
                parser.next();
                eventType = parser.getEventType();

                if (++protectCnt > 10000) {
                    SLog.e(TAG, "buildWhiteListMap:maybe dead loop!!");
                    break;
                }
            }

            addToNative(concernedDbPath, whiteListMap);
        } catch (XmlPullParserException e) {
            SLog.w(TAG, "buildWhiteListSet: exp=%s", e.getLocalizedMessage());
        } catch (IOException e) {
            SLog.w(TAG, "buildWhiteListSet: exp=%s", e.getLocalizedMessage());
        }

        parser.close();
    }

    private static void addToNative(final String concernedDbPath, Map<String, List<String>> whiteListMap) {
        if (whiteListMap == null) {
            return;
        }

        Iterator<Map.Entry<String, List<String>>> it = whiteListMap.entrySet().iterator();
        Map.Entry<String, List<String>> entry;
        List<String> list;
        String[] checkerArr = new String[whiteListMap.size()];
        String[][] whiteListArr = new String[whiteListMap.size()][];
        int index = 0;
        while (it.hasNext()) {
            entry = it.next();

            checkerArr[index] = entry.getKey();
            list = entry.getValue();
            whiteListArr[index] = new String[list.size()];
            for (int i = 0; i < list.size(); i++) {
                whiteListArr[index][i] = list.get(i);
            }

            index++;
        }

        SQLiteLintNativeBridge.nativeAddToWhiteList(concernedDbPath, checkerArr, whiteListArr);
    }
}
