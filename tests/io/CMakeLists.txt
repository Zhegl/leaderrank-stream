add_executable(test_io
    test_io.cpp
)
target_link_libraries(test_io
    PRIVATE
        mmap_file_lib
        gtest
        glog::glog
)

