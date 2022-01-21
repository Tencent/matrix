cmake_minimum_required(VERSION 3.4.1)

project(MatrixTraffic)

include_directories(src/main/cpp)

add_library(matrix-traffic
            SHARED
            src/main/cpp/MatrixTraffic.cc
            src/main/cpp/TrafficCollector.cc
            src/main/cpp/util/blocking_queue.h
            src/main/cpp/util/managed_jnienv.cc
             )


target_include_directories(
        matrix-traffic
        PRIVATE ${EXT_DEP}/include
        PRIVATE ${EXT_DEP}/include/backtrace
        PRIVATE ${EXT_DEP}/include/backtrace/common
)


TARGET_INCLUDE_DIRECTORIES(matrix-traffic PRIVATE ${EXT_DEP}/include)


find_library( log-lib
              log )


target_link_libraries(matrix-traffic
        PRIVATE ${log-lib}
        PRIVATE ${EXT_DEP}/lib/${ANDROID_ABI}/libxhook.a
        PRIVATE ${EXT_DEP}/lib/${ANDROID_ABI}/libwechatbacktrace.so
                    )

set_target_properties(matrix-traffic PROPERTIES
        CXX_STANDARD_REQUIRED ON
        CXX_STANDARD 17
        CXX_EXTENSIONS OFF
        )