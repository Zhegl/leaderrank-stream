add_executable(app app.cpp
)
target_link_libraries(app
    PRIVATE
        mmap_file_lib
        leaderrank_lib
        gtest
        glog::glog
)

