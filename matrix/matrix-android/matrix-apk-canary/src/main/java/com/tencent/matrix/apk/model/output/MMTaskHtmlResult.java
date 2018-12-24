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

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.tencent.matrix.apk.model.result.TaskHtmlResult;
import com.tencent.matrix.apk.model.task.TaskFactory;
import com.tencent.matrix.javalib.util.Util;
import org.w3c.dom.Element;

import java.util.Map;

import javax.xml.parsers.ParserConfigurationException;

/**
 * Created by jinqiuchen on 17/8/4.
 */

public class MMTaskHtmlResult extends TaskHtmlResult {

    private static final int MAX_SHOW_ITEMS = 10;
    private static final String FOLDER_STYLE = "color:white;font-size:20px;background-color:#C0C0C0";
    private static final String FOLDER_ONCLICK = "root = this.parentNode.parentNode; "
                                                + "next = root.nextSibling; "
                                                + "while (next != null) { "
                                                +    "if (next.hasAttribute('hidden')) { "
                                                +        "next.removeAttribute('hidden');"
                                                +   "} else { "
                                                +        "break; "
                                                +    "} "
                                                +    "next = next.nextSibling; "
                                                + "} "
                                                + "root.parentNode.removeChild(root)";

    public MMTaskHtmlResult(int type, JsonObject config) throws ParserConfigurationException {
        super(type, config);
    }

    @Override
    public void format(JsonObject jsonObject) throws ParserConfigurationException {
        MMTaskJsonResult.formatJson(jsonObject, null, config);

        int taskType = jsonObject.get("taskType").getAsInt();

        switch (taskType) {

            case TaskFactory.TASK_TYPE_UNZIP: {
                Element element = formatUnzipTask(jsonObject);
                foldElement(element);
                document.appendChild(element);
                break;
            }
            case TaskFactory.TASK_TYPE_MANIFEST: {
                Element element = formatManifestAnalyzeTask(jsonObject);
                foldElement(element);
                document.appendChild(element);
                break;
            }
            case TaskFactory.TASK_TYPE_SHOW_FILE_SIZE: {
                Element element = formatShowFileSizeTask(jsonObject);
                foldElement(element);
                document.appendChild(element);
                break;
            }
            case TaskFactory.TASK_TYPE_COUNT_METHOD: {
                Element element = formatMethodCountTask(jsonObject);
                foldElement(element);
                document.appendChild(element);
                break;
            }
            case TaskFactory.TASK_TYPE_FIND_NON_ALPHA_PNG: {
                Element element = formatFindNonAlphaPngTask(jsonObject);
                foldElement(element);
                document.appendChild(element);
                break;
            }
            case TaskFactory.TASK_TYPE_UNCOMPRESSED_FILE: {
                Element element = formatUncompressedFileTask(jsonObject);
                foldElement(element);
                document.appendChild(element);
                break;
            }
            default:
                jsonObject.remove("taskType");
                jsonObject.remove("start-time");
                jsonObject.remove("end-time");
                super.format(jsonObject);
                foldElement((Element) document.getFirstChild());
                break;
        }

    }

    private void foldElement(Element element) {
        if (element != null && element.getChildNodes().getLength() > MAX_SHOW_ITEMS) {
            for (int i = MAX_SHOW_ITEMS; i < element.getChildNodes().getLength(); i++) {
                if (element.getChildNodes().item(i) instanceof Element) {
                    ((Element) element.getChildNodes().item(i)).setAttribute("hidden", "true");
                }
            }
            Element span = document.createElement("span");
            span.setAttribute("style", FOLDER_STYLE);
            span.setAttribute("onClick", FOLDER_ONCLICK);
            span.setTextContent("...");

            Element folder = null;
            if (element.getTagName().equals("table")) {
                folder = document.createElement("tr");
                Element td = document.createElement("td");
                td.setAttribute("colspan", "100%");
                folder.appendChild(td);
                td.appendChild(span);
            } else if (element.getTagName().equals("ul")) {
                folder = document.createElement("li");
                Element parent = document.createElement("span");
                parent.appendChild(span);
                folder.appendChild(parent);
            }
            if (folder != null) {
                element.insertBefore(folder, element.getChildNodes().item(MAX_SHOW_ITEMS));
            }
        }
        for (int i = 0; i < element.getChildNodes().getLength(); i++) {
            if (element.getChildNodes().item(i) instanceof Element) {
                foldElement((Element) element.getChildNodes().item(i));
            }
        }
    }


