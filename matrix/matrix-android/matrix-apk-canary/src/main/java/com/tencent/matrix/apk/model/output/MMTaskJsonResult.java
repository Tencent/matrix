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


import com.android.utils.Pair;
import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.tencent.matrix.apk.model.result.TaskJsonResult;
import com.tencent.matrix.apk.model.task.TaskFactory;
import com.tencent.matrix.javalib.util.Util;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import javax.xml.parsers.ParserConfigurationException;

/**
 * Created by jinqiuchen on 17/8/15.
 */

public class MMTaskJsonResult extends TaskJsonResult {

    public MMTaskJsonResult(int type, JsonObject config) throws ParserConfigurationException {
        super(type, config);
    }

    @Override
    public void format(JsonObject jsonObject) {
        formatJson(jsonObject, this.jsonObject, config);
    }

    public static void formatJson(JsonObject jsonObjectInput, JsonObject jsonObjectOutput, JsonObject config) {
        int taskType = jsonObjectInput.get("taskType").getAsInt();
        switch (taskType) {
            case TaskFactory.TASK_TYPE_UNZIP:
                formatUnzipTask(jsonObjectInput);
                break;
            case TaskFactory.TASK_TYPE_MANIFEST:
                formatManifestAnalyzeTask(jsonObjectInput);
                break;
            case TaskFactory.TASK_TYPE_COUNT_METHOD:
                formatMethodCountTask(jsonObjectInput, config);
                break;
            case TaskFactory.TASK_TYPE_COUNT_R_CLASS:
                formatCountR(jsonObjectInput);
                break;
            /*case TaskFactory.TASK_TYPE_UNUSED_RESOURCES:
                formatUnusedResourcesTask(jsonObjectInput);
                break;*/
            case TaskFactory.TASK_TYPE_COUNT_CLASS:
                formatCountClass(jsonObjectInput, config);
                break;
            default :
                break;
        }
        if (jsonObjectOutput != null) {
            for (Map.Entry<String, JsonElement> entry : jsonObjectInput.entrySet()) {
                if (!jsonObjectOutput.has(entry.getKey())) {
                    jsonObjectOutput.add(entry.getKey(), entry.getValue());
                }
            }
        }
    }

    private static void formatCountR(JsonObject jsonObject) {
        JsonArray files = jsonObject.getAsJsonArray("R-classes");
        final HashMap<String, Integer> rMaps = new HashMap<>();
        for (JsonElement file : files) {
            JsonObject object = (JsonObject) file;
            rMaps.put(object.get("name").getAsString(), object.get("field-count").getAsInt());
        }

        ArrayList<String> keys = new ArrayList<>(rMaps.keySet());
        Collections.sort(keys, new Comparator<String>() {
            @Override
            public int compare(String left, String right) {
                int pair1 = rMaps.get(left);
                int pair2 = rMaps.get(right);

                if (pair1 > pair2) {
                    return -1;
                } else if (pair1 < pair2) {
                    return 1;
                } else {
                    return 0;
                }
            }
        });
        jsonObject.remove("R-classes");

        JsonArray groupArray = new JsonArray();
        for (String name : keys) {
            JsonObject groupObj = new JsonObject();
            groupObj.addProperty("name", name);
            groupObj.addProperty("field-count", rMaps.get(name));
            groupArray.add(groupObj);
        }
        jsonObject.add("R-classes", groupArray);
    }

