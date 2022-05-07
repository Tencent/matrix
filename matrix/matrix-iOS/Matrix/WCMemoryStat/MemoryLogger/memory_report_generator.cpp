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

#include "memory_report_generator.h"
#include "stack_frames_db.h"
#include "allocation_event_db.h"
#include "dyld_image_info.h"
#include "object_event_handler.h"
#include "prettywriter.h"
#include "logger_internal.h"

#include <vector>
#include <algorithm>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

struct allocation_stack {
    uint64_t size;
    uint32_t count;
    uint32_t stack_identifier;
    bool is_nsobject;

    template<typename Writer> void Serialize(Writer &writer, stack_frames_db *stack_frames_reader, dyld_image_info_db *dyld_image_info_reader) {
        // Parse the stack first
        uint32_t fcount = 0;
        uint64_t frames[STACK_LOGGING_MAX_STACK_SIZE];
        char const *uuids[STACK_LOGGING_MAX_STACK_SIZE];
        char const *image_names[STACK_LOGGING_MAX_STACK_SIZE];
        bool is_app_image[STACK_LOGGING_MAX_STACK_SIZE];

        if (stack_frames_reader == NULL) {
            return;
        }

        stack_frames_db_unwind_stack(stack_frames_reader, stack_identifier, frames, &fcount, STACK_LOGGING_MAX_STACK_SIZE);
        if (fcount <= 0) {
            return;
        }

        dyld_image_info_db_transform_frames(dyld_image_info_reader, frames, frames, uuids, image_names, is_app_image, fcount);

        // Find Responsible Caller, priority app symbol, if not found, have to find the first stack symbol; skip the main symbol
        int foundIndex = -1;
        // If it is NSObject, skip the method of +[NSObject event_logging_alloc] which hook by me.
        bool shouldSkip = is_nsobject;
        for (int i = 0; i < fcount - 2; ++i) {
            if (is_app_image[i]) {
                if (shouldSkip) {
                    shouldSkip = false;
                } else {
                    foundIndex = i;
                    break;
                }
            }
        }
        // If do not find the WeChat symbol, you have to find the first symbol that is different from the library where the top symbol address is located.
        // filter CoreGraphics、ImageIO、AppleJPEG、Foundation  CoreFoundation？
        if (foundIndex == -1) {
            for (foundIndex = 1; foundIndex < fcount - 1; ++foundIndex) {
                if (strcmp(uuids[foundIndex], uuids[0]) != 0 && strstr(image_names[foundIndex], "CoreGraphics") == NULL
                    && strstr(image_names[foundIndex], "ImageIO") == NULL && strstr(image_names[foundIndex], "AppleJPEG") == NULL
                    && strstr(image_names[foundIndex], "Foundation") == NULL && strstr(image_names[foundIndex], "CoreFoundation") == NULL) {
                    break;
                }
            }
        }

        writer.StartObject();

        writer.String("caller");

        char buff[128] = { 0 };
        sprintf(buff, "%s@%u", uuids[foundIndex], (uint32_t)frames[foundIndex]);
        writer.String(buff);

        writer.String("size");
        writer.Uint64(size);

        writer.String("count");
        writer.Uint64(count);

        writer.String("frames");
        writer.StartArray();
        for (int i = 0; i < fcount; ++i) {
            writer.StartObject();

            writer.String("uuid");
            writer.String(uuids[i]);

            writer.String("offset");
            writer.Uint64(frames[i]);

            writer.EndObject();
        }
        writer.EndArray();

        writer.EndObject();
    }
};

// sort by size, large to small
bool comparison_allocation_stack(allocation_stack *a, allocation_stack *b) {
    return a->size > b->size;
}

struct allocation_category {
    std::string name;
    uint64_t size;
    uint64_t count;
    std::vector<allocation_stack *> stacks;
    std::unordered_map<uint32_t, allocation_stack *> stack_map;

    ~allocation_category() {
        for (auto iter = stacks.begin(); iter != stacks.end(); ++iter) {
            delete *iter;
        }
    }

