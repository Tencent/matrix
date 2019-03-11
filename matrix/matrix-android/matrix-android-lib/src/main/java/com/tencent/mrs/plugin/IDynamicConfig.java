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

package com.tencent.mrs.plugin;

/**
 * Created by zhoushaotao on 17/10/15.
 */

public interface IDynamicConfig {

    enum ExptEnum {
        // trace
        clicfg_matrix_trace_fps_enable,
        clicfg_matrix_trace_care_scene_set,
        clicfg_matrix_trace_fps_time_slice,
        clicfg_matrix_trace_load_activity_threshold,
        clicfg_matrix_trace_evil_method_threshold,

        clicfg_matrix_trace_fps_report_threshold,
        clicfg_matrix_trace_max_evil_method_stack_num,
        clicfg_matrix_trace_max_evil_method_try_trim_num,
        clicfg_matrix_trace_min_evil_method_druration,
        clicfg_matrix_trace_time_upload_duration,

        clicfg_matrix_fps_dropped_normal,
        clicfg_matrix_fps_dropped_middle,
        clicfg_matrix_fps_dropped_high,
        clicfg_matrix_fps_dropped_frozen,
        clicfg_matrix_trace_evil_method_enable,
        clicfg_matrix_trace_anr_enable,
        clicfg_matrix_trace_startup_enable,

        clicfg_matrix_trace_app_start_up_threshold,
        clicfg_matrix_trace_warm_app_start_up_threshold,
        clicfg_matrix_trace_fps_frame_fresh_threshold,
        clicfg_matrix_trace_min_evil_method_run_cnt,
        clicfg_matrix_trace_min_evil_method_dur_time,

        clicfg_matrix_trace_splash_activity_name,

        //io
        clicfg_matrix_io_file_io_main_thread_enable,
        clicfg_matrix_io_main_thread_enable_threshold,
        clicfg_matrix_io_small_buffer_enable,
        clicfg_matrix_io_small_buffer_threshold,
        clicfg_matrix_io_small_buffer_operator_times,
        clicfg_matrix_io_repeated_read_enable,
        clicfg_matrix_io_repeated_read_threshold,
        clicfg_matrix_io_closeable_leak_enable,

        //battery
        clicfg_matrix_battery_detect_wake_lock_enable,
        clicfg_matrix_battery_record_wake_lock_enable,
        clicfg_matrix_battery_wake_lock_hold_time_threshold,
        clicfg_matrix_battery_wake_lock_1h_acquire_cnt_threshold,
        clicfg_matrix_battery_wake_lock_1h_hold_time_threshold,
        clicfg_matrix_battery_detect_alarm_enable,
        clicfg_matrix_battery_record_alarm_enable,
        clicfg_matrix_battery_alarm_1h_trigger_cnt_threshold,
        clicfg_matrix_battery_wake_up_alarm_1h_trigger_cnt_threshold,


        //memory
        clicfg_matrix_memory_middle_min_span,
        clicfg_matrix_memory_high_min_span,
        clicfg_matrix_memory_threshold,
        clicfg_matrix_memory_special_activities,

        //resource
        clicfg_matrix_resource_detect_interval_millis,
        clicfg_matrix_resource_max_detect_times,
        clicfg_matrix_resource_dump_hprof_enable,

        //thread
        clicfg_matrix_thread_check_time,
        clicfg_matrix_thread_report_time,
        clicfg_matrix_thread_contain_sys,
        clicfg_matrix_thread_filter_thread_set,

    }

    String get(String key, String defStr);

    int get(String key, int defInt);

    long get(String key, long defLong);

    boolean get(String key, boolean defBool);

    float get(String key, float defFloat);
}