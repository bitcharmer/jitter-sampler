cmake_minimum_required(VERSION 3.7)
project(jitter-sampler)

set(CMAKE_C_STANDARD 11)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE -Og -fno-omit-frame-pointer -fno-exceptions -Wall -pedantic -Werror -Wno-error=format-overflow=")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE -O1 -fPIC -fno-omit-frame-pointer")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB SOURCES src/*.c src/*.h)
add_executable(jitter ${SOURCES})
