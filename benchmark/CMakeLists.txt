find_package(benchmark REQUIRED)

add_executable(small_unique_ptr_benchmark "${CMAKE_CURRENT_SOURCE_DIR}/small_unique_ptr.cpp")
target_link_libraries(small_unique_ptr_benchmark PRIVATE small_unique_ptr benchmark::benchmark_main)
