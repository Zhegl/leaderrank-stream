add_library(graph_lib
    graph.cpp
)

target_include_directories(graph_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(graph_lib
    PUBLIC
        mmap_file_lib
)