    private static void formatUnzipTask(JsonObject jsonObject) {
        Map<String, Long> fileGroupMap = new HashMap<>();
        Map<String, JsonArray> fileListGroup = new HashMap<>();
        long otherFilesSize = 0;
        JsonArray otherFiles = new JsonArray();
        JsonArray files = jsonObject.getAsJsonArray("entries");
        for (JsonElement file : files) {
            final String filename = ((JsonObject) file).get("entry-name").getAsString();
            if (!Util.isNullOrNil(filename)) {
                int index = filename.lastIndexOf('.');
                if (index >= 0) {
                    final String suffix = filename.substring(index, filename.length());
                    if (!fileGroupMap.containsKey(suffix)) {
                        fileGroupMap.put(suffix, ((JsonObject) file).get("entry-size").getAsLong());
                        JsonArray fileList = new JsonArray();
                        fileList.add(file);
                        fileListGroup.put(suffix, fileList);
                    } else {
                        fileGroupMap.put(suffix, fileGroupMap.get(suffix) + ((JsonObject) file).get("entry-size").getAsLong());
                        fileListGroup.get(suffix).add(file);
                    }
                } else {
                    otherFilesSize += ((JsonObject) file).get("entry-size").getAsLong();
                    otherFiles.add(file);
                }
            }
        }

        List<Pair<String, Long>> fileGroupList = new ArrayList<>();
        for (Map.Entry<String, Long> entry : fileGroupMap.entrySet()) {
            fileGroupList.add(Pair.of(entry.getKey(), entry.getValue()));
        }

        Collections.sort(fileGroupList, new Comparator<Pair<String, Long>>() {
            @Override
            public int compare(Pair<String, Long> pair1, Pair<String, Long> pair2) {
                if (pair1.getSecond() > pair2.getSecond()) {
                    return -1;
                } else if (pair1.getSecond() < pair2.getSecond()) {
                    return 1;
                } else {
                    return 0;
                }
            }
        });

        JsonArray items = new JsonArray();
        for (Pair<String, Long> pair : fileGroupList) {
            JsonObject jsonObj = new JsonObject();
            jsonObj.addProperty("suffix", pair.getFirst());
            jsonObj.addProperty("total-size", pair.getSecond());
            jsonObj.add("files", fileListGroup.get(pair.getFirst()));
            items.add(jsonObj);
        }
        JsonObject jsonObj = new JsonObject();
        jsonObj.addProperty("suffix", "others");
        jsonObj.addProperty("total-size", otherFilesSize);
        jsonObj.add("files", otherFiles);
        jsonObject.remove("entries");
        jsonObject.add("entries", items);
    }


    private static void formatManifestAnalyzeTask(JsonObject jsonObject) {

        JsonObject manifest = jsonObject.getAsJsonObject("manifest");

        Map<String, String>  attribute = new HashMap<>();

        if (manifest.has("package")) {
            attribute.put("package", manifest.get("package").getAsString());
        }

        if (manifest.has("android:versionCode")) {
            attribute.put("android:versionCode", manifest.get("android:versionCode").getAsString());
        }

        if (manifest.has("android:versionName")) {
            attribute.put("android:versionName", manifest.get("android:versionName").getAsString());
        }

        if (manifest.has("uses-sdk")) {
            JsonArray sdks = manifest.getAsJsonArray("uses-sdk");
            if (sdks.size() > 0) {
                JsonObject sdk = sdks.get(0).getAsJsonObject();
                if (sdk.has("android:minSdkVersion")) {
                    attribute.put("android:minSdkVersion", sdk.get("android:minSdkVersion").getAsString());
                }
                if (sdk.has("android:targetSdkVersion")) {
                    attribute.put("android:targetSdkVersion", sdk.get("android:targetSdkVersion").getAsString());
                }
            }
        }

        if (manifest.has("application")) {
            JsonArray applications = manifest.getAsJsonArray("application");
            if (applications.size() > 0) {
                JsonObject application = applications.get(0).getAsJsonObject();
                if (application.has("meta-data")) {
                    JsonArray metaDatas = application.getAsJsonArray("meta-data");
                    for (JsonElement metaData : metaDatas) {
                        JsonObject obj = metaData.getAsJsonObject();
                        if (obj.has("android:name") && obj.has("android:value")) {
                            String name = obj.get("android:name").getAsString();
                            String value = obj.get("android:value").getAsString();

                            if ("com.tencent.mm.BuildInfo.CLIENT_VERSION".equals(name)) {
                                attribute.put("CLIENT_VERSION", value);
                            } else if ("com.tencent.mm.BuildInfo.BUILD_TAG".equals(name)) {
                                attribute.put("BUILD_TAG", value);
                            } else if ("com.tencent.mm.BuildInfo.BUILD_SVNPATH".equals(name)) {
                                attribute.put("BUILD_SVNPATH", value);
                            } else if ("com.tencent.mm.BuildInfo.BUILD_REV".equals(name)) {
                                attribute.put("BUILD_REV", value);
                            }
                        }
                    }
                }
            }
        }

        jsonObject.remove("manifest");

        JsonObject jsonObj = new JsonObject();

        for (String key : attribute.keySet()) {
            jsonObj.addProperty(key, attribute.get(key));
        }

        jsonObject.add("manifest", jsonObj);

    }

