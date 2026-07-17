#pragma once
#include <io/mmap_file.h>

namespace leaderrank {

struct LoadedGraph {
    std::vector<std::unique_ptr<MMapFile>> divided_edges;
    std::unique_ptr<MMapFile> vertex_degrees;
    size_t vertex_amount;
};

class GraphLoader {
public:
    LoadedGraph Load(const std::string& input_path, size_t threads);

private:
    struct ThreadDivResult {
        std::vector<size_t> edges_count;
        std::vector<size_t> file_pos;
    };

    ThreadDivResult DivideFile(size_t threads, MMapFile& input_reader);
    LoadedGraph ParseByDivison(MMapFile& input_reader, const ThreadDivResult& division);
    void CountDegrees(LoadedGraph& graph);
};

}