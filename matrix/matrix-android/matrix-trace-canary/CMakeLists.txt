cmake_minimum_required(VERSION 3.4.1)

project(TraceCanary)

include_directories(src/main/cpp)

add_library(trace-canary
            SHARED
        src/main/cpp/MatrixTracer.cc
        src/main/cpp/SignalHandler.cc
        src/main/cpp/Support.cc
        src/main/cpp/AnrDumper.cc
        src/main/cpp/nativehelper/managed_jnienv.cc
        src/main/cpp/SafeMaps.cpp
        src/main/cpp/DynamicLoader.cpp
             )

TARGET_INCLUDE_DIRECTORIES(trace-canary PRIVATE ${EXT_DEP}/include)


find_library( log-lib
              log )


target_link_libraries( trace-canary
        PRIVATE ${log-lib}
        PRIVATE ${EXT_DEP}/lib/${ANDROID_ABI}/libxhook.a
                    )

set_target_properties(trace-canary PROPERTIES
        CXX_STANDARD_REQUIRED ON
        CXX_STANDARD 17
        CXX_EXTENSIONS OFF
        )