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

package sample.tencent.matrix.config;

/** @author zhoushaotao
*   Created by zhoushaotao on 2018/10/9.
*/

public enum MatrixEnum {

    /*****matrix begin********/
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
    clicfg_matrix_trace_startup_enable,

    clicfg_matrix_trace_app_start_up_threshold,
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

    //resource
    clicfg_matrix_resource_detect_interval_millis,
    clicfg_matrix_resource_max_detect_times,
    clicfg_matrix_resource_dump_hprof_enable,


    /******matrix end*******/
}