    private Element formatUnzipTask(JsonObject jsonObject) {
        Element table = document.createElement("table");
        table.setAttribute("border", "1");
        table.setAttribute("width", "100%");
        if (jsonObject == null) {
            return table;
        }

        /*{
            //taskType
            Element tr = document.createElement("tr");
            Element td1 = document.createElement("td");
            td1.setTextContent("taskType");
            Element td2 = document.createElement("td");
            td2.setTextContent(String.valueOf(jsonObject.get("taskType").getAsInt()));
            tr.appendChild(td1);
            tr.appendChild(td2);
            table.appendChild(tr);
        }*/

        {
            //taskDescription
            Element tr = document.createElement("tr");
            Element td1 = document.createElement("td");
            td1.setTextContent("taskDescription");
            Element td2 = document.createElement("td");
            td2.setTextContent(jsonObject.get("taskDescription").getAsString());
            tr.appendChild(td1);
            tr.appendChild(td2);
            table.appendChild(tr);
        }

        {
            //total-size
            Element tr = document.createElement("tr");
            Element td1 = document.createElement("td");
            td1.setTextContent("total-size");
            Element td2 = document.createElement("td");
            td2.setTextContent(Util.formatByteUnit(jsonObject.get("total-size").getAsLong()));
            tr.appendChild(td1);
            tr.appendChild(td2);
            table.appendChild(tr);
        }

        {
            JsonArray files = jsonObject.getAsJsonArray("entries");
            for (JsonElement file : files) {
                final String suffix = ((JsonObject) file).get("suffix").getAsString();
                final long size = ((JsonObject) file).get("total-size").getAsLong();

                Element tr = document.createElement("tr");
                Element td1 = document.createElement("td");
                td1.setTextContent(suffix);
                Element td2 = document.createElement("td");
                td2.setTextContent(Util.formatByteUnit(size));
                tr.appendChild(td1);
                tr.appendChild(td2);
                table.appendChild(tr);
            }
        }

       /* {
            //start-time
            Element tr = document.createElement("tr");
            Element td1 = document.createElement("td");
            td1.setTextContent("start-time");
            Element td2 = document.createElement("td");
            td2.setTextContent(jsonObject.get("start-time").getAsString());
            tr.appendChild(td1);
            tr.appendChild(td2);
            table.appendChild(tr);
        }

        {
            //end-time
            Element tr = document.createElement("tr");
            Element td1 = document.createElement("td");
            td1.setTextContent("end-time");
            Element td2 = document.createElement("td");
            td2.setTextContent(jsonObject.get("end-time").getAsString());
            tr.appendChild(td1);
            tr.appendChild(td2);
            table.appendChild(tr);
        }*/

        return table;
    }

