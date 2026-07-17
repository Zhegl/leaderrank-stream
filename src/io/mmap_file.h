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

    MMapFile(const std::string& path, size_t size);  // Создаем на запись

    ~MMapFile();

    size_t GetSize() const {
        return size_;
    }

    template <typename T>
    size_t GetSizeFor() const {
        return GetSize() / sizeof(T);
    }

    template <typename T>
    T Read(size_t index) const {
        T result;
        std::memcpy(&result, base_ + index * sizeof(T), sizeof(T));
        return result;
    }

    template <typename T>
    void Write(size_t index, const T& value) const {
        std::memcpy(base_ + index * sizeof(T), &value, sizeof(T));
    }

    template <typename T>
    void Add(size_t index, T delta) const {
        std::atomic_ref<T> slot(*reinterpret_cast<T*>(base_ + index * sizeof(T)));
        slot.fetch_add(delta);
    }

    template <typename T>
    void Fill(const T& value, size_t threads = 1) const {
        size_t count = GetSizeFor<T>();
        if (threads <= 1) {
            for (size_t i = 0; i < count; ++i) {
                Write(i, value);
            }
            return;
        }

        ParallelFor(threads, [&](size_t t) {
            size_t start = t * count / threads;
            size_t finish = (t + 1) * count / threads;
            for (size_t i = start; i < finish; ++i) {
                Write(i, value);
            }
        });
    }

private:
    char* base_;
    size_t size_;
    int fd_;
};

}  // namespace leaderrank