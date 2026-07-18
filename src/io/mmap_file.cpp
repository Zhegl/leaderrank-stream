#include "mmap_file.h"
namespace leaderrank {

MMapFile::MMapFile(const std::string& path) : path_(path) {
    fd_ = open(path.c_str(), O_RDONLY);
    if (fd_ < 0) {
        throw std::runtime_error("Failed to open " + path);
    }
    struct stat st;
    if (fstat(fd_, &st) < 0) {
        close(fd_);
        throw std::runtime_error("Failed to stat " + path);
    }
    size_ = static_cast<size_t>(st.st_size);
    if (size_ > 0) {
        base_ = static_cast<char*>(mmap(nullptr, size_, PROT_READ, MAP_SHARED, fd_, 0));
        if (base_ == MAP_FAILED) {
            close(fd_);
            throw std::runtime_error("Failed to mmap " + path);
        }
        madvise(base_, size_, MADV_SEQUENTIAL);
    }
}

MMapFile::MMapFile(const std::string& path, size_t size, bool temporary)
    : size_(size), temporary_(temporary), path_(path) {
    fd_ = open(path.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd_ < 0) {
        throw std::runtime_error("Failed to open " + path);
    }
    if (size_ > 0 && ftruncate(fd_, static_cast<off_t>(size_)) < 0) {
        throw std::runtime_error("Failed to ftruncate " + path);
    }
    if (size_ > 0) {
        base_ =
            static_cast<char*>(mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0));
        if (base_ == MAP_FAILED) {
            close(fd_);
            throw std::runtime_error("Failed to mmap " + path);
        }
    }
}

MMapFile::~MMapFile() {
    if (base_ && size_ > 0) {
        munmap(base_, size_);
    }
    if (fd_ >= 0) {
        close(fd_);
    }
    if (temporary_) {
        std::remove(path_.c_str());
    }
}

}  // namespace leaderrank
