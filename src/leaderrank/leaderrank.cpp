#include "leaderrank.h"
#include <io/mmap_file.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include "format/bin_to_csv.h"
#include <utils/parallel.h>

namespace leaderrank {

void LeaderRankCounter::Run() {
    for (size_t iter = 0; iter < max_iters_; ++iter) {
        if (Step() < eps_) {
            break;
        }
    }
    Finish();
    ConvertBinToCsv(*current_step_.values, output_path_);
}

double LeaderRankCounter::Step() {
    std::atomic<double> max_delta = 0;

    std::swap(current_step_.values, prev_step_.values);
    current_step_.values->Fill<double>(0);

    prev_step_.ground_value.store(current_step_.ground_value.load());
    current_step_.ground_value.store(0);

    ParallelFor(threads_, [&](size_t t) {
        for (size_t i = 0; i < graph_.divided_edges[t]->GetSizeFor<uint32_t>(); i += 2) {
            uint32_t from = graph_.divided_edges[t]->Read<uint32_t>(i);
            uint32_t to = graph_.divided_edges[t]->Read<uint32_t>(i + 1);

            double from_old_val = prev_step_.values->Read<double>(from);
            double from_degree = graph_.vertex_degrees->Read<size_t>(from);

            current_step_.values->Add<double>(
                to, from_old_val / (from_degree + 1));  // +1 т.к. еще есть ground вершина
        }
    });

    // отдельно считаем неявные ребра ground вершины
    size_t vertex_amount = graph_.vertex_amount;
    size_t use_threads = (vertex_amount >= threads_ ? threads_ : 1);
    ParallelFor(use_threads, [&](size_t t) {
        double max_delta_in_thread = 0;
        size_t start = t * vertex_amount / use_threads;
        size_t finish = (t + 1) * vertex_amount / use_threads;
        for (size_t vertex = start; vertex < finish; ++vertex) {
            double degree = graph_.vertex_degrees->Read<size_t>(vertex);
            double old_result = prev_step_.values->Read<double>(vertex);

            current_step_.ground_value.fetch_add(old_result / (degree + 1));
            current_step_.values->Add<double>(vertex,
                                              prev_step_.ground_value.load() / vertex_amount);

            double new_result = current_step_.values->Read<double>(vertex);

            max_delta_in_thread = std::max(max_delta_in_thread, std::abs(new_result - old_result));
        }

        double current = max_delta.load();
        while (current < max_delta_in_thread &&
               !max_delta.compare_exchange_weak(current, max_delta_in_thread)) {
        }
    });

    max_delta =
        std::max(max_delta.load(), std::abs(current_step_.ground_value - prev_step_.ground_value));

    return max_delta.load();
}

void LeaderRankCounter::Finish() {
    size_t vertex_amount = graph_.vertex_amount;
    size_t use_threads = (vertex_amount >= threads_ ? threads_ : 1);

    ParallelFor(use_threads, [&](size_t t) {
        double max_delta_in_thread = 0;
        size_t start = t * vertex_amount / use_threads;
        size_t finish = (t + 1) * vertex_amount / use_threads;
        for (size_t vertex = start; vertex < finish; ++vertex) {
            current_step_.values->Add<double>(vertex,
                                              current_step_.ground_value / graph_.vertex_amount);
        }
    });
}

}  // namespace leaderrank
