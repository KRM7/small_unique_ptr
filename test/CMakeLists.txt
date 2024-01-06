find_package(Catch2 3 REQUIRED)

include(Catch)
include(CTest)
enable_testing()

add_executable(small_unique_ptr_test "${CMAKE_CURRENT_SOURCE_DIR}/small_unique_ptr.cpp")
target_link_libraries(small_unique_ptr_test PRIVATE Catch2::Catch2WithMain small_unique_ptr)

catch_discover_tests(small_unique_ptr_test)