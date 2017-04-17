cmake_minimum_required(VERSION 3.0)
project(smalloc C)

include_directories("${smalloc_SOURCE_DIR}/include")

add_library(smalloc STATIC
    src/smalloc.c)

add_subdirectory(tests)
