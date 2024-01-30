cmake_minimum_required(VERSION 3.21)

project(small_unique_ptr VERSION 0.1 LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build")

if(NOT CMAKE_BUILD_TYPE MATCHES "^(Debug|Release|RelWithDebInfo)$")
    message(WARNING "The specified build type [${CMAKE_BUILD_TYPE}] is not recognized. Defaulting to Release.")
    set(CMAKE_BUILD_TYPE "Release")
endif()

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -permissive- -W4 -WX -wd4324 -diagnostics:caret")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Werror -pedantic-errors -g")
endif()

if(CMAKE_BUILD_TYPE MATCHES "(Release|RelWithDebInfo)")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

add_library(small_unique_ptr INTERFACE)
target_include_directories(small_unique_ptr SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(small_unique_ptr INTERFACE "cxx_std_20")
target_compile_options(small_unique_ptr INTERFACE "$<$<CXX_COMPILER_ID:MSVC>:-Zc:preprocessor>" "$<$<CXX_COMPILER_ID:MSVC>:-Zc:__cplusplus>")

include(CTest)

enable_testing()
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/test")
