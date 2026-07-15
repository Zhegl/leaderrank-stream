#pragma once

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <stdexcept>

namespace leaderrank {

class MMapFile {
public:
    MMapFile(const std::string& path); // Read only

    MMapFile(const std::string& path, size_t size); // Create with size

    ~MMapFile();

    size_t GetSize() {
        return size_;
    }

    template <typename T>
    T Read(size_t pos) {
        T result;
        memcpy(&result, base_ + pos, sizeof(T));
        return result;
    }

    template <typename T>
    void Write(size_t pos, const T& value) {
        std::memcpy(base_ + pos, &value, sizeof(T));
    }

    template <typename T>
    void Add(size_t pos, T delta) {
        std::atomic_ref<T> slot(*reinterpret_cast<T*>(base_ + pos));
        slot.fetch_add(delta);
    }

private:
    char* base_;
    size_t size_;
    int fd_;
};

}  // namespace leaderrank