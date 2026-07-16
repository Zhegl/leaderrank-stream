#pragma once

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <utils/parallel.h>
#include <stdexcept>

namespace leaderrank {

class MMapFile {
public:
    MMapFile(const std::string& path);  // Read only

    MMapFile(const std::string& path, size_t size);  // Create with size

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

    template <typename T>
    void Fill(const T& value, size_t threads = 1) {
        size_t count = size_ / sizeof(T);
        if (threads <= 1) {
            for (size_t i = 0; i < count; ++i) {
                Write(i * sizeof(T), value);
            }
            return;
        }

        ParallelFor(threads, [&](size_t t) {
            size_t start = t * count / threads;
            size_t finish = (t + 1) * count / threads;
            for (size_t i = start; i < finish; ++i) {
                Write(i * sizeof(T), value);
            }
        });
    }

private:
    char* base_;
    size_t size_;
    int fd_;
};

}  // namespace leaderrank