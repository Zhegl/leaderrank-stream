#pragma once
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <io/mmap_file.h>

namespace leaderrank {

class LeaderRankCounter {
public:
    LeaderRankCounter(const std::string& input_path, std::string output_path, size_t threads,
                      double eps, size_t max_iters)
        : input_reader_(input_path),
          output_path_(std::move(output_path)),
          threads_(threads),
          eps_(eps),
          max_iters_(max_iters) {
    }

    void Run();

    double GetRank(uint32_t vertex) {
        return current_step_result_->Read<double>(vertex) +
               current_ground_result_.load() / current_step_result_->GetSizeFor<double>();
    }

    size_t VertexCount() {
        return current_step_result_->GetSizeFor<double>();
    }

private:
    void Preprocess();

    double Step();

    struct ThreadDivResult {
        std::vector<size_t> edges_count;
        std::vector<size_t> file_pos;
    };

    ThreadDivResult DivideFile();

    size_t ParseByDivison(const ThreadDivResult& division);

    void CountDegrees();

    MMapFile input_reader_;
    std::unique_ptr<MMapFile> old_step_result_;
    std::unique_ptr<MMapFile> current_step_result_;
    std::atomic<double> old_ground_result_;
    std::atomic<double> current_ground_result_;

    std::unique_ptr<MMapFile> vertex_degrees_;
    std::vector<std::unique_ptr<MMapFile>> divided_edges_;

    std::string output_path_;
    size_t threads_;
    double eps_;
    size_t max_iters_;
};

}  // namespace leaderrank
