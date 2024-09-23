cmake_minimum_required(VERSION 3.21)

project(small_unique_ptr VERSION 0.1 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE MATCHES "^(Debug|Release|RelWithDebInfo)$")
    message(WARNING "The specified build type [${CMAKE_BUILD_TYPE}] is not recognized. Defaulting to Release.")
    set(CMAKE_BUILD_TYPE "Release")
endif()

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -permissive- -W4 -WX -wd4324 -diagnostics:caret -Z7")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Werror -pedantic-errors -g")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-maybe-uninitialized")
    endif()
endif()

add_library(small_unique_ptr INTERFACE)
target_include_directories(small_unique_ptr SYSTEM INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
                                                             "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

target_compile_features(small_unique_ptr INTERFACE "cxx_std_20")
target_compile_options(small_unique_ptr INTERFACE "$<$<CXX_COMPILER_ID:MSVC>:-Zc:preprocessor>" "$<$<CXX_COMPILER_ID:MSVC>:-Zc:__cplusplus>")


install(TARGETS small_unique_ptr EXPORT smp-config)
install(DIRECTORY "src/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" FILES_MATCHING PATTERN "*.hpp")

install(EXPORT smp-config
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/smp"
        NAMESPACE smp::)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/smp-config-version.cmake"
    COMPATIBILITY "SameMajorVersion")

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/smp-config-version.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/smp")

include(CTest)

enable_testing()
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/test")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/benchmark")
