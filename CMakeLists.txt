cmake_minimum_required(VERSION 3.7)
project(Pthreads_Red_Blue_Computation)

set(CMAKE_C_STANDARD 99)

find_package (Threads)

set(SOURCE_FILES pthreads_red_blue_computation.c)
add_executable(Pthreads_Red_Blue_Computation ${SOURCE_FILES})

target_link_libraries (red_blue ${CMAKE_THREAD_LIBS_INIT})