add_library(leaderrank_lib
    leaderrank.cpp
)

target_include_directories(leaderrank_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(leaderrank_lib
    PUBLIC
        mmap_file_lib
        graph_lib
)