    template<typename Writer> void Serialize(Writer &writer,
                                             int index,
                                             stack_frames_db *stack_frames_reader,
                                             dyld_image_info_db *dyld_image_info_reader,
                                             const std::string &app_uuid,
                                             const std::string &scene) {
        writer.StartObject();

        writer.String("tag");
        writer.String("iOS_MemStat");

        writer.String("info");
        writer.String("");

        writer.String("scene");
        writer.String(scene.c_str());

        writer.String("name");
        writer.String(name.c_str());

        writer.String("size");
        if (name == "Alloc Fail" || name == "Unknow") {
            writer.Uint64(0);
        } else {
            writer.Uint64(size);
        }

        writer.String("count");
        writer.Uint64(count);

        if (stacks.size() == 0) {
            writer.EndObject();
            return;
        }

        // Take top N or assign a size larger than 1M or UI category
        if (size >= 1024 * 1024 || index < 15) {
            std::sort(stacks.begin(), stacks.end(), comparison_allocation_stack);
            writer.String("stacks");
            writer.StartArray();
            // Take top M or malloc size > 1M stack to be serialized
            for (int i = 0; i < stacks.size(); ++i) {
                allocation_stack *stack = stacks[i];
                if (stack->size >= 128 * 1024 || i < 10) {
                    stack->Serialize(writer, stack_frames_reader, dyld_image_info_reader);
                }
            }
            writer.EndArray();
        } else if (name.find("View") != std::string::npos || name.find("Image") != std::string::npos || name.find("Layer") != std::string::npos
                   || count >= 1000) {
            std::sort(stacks.begin(), stacks.end(), comparison_allocation_stack);
            writer.String("stacks");
            writer.StartArray();
            // Take top M or count>50 stack to be serialized
            for (int i = 0; i < stacks.size(); ++i) {
                allocation_stack *stack = stacks[i];
                if (i < 5 || stack->count >= 50) {
                    stack->Serialize(writer, stack_frames_reader, dyld_image_info_reader);
                }
            }
            writer.EndArray();
        }

        writer.EndObject();
    }
};

// sort by size, large to small
bool comparison_allocation_category(allocation_category *a, allocation_category *b) {
    return a->size > b->size;
}

template<typename Writer> void summary_report_header_serialize(Writer &writer, const summary_report_param &param) {
    writer.StartObject();

    writer.String("protocol_ver");
    writer.Uint(1);

    writer.String("phone");
    writer.String(param.phone.c_str());

    writer.String("os_ver");
    writer.String(param.os_ver.c_str());

    writer.String("launch_time");
    writer.Uint64(param.launch_time);

    writer.String("report_time");
    writer.Uint64(param.report_time);

    writer.String("app_uuid");
    writer.String(param.app_uuid.c_str());

    for (auto iter = param.customInfo.begin(); iter != param.customInfo.end(); ++iter) {
        writer.String(iter->first.c_str());
        writer.String(iter->second.c_str());
    }

    writer.EndObject();
}

std::shared_ptr<std::string> generate_summary_report(const char *event_dir, const summary_report_param &param) {
    allocation_event_db *allocation_event_reader = allocation_event_db_open_or_create(event_dir);
    stack_frames_db *stack_frames_reader = stack_frames_db_open_or_create(event_dir);
    dyld_image_info_db *dyld_image_info_reader = dyld_image_info_db_open_or_create(event_dir);
    object_type_db *object_type_reader = object_type_db_open_or_create(event_dir);

    if (!allocation_event_reader || !dyld_image_info_reader || !object_type_reader) {
        allocation_event_db_close(allocation_event_reader);
        stack_frames_db_close(stack_frames_reader);
        dyld_image_info_db_close(dyld_image_info_reader);
        object_type_db_close(object_type_reader);
        return NULL;
    }

    std::shared_ptr<std::string> report_data =
    generate_summary_report_i(allocation_event_reader, stack_frames_reader, dyld_image_info_reader, object_type_reader, param);

    allocation_event_db_close(allocation_event_reader);
    stack_frames_db_close(stack_frames_reader);
    dyld_image_info_db_close(dyld_image_info_reader);
    object_type_db_close(object_type_reader);

    return report_data;
}

