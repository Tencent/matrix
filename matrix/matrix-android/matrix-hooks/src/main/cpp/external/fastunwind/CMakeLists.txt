cmake_minimum_required(VERSION 3.6)

add_library(
  fastunwind
  STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/FastUnwind.cpp
)

target_compile_options(
  fastunwind
  PRIVATE -Wall -Wextra -Werror -fvisibility=hidden
)

target_include_directories(
  fastunwind
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)