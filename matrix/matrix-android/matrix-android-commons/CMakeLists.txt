cmake_minimum_required(VERSION 3.4.1)
project(android-commons)

option(EnableLOG "Enable Logs" ON)
if(EnableLOG)
  add_definitions(-DEnableLOG)
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp/libenhance_dlsym)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp/libsemi_dlfcn)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp/libxhook)