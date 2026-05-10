cmake_minimum_required(VERSION 3.16)
find_package(Threads REQUIRED)
add_executable(11_mutex main.c)
target_link_libraries(11_mutex Threads::Threads)
