package com.tencent.matrix.apk.model.task.util;

import com.tencent.matrix.javalib.util.Log;
import com.tencent.matrix.javalib.util.Util;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

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

    private static final Pattern RESOURCE_ID_PATTERN = Pattern.compile("^(\\S+\\.)*R\\.(\\S+?)\\.(\\S+)");

    private static String parseResourceNameFromResguard(String resName) {
        if (Util.isNullOrNil(resName)) return "";
        final Matcher matcher = RESOURCE_ID_PATTERN.matcher(resName);
        if (matcher.find()) {
            final StringBuilder builder = new StringBuilder();
            builder.append("R.");
            builder.append(matcher.group(2));
            /*
                The resource ID from resguard is read from ARSC file, which format is package-like
                (for example: R.style.Theme.AppCompat.Light.DarkActionBar). We should convert it as the regular format
                in code (for example: R.style.Theme_AppCompat_Light_DarkActionBar).
             */
            builder.append('.');
            builder.append(matcher.group(3).replace('.', '_'));
            return builder.toString();
        } else {
            return "";
        }
    }
}
