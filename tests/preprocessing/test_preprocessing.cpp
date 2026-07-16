// большинство тестов писали ллм

#include <io/mmap_file.h>
#include <leaderrank/leaderrank.h>
#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using leaderrank::LeaderRankCounter;
using leaderrank::MMapFile;

namespace {

std::string WriteInputFile(const std::string& name, const std::string& body) {
    std::string path = "/tmp/leaderrank_test_" + name + ".txt";
    std::ofstream out(path, std::ios::binary);
    out << "from, to\n" << body;
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

TEST(Preprocess, SimpleGraphDegreesAreCorrect) {
    // 0 -> 1, 0 -> 2, 1 -> 2  => degree(0)=2, degree(1)=1, degree(2)=0
    auto path = WriteInputFile("simple", "0 1\n0 2\n1 2\n");

    LeaderRankCounter counter(path, "/tmp/leaderrank_out_simple.csv",
                              /*threads=*/2, /*eps=*/1e-6, /*max_iters=*/50);
    counter.Run();

    MMapFile degrees("vertex_degrees.bin");
    EXPECT_EQ(degrees.GetSize(), 3 * sizeof(size_t));
    EXPECT_EQ(degrees.Read<size_t>(0 * sizeof(size_t)), 2u);
    EXPECT_EQ(degrees.Read<size_t>(1 * sizeof(size_t)), 1u);
    EXPECT_EQ(degrees.Read<size_t>(2 * sizeof(size_t)), 0u);

    CleanupArtifacts(2);
    std::remove(path.c_str());
}

TEST(Preprocess, DegreeSumEqualsEdgeCount) {
    std::string body;
    constexpr int kEdges = 500;
    for (int i = 0; i < kEdges; ++i) {
        body += std::to_string(i % 50) + " " + std::to_string((i + 1) % 50) + "\n";
    }
    auto path = WriteInputFile("degree_sum", body);

    LeaderRankCounter counter(path, "/tmp/leaderrank_out_sum.csv",
                              /*threads=*/4, /*eps=*/1e-6, /*max_iters=*/50);
    counter.Run();

    MMapFile degrees("vertex_degrees.bin");
    size_t num_vertices = degrees.GetSize() / sizeof(size_t);
    size_t sum = 0;
    for (size_t v = 0; v < num_vertices; ++v) {
        sum += degrees.Read<size_t>(v * sizeof(size_t));
    }
    EXPECT_EQ(sum, static_cast<size_t>(kEdges));

    CleanupArtifacts(4);
    std::remove(path.c_str());
}

TEST(Preprocess, ResultConsistentAcrossThreadCounts) {
    std::string body;
    constexpr int kEdges = 1000;
    for (int i = 0; i < kEdges; ++i) {
        body += std::to_string(i % 100) + " " + std::to_string((i * 7) % 100) + "\n";
    }

    auto path1 = WriteInputFile("consistency_1", body);
    LeaderRankCounter counter1(path1, "/tmp/leaderrank_out_c1.csv",
                               /*threads=*/1, /*eps=*/1e-6, /*max_iters=*/50);
    counter1.Run();
    std::vector<size_t> degrees_1thread;
    {
        MMapFile degrees("vertex_degrees.bin");
        size_t n = degrees.GetSize() / sizeof(size_t);
        for (size_t v = 0; v < n; ++v) {
            degrees_1thread.push_back(degrees.Read<size_t>(v * sizeof(size_t)));
        }
    }
    CleanupArtifacts(1);
    std::remove(path1.c_str());

    auto path8 = WriteInputFile("consistency_8", body);
    LeaderRankCounter counter8(path8, "/tmp/leaderrank_out_c8.csv",
                               /*threads=*/8, /*eps=*/1e-6, /*max_iters=*/50);
    counter8.Run();
    std::vector<size_t> degrees_8threads;
    {
        MMapFile degrees("vertex_degrees.bin");
        size_t n = degrees.GetSize() / sizeof(size_t);
        for (size_t v = 0; v < n; ++v) {
            degrees_8threads.push_back(degrees.Read<size_t>(v * sizeof(size_t)));
        }
    }
    CleanupArtifacts(8);
    std::remove(path8.c_str());

    ASSERT_EQ(degrees_1thread.size(), degrees_8threads.size());
    EXPECT_EQ(degrees_1thread, degrees_8threads);
}

TEST(Preprocess, WrongPrefixThrows) {
    std::string path = "/tmp/leaderrank_test_bad_prefix.txt";
    std::ofstream out(path, std::ios::binary);
    out << "not the right header\n0 1\n";
    out.close();

    LeaderRankCounter counter(path, "/tmp/leaderrank_out_bad.csv",
                              /*threads=*/2, /*eps=*/1e-6, /*max_iters=*/50);
    EXPECT_THROW(counter.Run(), std::runtime_error);

    std::remove(path.c_str());
}

TEST(Preprocess, MalformedEdgeLineThrows) {
    auto path = WriteInputFile("malformed", "0 1\n2  3\n");

    LeaderRankCounter counter(path, "/tmp/leaderrank_out_malformed.csv",
                              /*threads=*/1, /*eps=*/1e-6, /*max_iters=*/50);
    EXPECT_THROW(counter.Run(), std::runtime_error);

    std::remove(path.c_str());
}

TEST(Preprocess, SelfLoopStillWorks) {
    auto path = WriteInputFile("self_loop", "0 0\n");

    LeaderRankCounter counter(path, "/tmp/leaderrank_out_loop.csv",
                              /*threads=*/1, /*eps=*/1e-6, /*max_iters=*/50);
    counter.Run();

    MMapFile degrees("vertex_degrees.bin");
    EXPECT_EQ(degrees.GetSize(), 1 * sizeof(size_t));
    EXPECT_EQ(degrees.Read<size_t>(0), 1u);

    CleanupArtifacts(1);
    std::remove(path.c_str());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();
    return result;
}