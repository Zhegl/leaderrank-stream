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
    std::unique_ptr<MMapFile> vertex_degrees_;
    std::vector<std::unique_ptr<MMapFile>>
        divided_edges_;  // тут не будет фолс шеринга т.к. записываем один раз за тред при
                         // процессинге, дальше RO

    std::string output_path_;
    size_t threads_;
    double eps_;
    size_t max_iters_;
};

}  // namespace leaderrank