    private Element formatManifestAnalyzeTask(JsonObject jsonObject) {
        Element table = document.createElement("table");
        table.setAttribute("border", "1");
        table.setAttribute("width", "100%");
        if (jsonObject == null) {
            return table;
        }

        /*{
            //taskType
            Element tr = document.createElement("tr");
            Element td1 = document.createElement("td");
            td1.setTextContent("taskType");
            Element td2 = document.createElement("td");
            td2.setTextContent(String.valueOf(jsonObject.get("taskType").getAsInt()));
            tr.appendChild(td1);
            tr.appendChild(td2);
            table.appendChild(tr);
        }*/

        {
            //taskDescription
            Element tr = document.createElement("tr");
            Element td1 = document.createElement("td");
            td1.setTextContent("taskDescription");
            Element td2 = document.createElement("td");
            td2.setTextContent(jsonObject.get("taskDescription").getAsString());
            tr.appendChild(td1);
            tr.appendChild(td2);
            table.appendChild(tr);
        }

        {
            //entries
            JsonObject manifest = jsonObject.getAsJsonObject("manifest");
            for (Map.Entry<String, JsonElement> entry : manifest.entrySet()) {
                Element tr = document.createElement("tr");
                Element td1 = document.createElement("td");
                td1.setTextContent(entry.getKey());
                Element td2 = document.createElement("td");
                td2.setTextContent(entry.getValue().getAsString());
                tr.appendChild(td1);
                tr.appendChild(td2);
                table.appendChild(tr);
            }
        }

        /*{
            //start-time
            Element tr = document.createElement("tr");
            Element td1 = document.createElement("td");
            td1.setTextContent("start-time");
            Element td2 = document.createElement("td");
            td2.setTextContent(jsonObject.get("start-time").getAsString());
            tr.appendChild(td1);
            tr.appendChild(td2);
            table.appendChild(tr);
        }

        {
            //end-time
            Element tr = document.createElement("tr");
            Element td1 = document.createElement("td");
            td1.setTextContent("end-time");
            Element td2 = document.createElement("td");
            td2.setTextContent(jsonObject.get("end-time").getAsString());
            tr.appendChild(td1);
            tr.appendChild(td2);
            table.appendChild(tr);
        }*/

        return table;
    }

    private Element formatShowFileSizeTask(JsonObject jsonObject) {
        Element table = document.createElement("table");
        table.setAttribute("border", "1");
        table.setAttribute("width", "100%");
        if (jsonObject == null) {
            return table;
        }

        /*{
            //taskType
            Element tr = document.createElement("tr");
            Element td1 = document.createElement("td");
            td1.setTextContent("taskType");
            Element td2 = document.createElement("td");
            td2.setTextContent(String.valueOf(jsonObject.get("taskType").getAsInt()));
            tr.appendChild(td1);
            tr.appendChild(td2);
            table.appendChild(tr);
        }*/

        {
            //taskDescription
            Element tr = document.createElement("tr");
            Element td1 = document.createElement("td");
            td1.setTextContent("taskDescription");
            Element td2 = document.createElement("td");
            td2.setTextContent(jsonObject.get("taskDescription").getAsString());
            tr.appendChild(td1);
            tr.appendChild(td2);
            table.appendChild(tr);
        }

        {
            //files
            JsonArray files = jsonObject.getAsJsonArray("files");
            for (JsonElement file : files) {
                final String filename = ((JsonObject) file).get("entry-name").getAsString();
                if (!Util.isNullOrNil(filename)) {
                    Element tr = document.createElement("tr");
                    Element td1 = document.createElement("td");
                    td1.setTextContent(filename);
                    Element td2 = document.createElement("td");
                    td2.setTextContent(Util.formatByteUnit(((JsonObject) file).get("entry-size").getAsLong()));
                    tr.appendChild(td1);
                    tr.appendChild(td2);
                    table.appendChild(tr);
                }
            }

        }

        /*{
            //start-time
            Element tr = document.createElement("tr");
            Element td1 = document.createElement("td");
            td1.setTextContent("start-time");
            Element td2 = document.createElement("td");
            td2.setTextContent(jsonObject.get("start-time").getAsString());
            tr.appendChild(td1);
            tr.appendChild(td2);
            table.appendChild(tr);
        }

        {
            //end-time
            Element tr = document.createElement("tr");
            Element td1 = document.createElement("td");
            td1.setTextContent("end-time");
            Element td2 = document.createElement("td");
            td2.setTextContent(jsonObject.get("end-time").getAsString());
            tr.appendChild(td1);
            tr.appendChild(td2);
            table.appendChild(tr);
        }*/

        return table;
    }

