project(smalloc_tests C)

include_directories("${smalloc_SOURCE_DIR}/include")
link_directories("${smalloc_SOURCE_DIR}")

add_executable(test_01 test_01.c)
target_link_libraries(test_01 smalloc)