    private static void formatMethodCountTask(JsonObject jsonObject, JsonObject config) {
        JsonArray groups = null;
        if (config != null) {
            groups = config.getAsJsonArray("group");
        }
        //entries
        Map<String, Integer>  defMethodMap = new HashMap<>();
        Map<String, Integer>  refMethodMap = new HashMap<>();

        JsonArray dexFiles = jsonObject.getAsJsonArray("dex-files");
        for (JsonElement entry : dexFiles) {
            JsonObject dexFile = entry.getAsJsonObject();

            JsonArray defGroups = null;
            if (dexFile.has("internal-packages")) {
                defGroups = dexFile.getAsJsonArray("internal-packages");
            } else if (dexFile.has("internal-classes")) {
                defGroups = dexFile.getAsJsonArray("internal-classes");
            }
            if (defGroups != null) {
                for (JsonElement group : defGroups) {
                    JsonObject groupObj = group.getAsJsonObject();
                    String name = groupObj.get("name").getAsString();
                    defMethodMap.put(name, groupObj.get("methods").getAsInt());
                    if (!refMethodMap.containsKey(name)) {
                        refMethodMap.put(name, 0);
                    }
                }
            }
            JsonArray refGroups = null;
            if (dexFile.has("external-packages")) {
                refGroups = dexFile.getAsJsonArray("external-packages");
            } else if (dexFile.has("external-classes")) {
                refGroups = dexFile.getAsJsonArray("external-classes");
            }
            if (refGroups != null) {
                for (JsonElement group : refGroups) {
                    JsonObject groupObj = group.getAsJsonObject();
                    String name = groupObj.get("name").getAsString();
                    if (!refMethodMap.containsKey(name)) {
                        refMethodMap.put(name, groupObj.get("methods").getAsInt());
                    } else {
                        refMethodMap.put(name, refMethodMap.get(name) + groupObj.get("methods").getAsInt());
                    }
                    if (!defMethodMap.containsKey(name)) {
                        defMethodMap.put(name, 0);
                    }
                }
            }
        }

        final Map<String, Pair<Integer, Integer>> groupMap = new HashMap<>();
        if (groups != null) {
            Map<String, String> groupPattern = new HashMap<>();
            for (JsonElement group : groups) {
                JsonObject obj = group.getAsJsonObject();
                groupPattern.put(obj.get("name").getAsString(), obj.get("package").getAsString());
            }
            Map<String, Integer> groupDefMap = new HashMap<>();
            Map<String, Integer> groupRefMap = new HashMap<>();
            groupDefMap.put("[others]", 0);
            groupRefMap.put("[others]", 0);
            for (String pkg : defMethodMap.keySet()) {
                boolean other = true;
                for (String key : groupPattern.keySet()) {
                    String groupValue = groupPattern.get(key);
                    String groupName = key;
                    int index = groupValue.indexOf('$');
                    if (index >= 0) {
                        groupValue = groupValue.substring(0, index);
                    }
                    if (pkg.startsWith(groupValue)) {
                        if (index >= 0) {
                            groupName = pkg.substring(index);
                            int nextIndex = groupName.indexOf('.');
                            if (nextIndex >= 0) {
                                groupName = groupName.substring(0, nextIndex);
                            }
                            groupName = key.replace("$", groupName);
                        }
                        if (!groupDefMap.containsKey(groupName)) {
                            groupDefMap.put(groupName, defMethodMap.get(pkg));
                        } else {
                            groupDefMap.put(groupName, groupDefMap.get(groupName) + defMethodMap.get(pkg));
                        }
                        other = false;
                    }
                }
                if (other) {
                    groupDefMap.put("[others]", groupDefMap.get("[others]") + defMethodMap.get(pkg));
                }
            }

            for (String pkg : refMethodMap.keySet()) {
                boolean other = true;
                for (String key : groupPattern.keySet()) {
                    String groupValue = groupPattern.get(key);
                    String groupName = key;
                    int index = groupValue.indexOf('$');
                    if (index >= 0) {
                        groupValue = groupValue.substring(0, index);
                    }
                    if (pkg.startsWith(groupValue)) {
                        if (index >= 0) {
                            groupName = pkg.substring(index);
                            int nextIndex = groupName.indexOf('.');
                            if (nextIndex >= 0) {
                                groupName = groupName.substring(0, nextIndex);
                            }
                            groupName = key.replace("$", groupName);
                        }
                        if (!groupRefMap.containsKey(groupName)) {
                            groupRefMap.put(groupName, refMethodMap.get(pkg));
                        } else {
                            groupRefMap.put(groupName, groupRefMap.get(groupName) + refMethodMap.get(pkg));
                        }
                        other = false;
                    }
                }
                if (other) {
                    groupRefMap.put("[others]", groupRefMap.get("[others]") + refMethodMap.get(pkg));
                }
            }

            for (String pkg : groupDefMap.keySet()) {
                groupMap.put(pkg, Pair.of(groupDefMap.get(pkg), groupRefMap.get(pkg)));
            }

        } else {
            for (String pkg : defMethodMap.keySet()) {
                groupMap.put(pkg, Pair.of(defMethodMap.get(pkg), refMethodMap.get(pkg)));
            }
        }
        ArrayList<String> keys = new ArrayList<>(groupMap.keySet());
        Collections.sort(keys, new Comparator<String>() {
            @Override
            public int compare(String left, String right) {
                Pair<Integer, Integer> pair1 = groupMap.get(left);
                Pair<Integer, Integer> pair2 = groupMap.get(right);
                int total1 = pair1.getFirst() + pair1.getSecond();
                int total2 = pair2.getFirst() + pair2.getSecond();

                if (total1 > total2) {
                    return -1;
                } else if (total1 < total2) {
                    return 1;
                } else {
                    return 0;
                }
            }
        });

        jsonObject.remove("dex-files");

        long totalMethods = 0;
        JsonArray groupArray = new JsonArray();
        for (String group : keys) {
            JsonObject groupObj = new JsonObject();
            groupObj.addProperty("name", group);
            Pair<Integer, Integer> pair = groupMap.get(group);
            totalMethods += pair.getFirst() + pair.getSecond();
            groupObj.addProperty("method-count", pair.getFirst() + pair.getSecond());
            groupArray.add(groupObj);
        }

        jsonObject.addProperty("total-methods", totalMethods);
        jsonObject.add("groups", groupArray);
    }


