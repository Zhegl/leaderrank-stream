#pragma once

#include <cstddef>
#include <thread>
#include <vector>

template <typename Fn>
void ParallelFor(size_t num_threads, Fn&& fn) {
    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back(fn, t);
    }
    for (auto& th : threads) {
        th.join();
    }
}