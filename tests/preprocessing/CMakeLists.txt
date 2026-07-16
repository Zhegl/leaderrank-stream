add_executable(test_preprocessing
    test_preprocessing.cpp
)
target_link_libraries(test_preprocessing
    PRIVATE
        mmap_file_lib
        leaderrank_lib
        gtest
        glog::glog
)

