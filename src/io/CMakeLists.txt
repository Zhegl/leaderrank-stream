add_library(mmap_file_lib
    mmap_file.cpp
)

target_include_directories(mmap_file_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/src
)
