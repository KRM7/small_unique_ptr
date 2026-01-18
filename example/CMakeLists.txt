#find_package(benchmark REQUIRED)

add_executable(small_unique_ptr_example "${CMAKE_CURRENT_SOURCE_DIR}/move_only_function.cpp")
target_link_libraries(small_unique_ptr_example PRIVATE small_unique_ptr)
