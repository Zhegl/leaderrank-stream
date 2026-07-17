add_executable(test_rank
    test_rank.cpp
)
target_link_libraries(test_rank
    PRIVATE
        mmap_file_lib
        leaderrank_lib
        gtest
        glog::glog
)

