#include "graph.h"
#include <memory>
#include <numeric>
#include "mmap_file.h"

namespace leaderrank {

LoadedGraph GraphLoader::Load(const std::string& input_path, size_t threads) {
    MMapFile input(input_path);
    auto division = DivideFile(threads, input);
    return ParseByDivison(input, division);
}

GraphLoader::ThreadDivResult GraphLoader::DivideFile(size_t threads, MMapFile& input_reader) {
    static constexpr std::string_view kFilePrefix = "from,to\n";

    for (size_t i = 0; i < kFilePrefix.size(); ++i) {
        if (input_reader.Read<char>(i) != kFilePrefix[i]) {
            throw std::runtime_error("Wrong file format: incorrect prefix");
        }
    }

    ThreadDivResult result;
    result.edges_count.resize(threads);
    result.file_pos.resize(threads + 1);

    std::atomic<bool> file_is_too_small = false;

    ParallelFor(threads, [&](size_t t) {
        size_t count = 0;
        size_t start = std::max(t * input_reader.GetSize() / threads, kFilePrefix.size());
        size_t finish = (t + 1) * input_reader.GetSize() / threads;
        size_t last_pos = -1;
        for (size_t i = start; i < finish; ++i) {
            if (input_reader.Read<char>(i) == '\n') {
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
        threads = 1;
        return {{static_cast<size_t>(
                    std::accumulate(result.edges_count.begin(), result.edges_count.end(), 0))},
                {kFilePrefix.size(), input_reader.GetSize()}};
    }

    return result;
}

LoadedGraph GraphLoader::ParseByDivison(MMapFile& input_reader, const ThreadDivResult& division) {
    LoadedGraph result;
    size_t threads = division.edges_count.size();

    std::atomic<uint32_t> max_edge_id = 0;
    result.divided_edges.resize(threads);

    ParallelFor(threads, [&](size_t t) {
        uint32_t max_edge_in_thread = 0;
        result.divided_edges[t] =
            std::make_unique<MMapFile>("edges_thread_" + std::to_string(t) + ".bin",
                                       division.edges_count[t] * 2 * sizeof(uint32_t));
        bool is_reading_from = true;
        uint32_t from = 0;
        uint32_t to = 0;

        size_t edge_id = 0;
        for (size_t i = division.file_pos[t]; i < division.file_pos[t + 1]; ++i) {
            char c = input_reader.Read<char>(i);
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

                result.divided_edges[t]->Write<uint32_t>(edge_id * 2, from);
                result.divided_edges[t]->Write<uint32_t>(edge_id * 2 + 1, to);

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

    result.vertex_amount = max_edge_id + 1;

    CountDegrees(result);

    return result;
}

void GraphLoader::CountDegrees(LoadedGraph& graph) {
    graph.vertex_degrees =
        std::make_unique<MMapFile>("vertex_degrees.bin", graph.vertex_amount * sizeof(size_t));
    size_t threads = graph.divided_edges.size();
    ParallelFor(threads, [&](size_t t) {
        for (size_t i = 0; i < graph.divided_edges[t]->GetSizeFor<uint32_t>(); i += 2) {
            graph.vertex_degrees->Add<size_t>(graph.divided_edges[t]->Read<uint32_t>(i), 1);
        }
    });
}

}  // namespace leaderrank