std::shared_ptr<std::string> generate_summary_report_i(allocation_event_db *allocation_event_reader,
                                                       stack_frames_db *stack_frames_reader,
                                                       dyld_image_info_db *dyld_image_info_reader,
                                                       object_type_db *object_type_reader,
                                                       const summary_report_param &param) {
    // Classify alloc events first
    std::unordered_map<uint64_t, allocation_category *> *category_map = new std::unordered_map<uint64_t, allocation_category *>(); // key=object_type
    allocation_event_db_enumerate(allocation_event_reader, ^(const uint64_t &address, const allocation_event &event) {
      uint64_t object_type = event.object_type;
      uint64_t org_address = address; //ORIGINL_ADDRESS_FROM_ADDRESS(event.address);
      // align to 16
      uint32_t event_size = (((event.size + 15) >> 4) << 4);

      // if object_type=0
      // 1. If the assignment fails, it is classified into Alloc Fail
      // 2. If they are from VM allocation, put them in the same class
      // 3. If it is from malloc, it is classified into Malloc {size}
      // 4. Remaining classification to unknown
      if (object_type == 0) {
          if (org_address == 0) {
              object_type = UINT32_MAX;
          } else if (event.alloca_type & memory_logging_type_alloc) {
              object_type = (((uint64_t)1 << 32) | event_size);
          } else if (event.alloca_type & memory_logging_type_vm_allocate) {
              object_type = (((uint64_t)2 << 32));
          } else {
              object_type = (((uint64_t)3 << 32));
          }
      }

      auto iter = category_map->find(object_type);
      if (iter == category_map->end()) {
          allocation_category *new_category = new allocation_category();
          const char *type_name = object_type_db_get_object_name(object_type_reader, event.object_type);
          if (type_name != NULL) {
              if (event.alloca_type & memory_logging_type_vm_allocate) {
                  char vm_type_name[128] = { 0 };
                  snprintf(vm_type_name, sizeof(vm_type_name), "VM: %s", type_name);
                  new_category->name = vm_type_name;
              } else {
                  new_category->name = type_name;
              }
          } else {
              char buff[128] = { 0 };
              if (org_address == 0) {
                  sprintf(buff, "Alloc Fail");
                  new_category->name = buff;
              } else if (event.alloca_type & memory_logging_type_alloc) {
                  if (event_size < 1024) {
                      sprintf(buff, "Malloc %u Bytes", event_size);
                  } else if (event_size < 1024 * 1024) {
                      sprintf(buff, "Malloc %0.02f KiB", event_size / 1024.0);
                  } else {
                      sprintf(buff, "Malloc %0.02f MiB", event_size / (1024.0 * 1024.0));
                  }
                  new_category->name = buff;
              } else if (event.alloca_type & memory_logging_type_vm_allocate) {
                  sprintf(buff, "All Anonymous VM");
                  new_category->name = buff;
              } else {
                  sprintf(buff, "Unknow");
                  new_category->name = buff;
              }
          }
          iter = category_map->insert(std::make_pair(object_type, new_category)).first;
      }

      iter->second->size += event_size;
      iter->second->count++;

      // After finding the object classification, sort by stack type
      if (event.stack_identifier > 0) {
          auto iter2 = iter->second->stack_map.find(event.stack_identifier);
          if (iter2 == iter->second->stack_map.end()) {
              allocation_stack *new_stack = new allocation_stack();
              new_stack->stack_identifier = event.stack_identifier;
              new_stack->is_nsobject = object_type_db_is_nsobject(object_type_reader, event.object_type);
              iter->second->stacks.push_back(new_stack);
              iter2 = iter->second->stack_map.insert(std::make_pair(event.stack_identifier, new_stack)).first;
          }

          iter2->second->size += event_size;
          iter2->second->count++;
      }
    });

    // map to vector
    // If the category has only one kind of stack, try to merge with other categories that contain this stack.
    std::vector<allocation_category *> *category_list = new std::vector<allocation_category *>();
    for (auto iter = category_map->begin(); iter != category_map->end(); ++iter) {
        iter->second->stack_map.clear();

        if (iter->second->stacks.size() == 1) {
            bool found = false;

            for (auto iter2 = category_list->begin(); iter2 != category_list->end() && !found; ++iter2) {
                allocation_category *category = (*iter2);
                for (auto iter3 = category->stacks.begin(); iter3 != category->stacks.end(); ++iter3) {
                    allocation_stack *stack = (*iter3);
                    if (stack->stack_identifier == iter->second->stacks[0]->stack_identifier) {
                        stack->size += iter->second->stacks[0]->size;
                        stack->count += iter->second->stacks[0]->count;
                        category->size += iter->second->stacks[0]->size;
                        category->count += iter->second->stacks[0]->count;
                        found = true;
                        break;
                    }
                }
            }

            if (found) {
                delete iter->second;
            } else {
                category_list->push_back(iter->second);
            }
        } else {
            category_list->push_back(iter->second);
        }
    }

    // clear memory
    delete category_map;

    // sort by size
    std::sort(category_list->begin(), category_list->end(), comparison_allocation_category);

    // serialize to json
    rapidjson::StringBuffer sb;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
    writer.SetIndent(' ', 0);
    writer.SetFormatOptions(rapidjson::kFormatSingleLineArray);

    writer.StartObject();

    writer.Key("head");
    summary_report_header_serialize(writer, param);

    writer.Key("items");
    writer.StartArray();
    // Take top M or malloc size > 1M category
    for (int i = 0; i < category_list->size(); ++i) {
        allocation_category *category = (*category_list)[i];
        category->Serialize(writer, i, stack_frames_reader, dyld_image_info_reader, param.app_uuid, (i == 0 ? param.foom_scene : ""));
    }
    writer.EndArray();

    writer.EndObject();

    // clear memory
    for (auto iter = category_list->begin(); iter != category_list->end(); ++iter) {
        delete *iter;
    }
    delete category_list;

    return std::make_shared<std::string>(sb.GetString());
}
