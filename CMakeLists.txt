cmake_minimum_required(VERSION 3.16)
project(columnar_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY           ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG     ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE   ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_compile_options(-march=native -mtune=native)

if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-O3)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug build: sanitizers enabled")
    add_compile_options(
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )
    add_link_options(
        -fsanitize=address,undefined
    )
endif()


add_subdirectory(cmake)
add_subdirectory(src)
add_subdirectory(tests)

enable_testing()