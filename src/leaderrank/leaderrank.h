#pragma once
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <io/mmap_file.h>
#include <graph_loader/graph.h>

namespace leaderrank {

class LeaderRankCounter {
public:
    LeaderRankCounter(const std::string& input_path, std::string output_path, size_t threads,
                      double eps, size_t max_iters)
        : output_path_(std::move(output_path)), eps_(eps), max_iters_(max_iters) {
        GraphLoader graph_loader;
        graph_ = graph_loader.Load(input_path, threads);
        threads_ = graph_.divided_edges.size();

        current_step_.values = std::make_unique<MMapFile>("current_step_result.bin",
                                                          graph_.vertex_amount * sizeof(double));
        current_step_.values->Fill<double>(1.0);
        current_step_.ground_value = 0;

        prev_step_.values = std::make_unique<MMapFile>("prev_step_result.bin",
                                                       graph_.vertex_amount * sizeof(double));
        prev_step_.ground_value = 0;
    }

    void Run();

    double GetRank(uint32_t vertex) const {
        return current_step_.values->Read<double>(vertex);
    }

    size_t VertexCount() const {
        return graph_.vertex_amount;
    }

private:
    double Step();

    void Finish();

    LoadedGraph graph_;

    struct StepState {
        std::unique_ptr<MMapFile> values;
        std::atomic<double> ground_value;
    };

    StepState current_step_;
    StepState prev_step_;

    std::string output_path_;
    size_t threads_;
    double eps_;
    size_t max_iters_;
};

}  // namespace leaderrank
