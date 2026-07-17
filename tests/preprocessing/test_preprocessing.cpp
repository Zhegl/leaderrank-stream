// большинство тестов писали ллм

#include <graph_loader/graph.h>
#include <io/mmap_file.h>
#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using leaderrank::GraphLoader;
using leaderrank::LoadedGraph;
using leaderrank::MMapFile;

namespace {

std::string WriteInputFile(const std::string& name, const std::string& body) {
    std::string path = "/tmp/leaderrank_test_" + name + ".txt";
    std::ofstream out(path, std::ios::binary);
    out << "from,to\n" << body;
    return path;
}

void CleanupArtifacts(size_t threads) {
    std::remove("vertex_degrees.bin");
    std::remove("old_step_result.bin");
    std::remove("current_step_result.bin");
    for (size_t t = 0; t < threads; ++t) {
        std::remove(("edges_thread_" + std::to_string(t) + ".bin").c_str());
    }
}

}  // namespace

TEST(GraphLoader, SimpleGraphDegreesAreCorrect) {
    // 0 -> 1, 0 -> 2, 1 -> 2  => degree(0)=2, degree(1)=1, degree(2)=0
    auto path = WriteInputFile("simple", "0 1\n0 2\n1 2\n");

    GraphLoader loader;
    LoadedGraph graph = loader.Load(path, /*threads=*/2);

    EXPECT_EQ(graph.vertex_amount, 3u);
    EXPECT_EQ(graph.vertex_degrees->Read<size_t>(0), 2u);
    EXPECT_EQ(graph.vertex_degrees->Read<size_t>(1), 1u);
    EXPECT_EQ(graph.vertex_degrees->Read<size_t>(2), 0u);

    CleanupArtifacts(2);
    std::remove(path.c_str());
}

TEST(GraphLoader, DegreeSumEqualsEdgeCount) {
    std::string body;
    constexpr int kEdges = 500;
    for (int i = 0; i < kEdges; ++i) {
        body += std::to_string(i % 50) + " " + std::to_string((i + 1) % 50) + "\n";
    }
    auto path = WriteInputFile("degree_sum", body);

    GraphLoader loader;
    LoadedGraph graph = loader.Load(path, /*threads=*/4);

    size_t sum = 0;
    for (size_t v = 0; v < graph.vertex_amount; ++v) {
        sum += graph.vertex_degrees->Read<size_t>(v);
    }
    EXPECT_EQ(sum, static_cast<size_t>(kEdges));

    CleanupArtifacts(4);
    std::remove(path.c_str());
}

TEST(GraphLoader, ResultConsistentAcrossThreadCounts) {
    std::string body;
    constexpr int kEdges = 1000;
    for (int i = 0; i < kEdges; ++i) {
        body += std::to_string(i % 100) + " " + std::to_string((i * 7) % 100) + "\n";
    }

    auto path1 = WriteInputFile("consistency_1", body);
    GraphLoader loader1;
    LoadedGraph graph1 = loader1.Load(path1, /*threads=*/1);
    std::vector<size_t> degrees_1thread;
    for (size_t v = 0; v < graph1.vertex_amount; ++v) {
        degrees_1thread.push_back(graph1.vertex_degrees->Read<size_t>(v));
    }
    CleanupArtifacts(1);
    std::remove(path1.c_str());

    auto path8 = WriteInputFile("consistency_8", body);
    GraphLoader loader8;
    LoadedGraph graph8 = loader8.Load(path8, /*threads=*/8);
    std::vector<size_t> degrees_8threads;
    for (size_t v = 0; v < graph8.vertex_amount; ++v) {
        degrees_8threads.push_back(graph8.vertex_degrees->Read<size_t>(v));
    }
    CleanupArtifacts(8);
    std::remove(path8.c_str());

    ASSERT_EQ(degrees_1thread.size(), degrees_8threads.size());
    EXPECT_EQ(degrees_1thread, degrees_8threads);
}

TEST(GraphLoader, WrongPrefixThrows) {
    std::string path = "/tmp/leaderrank_test_bad_prefix.txt";
    std::ofstream out(path, std::ios::binary);
    out << "not the right header\n0 1\n";
    out.close();

    GraphLoader loader;
    EXPECT_THROW(loader.Load(path, /*threads=*/2), std::runtime_error);

    std::remove(path.c_str());
}

TEST(GraphLoader, MalformedEdgeLineThrows) {
    auto path = WriteInputFile("malformed", "0 1\n2  3\n");

    GraphLoader loader;
    EXPECT_THROW(loader.Load(path, /*threads=*/1), std::runtime_error);

    std::remove(path.c_str());
}

TEST(GraphLoader, SelfLoopStillWorks) {
    auto path = WriteInputFile("self_loop", "0 0\n");

    GraphLoader loader;
    LoadedGraph graph = loader.Load(path, /*threads=*/1);

    EXPECT_EQ(graph.vertex_amount, 1u);
    EXPECT_EQ(graph.vertex_degrees->Read<size_t>(0), 1u);

    CleanupArtifacts(1);
    std::remove(path.c_str());
}

/*
TEST(GraphLoader, TrailingBlankLineIsIgnored) {
    auto path = WriteInputFile("trailing_newline", "0 1\n1 2\n2 0\n\n");

    GraphLoader loader;
    LoadedGraph graph = loader.Load(path, 2);

    EXPECT_EQ(graph.vertex_amount, 3u);
    size_t sum = 0;
    for (size_t v = 0; v < graph.vertex_amount; ++v) {
        sum += graph.vertex_degrees->Read<size_t>(v);
    }
    EXPECT_EQ(sum, 3u);

    CleanupArtifacts(2);
    std::remove(path.c_str());
}
*/

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}