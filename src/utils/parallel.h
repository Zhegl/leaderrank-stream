#pragma once

#include <cstddef>
#include <thread>
#include <vector>

namespace leaderrank {

template <typename Fn>
void ParallelFor(size_t num_threads, Fn&& fn) {
    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    std::exception_ptr first_exception = nullptr;
    std::mutex exception_mutex;

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t] {
            try {
                fn(t);
            } catch (...) {
                std::lock_guard<std::mutex> lock(exception_mutex);
                if (!first_exception) {
                    first_exception = std::current_exception();
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    if (first_exception) {
        std::rethrow_exception(first_exception);
    }
}

}  // namespace leaderrank