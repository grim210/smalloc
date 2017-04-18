cmake_minimum_required(VERSION 3.0)
project(smalloc C)

include_directories("${smalloc_SOURCE_DIR}/include")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu89 -Wall -Werror")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DSMALLOC_DEBUG")

add_library(smalloc STATIC
    src/smalloc.c)

add_subdirectory(tests)
