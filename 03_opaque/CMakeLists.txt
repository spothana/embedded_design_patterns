cmake_minimum_required(VERSION 3.16)
add_executable(03_opaque main.c ring_buffer.c)
target_include_directories(03_opaque PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