    private Element formatMethodCountTask(JsonObject jsonObject) {
        Element table = document.createElement("table");
        table.setAttribute("border", "1");
        table.setAttribute("width", "100%");
        if (jsonObject == null) {
            return table;
        }

        /*{
            //taskType
            Element tr = document.createElement("tr");
            Element td1 = document.createElement("td");
            td1.setTextContent("taskType");
            Element td2 = document.createElement("td");
            td2.setTextContent(String.valueOf(jsonObject.get("taskType").getAsInt()));
            tr.appendChild(td1);
            tr.appendChild(td2);
            table.appendChild(tr);
        }*/

        {
            //taskDescription
            Element tr = document.createElement("tr");
            Element td1 = document.createElement("td");
            td1.setTextContent("taskDescription");
            Element td2 = document.createElement("td");
            td2.setTextContent(jsonObject.get("taskDescription").getAsString());
            tr.appendChild(td1);
            tr.appendChild(td2);
            table.appendChild(tr);
        }

        {
            JsonArray groups = jsonObject.getAsJsonArray("groups");


            for (JsonElement entry : groups) {
                JsonObject object = (JsonObject) entry;
                Element tr = document.createElement("tr");
                Element td1 = document.createElement("td");
                td1.setTextContent(object.get("name").getAsString());
                Element td2 = document.createElement("td");
                td2.setTextContent(object.get("method-count").getAsString());
                tr.appendChild(td1);
                tr.appendChild(td2);
                table.appendChild(tr);
            }

            //total
            Element tr = document.createElement("tr");
            Element td1 = document.createElement("td");
            td1.setTextContent("total-methods");
            Element td2 = document.createElement("td");
            td2.setTextContent(jsonObject.get("total-methods").getAsString() + " methods");
            tr.appendChild(td1);
            tr.appendChild(td2);
            table.appendChild(tr);

        }

       /* {
            //start-time
            Element tr = document.createElement("tr");
            Element td1 = document.createElement("td");
            td1.setTextContent("start-time");
            Element td2 = document.createElement("td");
            td2.setTextContent(jsonObject.get("start-time").getAsString());
            tr.appendChild(td1);
            tr.appendChild(td2);
            table.appendChild(tr);
        }

        {
            //end-time
            Element tr = document.createElement("tr");
            Element td1 = document.createElement("td");
            td1.setTextContent("end-time");
            Element td2 = document.createElement("td");
            td2.setTextContent(jsonObject.get("end-time").getAsString());
            tr.appendChild(td1);
            tr.appendChild(td2);
            table.appendChild(tr);
        }*/

        return table;
    }

