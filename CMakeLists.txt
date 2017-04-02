cmake_minimum_required(VERSION 3.7)
project(jitter-sampler)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE")

file(GLOB SOURCES src/*.c src/*.h)
add_executable(jitter ${SOURCES} src/csv.h src/csv.c)