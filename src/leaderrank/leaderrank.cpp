#include "leaderrank.h"
#include <io/mmap_file.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <string>
#include <thread>
#include <vector>
#include <utils/parallel.h>

namespace leaderrank {

void LeaderRankCounter::Run() {
    Preprocess();
    for (size_t iter = 0; iter < max_iters_; ++iter) {
        if (Step() < eps_) {
            break;
        }
    }
}

void LeaderRankCounter::Preprocess() {
    // проход 1: считаем количество ребер + разбиваем строки по потокам
    auto division = DivideFile();

    // проход 2: парсим ребра, каждый поток создает свой файл в удобном формате
    auto max_vertix_id = ParseByDivison(division);

    // проход 3: считаем кол-во исходящих ребер для каждого ребра + готовим файл текущего файла
}

LeaderRankCounter::ThreadDivResult LeaderRankCounter::DivideFile() {
    ThreadDivResult result;
    result.edges_count.resize(threads_);
    result.file_pos.resize(threads_ + 1);

    std::atomic<bool> file_is_too_small = false;

    ParallelFor(threads_, [&](size_t t) {
        size_t count = 0;
        size_t start = t * input_reader_.GetSize() / threads_;
        size_t finish = (t + 1) * input_reader_.GetSize() / threads_;
        size_t last_pos = -1;
        for (size_t i = start; i < finish; ++i) {
            if (input_reader_.Read<char>(i) == '\n') {
                last_pos = i;
                ++count;
            }
        }

        if (last_pos == -1) {
            file_is_too_small = true;
        }

        result.file_pos[t + 1] = last_pos + 1;
        result.edges_count[t] += count;
    });

    if (file_is_too_small) {
        threads_ = 1;
        return {{static_cast<size_t>(
                    std::accumulate(result.edges_count.begin(), result.edges_count.end(), 0))},
                {0, input_reader_.GetSize()}};
    }

    return result;
}

size_t LeaderRankCounter::ParseByDivison(const ThreadDivResult& division) {
    std::atomic<uint32_t> max_edge_id = 0;
    ParallelFor(threads_, [&](size_t t) {
        uint32_t max_edge_in_thread = 0;
        MMapFile edges("edges_thread_" + std::to_string(t) + ".bin",
                       division.edges_count[t] * 2 * sizeof(uint32_t));
        bool is_reading_from = true;
        uint32_t from = 0;
        uint32_t to = 0;

        size_t edge_id = 0;
        for (size_t i = division.file_pos[t]; i < division.file_pos[t + 1]; ++i) {
            char c = input_reader_.Read<char>(i);
            if (c == ' ') {
                is_reading_from = false;
            } else if (c == '\n') {
                max_edge_in_thread = std::max(max_edge_in_thread, from);
                max_edge_in_thread = std::max(max_edge_in_thread, to);

                edges.Write(edge_id * 2 * sizeof(uint32_t), from);
                edges.Write(edge_id * 2 * sizeof(uint32_t) + sizeof(uint32_t), to);

                from = 0;
                to = 0;

                is_reading_from = true;
                ++edge_id;
            } else {
                uint32_t add = c - '0';
                if (is_reading_from) {
                    from = from * 10 + add;
                } else {
                    to = to * 10 + add;
                }
            }
        }

        uint32_t current = max_edge_id.load(std::memory_order_relaxed);
        while (current < max_edge_in_thread &&
               !max_edge_id.compare_exchange_weak(current, max_edge_in_thread,
                                                  std::memory_order_relaxed)) {
        }
    });

    return max_edge_id;
}

}  // namespace leaderrank
