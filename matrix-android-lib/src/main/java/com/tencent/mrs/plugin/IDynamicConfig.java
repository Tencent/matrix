package com.tencent.mrs.plugin;




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