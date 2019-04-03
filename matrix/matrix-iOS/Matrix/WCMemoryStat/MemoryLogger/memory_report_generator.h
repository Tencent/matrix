/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef stack_report_generator_h
#define stack_report_generator_h

#include <stdio.h>
#include <string>
#include <unordered_map>

/** 
 * Data report format (JSON)
 {
	"head": {
        "protocol_ver": 1,				# protocol version, current(version 1), required
		"phone": "oppo",				# string，device type, required
		"os_ver": "android-17",			# Android(api level), iPhone(iOS version), required
 		"launch_time": timestamp,		# launch time, unix timestamp, required
		"report_time": timestamp,		# report time, unix timestamp, required
		"app_uuid": uuid				# app uuid, required
 		// custom field
 		"uin": uin						# uin, optional
	},
	"items": [{
		"tag": "iOS_MemStat",
        "info": "",
		"scene": "WCTimeLine",			# FOOM Scene
		"name": "NSObject",				# class name
		"size": 123456,					# allocated memory size
		"count": 123,					# object count
		"stacks": [{
			"caller": "uuid@offset",	# caller who allocated the memory
			"size": 21313,				# total size
			"count": 123,				# object count
			"frames": [{
				"uuid": "uuid",
				"offset": 123456
			}]
		}]
	}]
 }
Note:
 Each object is allocated by a source (stack).
 This object has its own classification,
 so take the TOP N memory allocation maximum classification,
 and then take the TOP M allocation source (stack) for each classification.
 **/

std::string generate_summary_report(const char *event_dir, std::string phone, std::string os_ver, uint64_t launch_time, uint64_t report_time, std::string app_uuid, std::string foom_scene, std::unordered_map<std::string, std::string> &customInfo);
std::unordered_map<uint64_t, uint64_t> thread_alloc_size(const char *event_dir);

#endif /* stack_report_generator_h */
