#include "leaderrank.h"
#include <io/mmap_file.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numeric>
#include <stdexcept>
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

double LeaderRankCounter::Step() {
    return 0;
}

void LeaderRankCounter::Preprocess() {
    // проход 1: считаем количество ребер + разбиваем строки по потокам
    auto division = DivideFile();

    // проход 2: парсим ребра, каждый поток создает свой файл в удобном формате
    auto vertex_amount = ParseByDivison(division);

    // создаем и готовим файлы
    old_step_result_ =
        std::make_unique<MMapFile>("old_step_result.bin", vertex_amount * sizeof(double));
    current_step_result_ =
        std::make_unique<MMapFile>("current_step_result.bin", vertex_amount * sizeof(double));
    current_step_result_->Fill<double>(1.0);
    vertex_degrees_ =
        std::make_unique<MMapFile>("vertex_degrees.bin", vertex_amount * sizeof(size_t));

    CountDegrees();
}

LeaderRankCounter::ThreadDivResult LeaderRankCounter::DivideFile() {
    static constexpr std::string_view kFilePrefix = "from, to\n";

    for (size_t i = 0; i < kFilePrefix.size(); ++i) {
        if (input_reader_.Read<char>(i) != kFilePrefix[i]) {
            throw std::runtime_error("Wrong file format: incorrect prefix");
        }
    }

    ThreadDivResult result;
    result.edges_count.resize(threads_);
    result.file_pos.resize(threads_ + 1);

    std::atomic<bool> file_is_too_small = false;

    ParallelFor(threads_, [&](size_t t) {
        size_t count = 0;
        size_t start = std::max(t * input_reader_.GetSize() / threads_, kFilePrefix.size());
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

    result.file_pos[0] = kFilePrefix.size();

    if (file_is_too_small) {
        threads_ = 1;
        return {{static_cast<size_t>(
                    std::accumulate(result.edges_count.begin(), result.edges_count.end(), 0))},
                {kFilePrefix.size(), input_reader_.GetSize()}};
    }

    return result;
}

size_t LeaderRankCounter::ParseByDivison(const ThreadDivResult& division) {
    std::atomic<uint32_t> max_edge_id = 0;
    divided_edges_.resize(threads_);

    ParallelFor(threads_, [&](size_t t) {
        uint32_t max_edge_in_thread = 0;
        divided_edges_[t] =
            std::make_unique<MMapFile>("edges_thread_" + std::to_string(t) + ".bin",
                                       division.edges_count[t] * 2 * sizeof(uint32_t));
        bool is_reading_from = true;
        uint32_t from = 0;
        uint32_t to = 0;

        size_t edge_id = 0;
        for (size_t i = division.file_pos[t]; i < division.file_pos[t + 1]; ++i) {
            char c = input_reader_.Read<char>(i);
            if (c == ' ') {
                if (!is_reading_from) {
                    throw std::runtime_error("Wrong file format");
                }
                is_reading_from = false;
            } else if (c == '\n') {
                if (is_reading_from) {
                    throw std::runtime_error("Wrong file format");
                }

                max_edge_in_thread = std::max(max_edge_in_thread, from);
                max_edge_in_thread = std::max(max_edge_in_thread, to);

                divided_edges_[t]->Write(edge_id * 2 * sizeof(uint32_t), from);
                divided_edges_[t]->Write(edge_id * 2 * sizeof(uint32_t) + sizeof(uint32_t), to);

                from = 0;
                to = 0;

                is_reading_from = true;
                ++edge_id;
            } else {
                uint32_t add = c - '0';
                if (0 > add || add > 9) {
                    throw std::runtime_error("Wrong file format");
                }
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

    return max_edge_id + 1;
}

void LeaderRankCounter::CountDegrees() {
    ParallelFor(threads_, [&](size_t t) {
        for (size_t i = 0; i < divided_edges_[t]->GetSize(); i += sizeof(uint32_t) * 2) {
            vertex_degrees_->Add<size_t>(divided_edges_[t]->Read<uint32_t>(i) * sizeof(size_t), 1);
        }
    });
}

}  // namespace leaderrank
