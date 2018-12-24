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

package com.tencent.matrix.apk;

import com.tencent.matrix.apk.model.job.ApkJob;
import com.tencent.matrix.apk.model.job.JobConstants;

/**
 * Created by jinqiuchen on 17/5/23.
 */

public final class ApkChecker {

    private static final String INTRODUCT =
        "Welcome to use ApkChecker!\n"
            + "ApkChecker can help you do something like:\n"
            + "1. Overview the apk file.\n"
            + "   package name, version code, version name, method count, resources file size, dex file size ...\n"
            + "2. Find out some problems in apk and give suggestions or solutions.\n"
            + "   lack proguard, exceed 65535 methods, abnormal large img file ...\n"
            + "3. Shrink the size of apk.\n"
            + "   non-alpha PNG, large img file, duplicated files ...\n";

    private static final String HELP =
        "Usages:\n\n"
            + JobConstants.PARAM_CONFIG + " CONFIG-FILE-PATH\n\n"
            + "or\n\n"
            + "[" + JobConstants.PARAM_INPUT + " INPUT-DIR-PATH] [" + JobConstants.PARAM_APK + " APK-FILE-PATH] [" + JobConstants.PARAM_UNZIP + " APK-UNZIP-PATH] [" + JobConstants.PARAM_MAPPING_TXT + " MAPPING-FILE-PATH] [" + JobConstants.PARAM_RES_MAPPING_TXT + " RESGUARD-MAPPING-FILE-PATH] [" + JobConstants.PARAM_OUTPUT + " OUTPUT-PATH] [" + JobConstants.PARAM_FORMAT + " OUTPUT-FORMAT] [" + JobConstants.PARAM_FORMAT_JAR + " OUTPUT-FORMAT-JAR] [" + JobConstants.PARAM_FORMAT_CONFIG + " OUTPUT-FORMAT-CONFIG (json-array format)] [Options]\n\n"
            + "Options:\n"
                + JobConstants.OPTION_MANIFEST + "\n"
                + "     Read package info from the AndroidManifest.xml.\n"
                + JobConstants.OPTION_FILE_SIZE + " [" + JobConstants.PARAM_MIN_SIZE_IN_KB + " DOWN-LIMIT-SIZE (KB)] [" + JobConstants.PARAM_ORDER + " ORDER-BY ('" + JobConstants.ORDER_ASC + "'|'" + JobConstants.ORDER_DESC + "')] [" + JobConstants.PARAM_SUFFIX + " FILTER-SUFFIX-LIST (split by ',')]\n"
                + "     Show files whose size exceed limit size in order.\n"
                + JobConstants.OPTION_COUNT_METHOD + " [" + JobConstants.PARAM_GROUP + " GROUP-BY ('" + JobConstants.GROUP_CLASS + "'|'" + JobConstants.GROUP_PACKAGE + "')]\n"
                + "     Count methods in dex file, output results group by class name or package name.\n"
                + JobConstants.OPTION_CHECK_RES_PROGUARD + "\n"
                + "     Check if the resguard was applied.\n"
                + JobConstants.OPTION_FIND_NON_ALPHA_PNG + " [" + JobConstants.PARAM_MIN_SIZE_IN_KB + " DOWN-LIMIT-SIZE (KB)]\n"
                + "     Find out the non-alpha png-format files whose size exceed limit size in desc order.\n"
                + JobConstants.OPTION_CHECK_MULTILIB + "\n"
                + "     Check if there are more than one library dir in the 'lib'.\n"
                + JobConstants.OPTION_UNCOMPRESSED_FILE + " [" + JobConstants.PARAM_SUFFIX + " FILTER-SUFFIX-LIST (split by ',')]\n"
                + "     Show uncompressed file types.\n"
                + JobConstants.OPTION_COUNT_R_CLASS + "\n"
                + "     Count the R class.\n"
                + JobConstants.OPTION_DUPLICATE_RESOURCES + "\n"
                + "     Find out the duplicated resource files in desc order.\n"
                + JobConstants.OPTION_CHECK_MULTISTL + "  " + JobConstants.PARAM_TOOL_NM + " TOOL-NM-PATH\n"
                + "     Check if there are more than one shared library statically linked the STL.\n"
                + JobConstants.OPTION_UNUSED_RESOURCES + " " + JobConstants.PARAM_R_TXT  + " R-TXT-FILE-PATH [" + JobConstants.PARAM_IGNORE_RESOURCES_LIST + " IGNORE-RESOURCES-LIST (split by ',')]\n"
                + "     Find out the unused resources.\n"
                + JobConstants.OPTION_UNUSED_ASSETS  + " [" + JobConstants.PARAM_IGNORE_ASSETS_LIST + " IGNORE-ASSETS-LIST (split by ',')]\n"
                + "     Find out the unused assets file.\n"
                + JobConstants.OPTION_UNSTRIPPED_SO + "  " + JobConstants.PARAM_TOOL_NM + " TOOL-NM-PATH\n"
                + "     Find out the unstripped shared library file.\n"
                + JobConstants.OPTION_COUNT_CLASS + " [" + JobConstants.PARAM_GROUP + " GROUP-BY ('" + JobConstants.GROUP_PACKAGE + "')]\n"
                + "     Count classes in dex file, output results group by package name.";


    private ApkChecker() {
    }

    public static void main(String... args) {
        if (args.length > 0) {
            ApkChecker m = new ApkChecker();
            m.run(args);
        } else {
            System.out.println(INTRODUCT + HELP);
            System.exit(0);
        }
    }

    private void run(String[] args) {
      ApkJob job = new ApkJob(args);
      try {
          job.run();
      } catch (Exception e) {
          e.printStackTrace();
          System.exit(-1);
      }
    }

    public static void printError(String error) {
        System.out.println(error);
        printHelp();
    }

    public static void printHelp() {
        System.out.println(HELP);
        System.exit(-1);
    }

}
