cmake_minimum_required(VERSION 3.4.1)
project(libsemi_dlfcn C)

set(semi_dlfcn_source_dir ${CMAKE_CURRENT_SOURCE_DIR})

set(
  semi_dlfcn_source
  ${semi_dlfcn_source_dir}/semi_dlfcn.c
  ${semi_dlfcn_source_dir}/sd_log.c
)

add_library(semi_dlfcn STATIC ${semi_dlfcn_source})

find_library(log-lib log)

target_include_directories(
  semi_dlfcn
  PUBLIC ${semi_dlfcn_source_dir}
)

target_link_libraries(
  semi_dlfcn
  PUBLIC ${log-lib}
)