    private Element formatFindNonAlphaPngTask(JsonObject jsonObject) {
        Element table = document.createElement("table");
        table.setAttribute("border", "1");
        table.setAttribute("width", "100%");
        if (jsonObject == null) {
            return table;
        }

        /*{
            //taskType
            Element tr = document.createElement("tr");
            Element td1 = document.createElement("td");
            td1.setTextContent("taskType");
            Element td2 = document.createElement("td");
            td2.setTextContent(String.valueOf(jsonObject.get("taskType").getAsInt()));
            tr.appendChild(td1);
            tr.appendChild(td2);
            table.appendChild(tr);
        }*/

        {
            //taskDescription
            Element tr = document.createElement("tr");
            Element td1 = document.createElement("td");
            td1.setTextContent("taskDescription");
            Element td2 = document.createElement("td");
            td2.setTextContent(jsonObject.get("taskDescription").getAsString());
            tr.appendChild(td1);
            tr.appendChild(td2);
            table.appendChild(tr);
        }

        {
            //files
            long totalSize = 0;
            JsonArray files = jsonObject.getAsJsonArray("files");
            for (JsonElement file : files) {
                final String filename = ((JsonObject) file).get("entry-name").getAsString();
                if (!Util.isNullOrNil(filename)) {
                    Element tr = document.createElement("tr");
                    Element td1 = document.createElement("td");
                    td1.setTextContent(filename);
                    Element td2 = document.createElement("td");
                    totalSize += ((JsonObject) file).get("entry-size").getAsLong();
                    td2.setTextContent(Util.formatByteUnit(((JsonObject) file).get("entry-size").getAsLong()));
                    tr.appendChild(td1);
                    tr.appendChild(td2);
                    table.appendChild(tr);
                }
            }

            //total
            Element tr = document.createElement("tr");
            Element td1 = document.createElement("td");
            td1.setTextContent("total-size");
            Element td2 = document.createElement("td");
            td2.setTextContent(Util.formatByteUnit(totalSize));
            tr.appendChild(td1);
            tr.appendChild(td2);
            table.appendChild(tr);
        }

       /* {
            //start-time
            Element tr = document.createElement("tr");
            Element td1 = document.createElement("td");
            td1.setTextContent("start-time");
            Element td2 = document.createElement("td");
            td2.setTextContent(jsonObject.get("start-time").getAsString());
            tr.appendChild(td1);
            tr.appendChild(td2);
            table.appendChild(tr);
        }

        {
            //end-time
            Element tr = document.createElement("tr");
            Element td1 = document.createElement("td");
            td1.setTextContent("end-time");
            Element td2 = document.createElement("td");
            td2.setTextContent(jsonObject.get("end-time").getAsString());
            tr.appendChild(td1);
            tr.appendChild(td2);
            table.appendChild(tr);
        }*/

        return table;
    }


    private Element formatUncompressedFileTask(JsonObject jsonObject) {
        Element table = document.createElement("table");
        table.setAttribute("border", "1");
        table.setAttribute("width", "100%");
        if (jsonObject == null) {
            return table;
        }

        /*{
            //taskType
            Element tr = document.createElement("tr");
            Element td1 = document.createElement("td");
            td1.setTextContent("taskType");
            Element td2 = document.createElement("td");
            td2.setTextContent(String.valueOf(jsonObject.get("taskType").getAsInt()));
            tr.appendChild(td1);
            tr.appendChild(td2);
            table.appendChild(tr);
        }*/

        {
            //taskDescription
            Element tr = document.createElement("tr");
            Element td1 = document.createElement("td");
            td1.setTextContent("taskDescription");
            Element td2 = document.createElement("td");
            td2.setTextContent(jsonObject.get("taskDescription").getAsString());
            tr.appendChild(td1);
            tr.appendChild(td2);
            table.appendChild(tr);
        }

        {
            //entries
            JsonArray entries = jsonObject.getAsJsonArray("files");
            for (JsonElement jsonElement : entries) {
                JsonObject jsonObj = jsonElement.getAsJsonObject();
                Element tr = document.createElement("tr");
                Element td1 = document.createElement("td");
                td1.setTextContent(jsonObj.get("suffix").getAsString());
                Element td2 = document.createElement("td");
                td2.setTextContent(Util.formatByteUnit(jsonObj.get("total-size").getAsLong()));
                tr.appendChild(td1);
                tr.appendChild(td2);
                table.appendChild(tr);
            }

        }

       /* {
            //start-time
            Element tr = document.createElement("tr");
            Element td1 = document.createElement("td");
            td1.setTextContent("start-time");
            Element td2 = document.createElement("td");
            td2.setTextContent(jsonObject.get("start-time").getAsString());
            tr.appendChild(td1);
            tr.appendChild(td2);
            table.appendChild(tr);
        }

        {
            //end-time
            Element tr = document.createElement("tr");
            Element td1 = document.createElement("td");
            td1.setTextContent("end-time");
            Element td2 = document.createElement("td");
            td2.setTextContent(jsonObject.get("end-time").getAsString());
            tr.appendChild(td1);
            tr.appendChild(td2);
            table.appendChild(tr);
        }*/

        return table;
    }
}
