cmake_minimum_required(VERSION 3.7)
project(jitter)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE")

set(SOURCE_FILES main.c)
add_executable(jitter ${SOURCE_FILES})