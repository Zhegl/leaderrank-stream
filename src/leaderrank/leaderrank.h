#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include <io/mmap_file.h>

namespace leaderrank {

class LeaderRankCounter {
public:
    LeaderRankCounter(const std::string& input_path, std::string output_path, size_t threads,
                      double eps)
        : input_reader_(input_path),
          output_path_(std::move(output_path)),
          threads_(threads),
          eps_(eps),
          max_iters_() {
    }

    void Run();

private:
    void Preprocess();

    struct ThreadDivResult {
        std::vector<size_t> edges_count;
        std::vector<size_t> file_pos;
    };

    ThreadDivResult DivideFile(); 

    size_t ParseByDivison(const ThreadDivResult& division);

    double Step();

    MMapFile input_reader_;

    std::string output_path_;
    size_t threads_;
    double eps_;
    size_t max_iters_;
};

}  // namespace leaderrank
