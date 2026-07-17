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
#include "format/bin_to_csv.h"
#include <utils/parallel.h>

namespace leaderrank {

void LeaderRankCounter::Run() {
    Preprocess();
    for (size_t iter = 0; iter < max_iters_; ++iter) {
        if (Step() < eps_) {
            break;
        }
    }
    ConvertBinToCsv(*current_step_result_, output_path_,
                    current_ground_result_ / current_step_result_->GetSizeFor<double>());
}

double LeaderRankCounter::Step() {
    std::atomic<double> max_delta = 0;

    std::swap(old_step_result_, current_step_result_);

    old_ground_result_.store(current_ground_result_.load());
    current_ground_result_.store(0);

    current_step_result_->Fill<double>(0);

    ParallelFor(threads_, [&](size_t t) {
        for (size_t i = 0; i < divided_edges_[t]->GetSizeFor<uint32_t>(); i += 2) {
            uint32_t from = divided_edges_[t]->Read<uint32_t>(i);
            uint32_t to = divided_edges_[t]->Read<uint32_t>(i + 1);

            double from_old_val = old_step_result_->Read<double>(from);
            double from_degree = vertex_degrees_->Read<size_t>(from);

            current_step_result_->Add<double>(
                to, from_old_val / (from_degree + 1));  // +1 т.к. еще есть ground вершина
        }
    });

    // отдельно считаем неявные ребра ground вершины
    size_t vertex_amount = current_step_result_->GetSizeFor<double>();
    size_t use_threads = (vertex_amount >= threads_ ? threads_ : 1);
    ParallelFor(use_threads, [&](size_t t) {
        double max_delta_in_thread = 0;
        size_t start = t * vertex_amount / use_threads;
        size_t finish = (t + 1) * vertex_amount / use_threads;
        for (size_t vertex = start; vertex < finish; ++vertex) {
            double degree = vertex_degrees_->Read<size_t>(vertex);
            double old_result = old_step_result_->Read<double>(vertex);

            current_ground_result_.fetch_add(old_result / (degree + 1));
            current_step_result_->Add<double>(vertex, old_ground_result_.load() / vertex_amount);

            double new_result = current_step_result_->Read<double>(vertex);

            max_delta_in_thread = std::max(max_delta_in_thread, std::abs(new_result - old_result));
        }

        double current = max_delta.load();
        while (current < max_delta_in_thread &&
               !max_delta.compare_exchange_weak(current, max_delta_in_thread)) {
        }
    });

    max_delta = std::max(max_delta.load(), std::abs(current_ground_result_ - old_ground_result_));

    return max_delta.load();
}

void LeaderRankCounter::Preprocess() {
    // проход 1: разбиваем строки по потокам + готовим данные для парсера
    auto division = DivideFile();

    // проход 2: парсим ребра, каждый поток создает свой файл в удобном формате
    auto vertex_amount = ParseByDivison(division);

    // создаем и готовим остальные файлы
    old_step_result_ =
        std::make_unique<MMapFile>("old_step_result.bin", vertex_amount * sizeof(double));
    current_step_result_ =
        std::make_unique<MMapFile>("current_step_result.bin", vertex_amount * sizeof(double));
    current_step_result_->Fill<double>(1.0);
    vertex_degrees_ =
        std::make_unique<MMapFile>("vertex_degrees.bin", vertex_amount * sizeof(size_t));

    current_ground_result_ = 0;

    CountDegrees();
}

LeaderRankCounter::ThreadDivResult LeaderRankCounter::DivideFile() {
    static constexpr std::string_view kFilePrefix = "from,to\n";

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
            if (c == ' ' || c == ',') {
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

                divided_edges_[t]->Write<uint32_t>(edge_id * 2, from);
                divided_edges_[t]->Write<uint32_t>(edge_id * 2 + 1, to);

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

        uint32_t current = max_edge_id.load();
        while (current < max_edge_in_thread &&
               !max_edge_id.compare_exchange_weak(current, max_edge_in_thread)) {
        }
    });

    return max_edge_id + 1;
}

void LeaderRankCounter::CountDegrees() {
    ParallelFor(threads_, [&](size_t t) {
        for (size_t i = 0; i < divided_edges_[t]->GetSizeFor<uint32_t>(); i += 2) {
            vertex_degrees_->Add<size_t>(divided_edges_[t]->Read<uint32_t>(i), 1);
        }
    });
}

}  // namespace leaderrank
