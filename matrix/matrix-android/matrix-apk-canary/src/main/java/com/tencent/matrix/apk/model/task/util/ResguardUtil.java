package com.tencent.matrix.apk.model.task.util;

import com.tencent.matrix.javalib.util.Log;
import com.tencent.matrix.javalib.util.Util;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.Map;

public class ResguardUtil {

    private static final String TAG = "Matrix.ResguardUtil";

    public static void readResMappingTxtFile(File resMappingTxt, Map<String, String> resDirMap, Map<String, String> resguardMap) throws IOException {
        if (resMappingTxt != null) {
            BufferedReader bufferedReader = new BufferedReader(new FileReader(resMappingTxt));
            try {
                String line = bufferedReader.readLine();
                boolean readResStart = false;
                boolean readPathStart = false;
                while (line != null) {
                    if (line.trim().equals("res path mapping:")) {
                        readPathStart = true;
                    } else if (line.trim().equals("res id mapping:")) {
                        readResStart = true;
                        readPathStart = false;
                    } else if (readPathStart && resDirMap != null) {
                        String[] columns = line.split("->");
                        if (columns.length == 2) {
                            String before = columns[0].trim();
                            String after = columns[1].trim();
                            if (!Util.isNullOrNil(before) && !Util.isNullOrNil(after)) {
                                Log.d(TAG, "%s->%s", before, after);
                                resDirMap.put(after, before);
                            }
                        }
                    } else if (readResStart && resguardMap != null) {
                        String[] columns = line.split("->");
                        if (columns.length == 2) {
                            String before = parseResourceNameFromResguard(columns[0].trim());
                            String after = parseResourceNameFromResguard(columns[1].trim());
                            if (!Util.isNullOrNil(before) && !Util.isNullOrNil(after)) {
                                Log.d(TAG, "%s->%s", before, after);
                                resguardMap.put(after, before);
                            }
                        }
                    }
                    line = bufferedReader.readLine();
                }
            } finally {
                bufferedReader.close();
            }
        }
    }

    private static String parseResourceNameFromResguard(String resName) {
        if (!Util.isNullOrNil(resName)) {
            int index = resName.indexOf('R');
            if (index >= 0) {
                return resName.substring(index);
            }
        }
        return "";
    }
}
