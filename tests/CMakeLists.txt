project(smalloc_tests C)

include_directories("${smalloc_SOURCE_DIR}/include")
link_directories("${smalloc_SOURCE_DIR}")

add_executable(test_00 test_00.c)
add_executable(test_01 test_01.c)
add_executable(test_02 test_02.c)

target_link_libraries(test_00 smalloc)
target_link_libraries(test_01 smalloc)
target_link_libraries(test_02 smalloc)