    private static void formatUnusedResourcesTask(JsonObject jsonObject) {
        JsonArray resources = jsonObject.getAsJsonArray("unused-resources");
        Map<String, List<String>> group = new HashMap<String, List<String>>();
        jsonObject.addProperty("total-count", resources.size());
        for (JsonElement resource : resources) {
            String res = resource.getAsString();
            String type = res.substring(0, res.indexOf('.', 2));
            if (!group.containsKey(type)) {
                group.put(type, new ArrayList<String>());
            }
            group.get(type).add(res);
        }
        jsonObject.remove("unused-resources");
        for (Map.Entry<String, List<String>> entry : group.entrySet()) {
            JsonArray list = new JsonArray();
            for (String res : entry.getValue()) {
                list.add(res);
            }
            jsonObject.add(entry.getKey(), list);
        }
    }

    private static void formatCountClass(JsonObject jsonObject, JsonObject config) {
        JsonArray groups = null;
        if (config != null) {
            groups = config.getAsJsonArray("group");
        }

        JsonArray dexFiles = jsonObject.getAsJsonArray("dex-files");

        Map<String, Set<String>> pkgMap = new HashMap<>();

        for (JsonElement entry : dexFiles) {
            JsonObject dexFile = entry.getAsJsonObject();
            JsonArray pkgs = dexFile.get("packages").getAsJsonArray();
            for (JsonElement pkg : pkgs) {
                JsonObject pkgObj = pkg.getAsJsonObject();
                String pkgName = pkgObj.get("package").getAsString();
                if (!pkgMap.containsKey(pkgName)) {
                    pkgMap.put(pkgName, new HashSet<String>());
                }
                JsonArray classes = pkgObj.getAsJsonArray("classes");
                for (JsonElement clazz : classes) {
                    pkgMap.get(pkgName).add(clazz.getAsString());
                }
            }
        }

        final Map<String, Integer> groupDefMap = new HashMap<>();
        if (groups != null) {
            Map<String, String> groupPattern = new HashMap<>();
            for (JsonElement group : groups) {
                JsonObject obj = group.getAsJsonObject();
                groupPattern.put(obj.get("name").getAsString(), obj.get("package").getAsString());
            }
            groupDefMap.put("[others]", 0);
            for (String pkg : pkgMap.keySet()) {
                boolean other = true;
                for (String key : groupPattern.keySet()) {
                    String groupValue = groupPattern.get(key);
                    String groupName = key;
                    int index = groupValue.indexOf('$');
                    if (index >= 0) {
                        groupValue = groupValue.substring(0, index);
                    }
                    if (pkg.startsWith(groupValue)) {
                        if (index >= 0) {
                            groupName = pkg.substring(index);
                            int nextIndex = groupName.indexOf('.');
                            if (nextIndex >= 0) {
                                groupName = groupName.substring(0, nextIndex);
                            }
                            groupName = key.replace("$", groupName);
                        }
                        if (!groupDefMap.containsKey(groupName)) {
                            groupDefMap.put(groupName, pkgMap.get(pkg).size());
                        } else {
                            groupDefMap.put(groupName, groupDefMap.get(groupName) + pkgMap.get(pkg).size());
                        }
                        other = false;
                    }
                }
                if (other) {
                    groupDefMap.put("[others]", groupDefMap.get("[others]") + pkgMap.get(pkg).size());
                }
            }

        } else {
            for (String pkg : pkgMap.keySet()) {
                groupDefMap.put(pkg, pkgMap.get(pkg).size());
            }
        }
        ArrayList<String> keys = new ArrayList<>(groupDefMap.keySet());
        Collections.sort(keys, new Comparator<String>() {
            @Override
            public int compare(String left, String right) {
                int total1 = groupDefMap.get(left);
                int total2 = groupDefMap.get(right);

                if (total1 > total2) {
                    return -1;
                } else if (total1 < total2) {
                    return 1;
                } else {
                    return 0;
                }
            }
        });

        jsonObject.remove("dex-files");

        long totalClasses = 0;
        JsonArray groupArray = new JsonArray();
        for (String group : keys) {
            JsonObject groupObj = new JsonObject();
            groupObj.addProperty("name", group);
            totalClasses += groupDefMap.get(group);
            groupObj.addProperty("class-count", groupDefMap.get(group));
            groupArray.add(groupObj);
        }

        jsonObject.addProperty("total-classes", totalClasses);
        jsonObject.add("groups", groupArray);
    }

}
