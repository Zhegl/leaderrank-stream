// большинство тестов писали ллм

#include <io/mmap_file.h>
#include <leaderrank/leaderrank.h>
#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using leaderrank::LeaderRankCounter;

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

TEST(Run, StarGraphCenterHasHighestRank) {
    // Центр 0 получает рёбра от всех листьев — должен получить наибольший ранг.
    std::string body;
    constexpr int kLeaves = 20;
    for (int i = 1; i <= kLeaves; ++i) {
        body += std::to_string(i) + " 0\n";
    }
    auto path = WriteInputFile("star", body);

    LeaderRankCounter counter(path, "/tmp/leaderrank_out_star.csv",
                               /*threads=*/4, /*eps=*/1e-9, /*max_iters=*/200);
    counter.Run();

    double center_rank = counter.GetRank(0);
    for (int i = 1; i <= kLeaves; ++i) {
        EXPECT_GT(center_rank, counter.GetRank(i))
            << "center should outrank leaf " << i;
    }

    CleanupArtifacts(4);
    std::remove(path.c_str());
}

TEST(Run, SymmetricCycleGivesEqualRanks) {
    // Цикл 0->1->2->...->N->0: по симметрии все ранги должны сойтись к одному значению.
    constexpr int kN = 10;
    std::string body;
    for (int i = 0; i < kN; ++i) {
        body += std::to_string(i) + " " + std::to_string((i + 1) % kN) + "\n";
    }
    auto path = WriteInputFile("cycle", body);

    LeaderRankCounter counter(path, "/tmp/leaderrank_out_cycle.csv",
                               /*threads=*/2, /*eps=*/1e-9, /*max_iters=*/500);
    counter.Run();

    double first_rank = counter.GetRank(0);
    for (int i = 1; i < kN; ++i) {
        EXPECT_NEAR(counter.GetRank(i), first_rank, 1e-4)
            << "vertex " << i << " should have rank close to vertex 0 by symmetry";
    }

    CleanupArtifacts(2);
    std::remove(path.c_str());
}

TEST(Run, RanksAreFiniteAndNonNegative) {
    // Общий sanity-чек: никаких NaN/inf/отрицательных значений после сходимости.
    std::string body;
    constexpr int kEdges = 300;
    for (int i = 0; i < kEdges; ++i) {
        body += std::to_string(i % 40) + " " + std::to_string((i * 3 + 1) % 40) + "\n";
    }
    auto path = WriteInputFile("sanity", body);

    LeaderRankCounter counter(path, "/tmp/leaderrank_out_sanity.csv",
                               /*threads=*/4, /*eps=*/1e-9, /*max_iters=*/300);
    counter.Run();

    for (size_t v = 0; v < counter.VertexCount(); ++v) {
        double rank = counter.GetRank(v);
        EXPECT_TRUE(std::isfinite(rank)) << "vertex " << v << " rank is not finite";
        EXPECT_GE(rank, 0.0) << "vertex " << v << " rank is negative";
    }

    CleanupArtifacts(4);
    std::remove(path.c_str());
}

TEST(Run, ConsistentAcrossThreadCounts) {
    // Результат не должен зависеть от числа потоков (детерминированность).
    std::string body;
    constexpr int kEdges = 400;
    for (int i = 0; i < kEdges; ++i) {
        body += std::to_string(i % 30) + " " + std::to_string((i * 7 + 3) % 30) + "\n";
    }

    auto path1 = WriteInputFile("threads_1", body);
    LeaderRankCounter counter1(path1, "/tmp/leaderrank_out_t1.csv",
                                /*threads=*/1, /*eps=*/1e-9, /*max_iters=*/300);
    counter1.Run();
    std::vector<double> ranks_1thread;
    for (size_t v = 0; v < counter1.VertexCount(); ++v) {
        ranks_1thread.push_back(counter1.GetRank(v));
    }
    CleanupArtifacts(1);
    std::remove(path1.c_str());

    auto path8 = WriteInputFile("threads_8", body);
    LeaderRankCounter counter8(path8, "/tmp/leaderrank_out_t8.csv",
                                /*threads=*/8, /*eps=*/1e-9, /*max_iters=*/300);
    counter8.Run();
    std::vector<double> ranks_8threads;
    for (size_t v = 0; v < counter8.VertexCount(); ++v) {
        ranks_8threads.push_back(counter8.GetRank(v));
    }
    CleanupArtifacts(8);
    std::remove(path8.c_str());

    ASSERT_EQ(ranks_1thread.size(), ranks_8threads.size());
    for (size_t v = 0; v < ranks_1thread.size(); ++v) {
        EXPECT_NEAR(ranks_1thread[v], ranks_8threads[v], 1e-9)
            << "mismatch at vertex " << v;
    }
}

TEST(Run, MatchesPaperReferenceValues) {
    // Числовой пример из оригинальной статьи Lü, Zhang, Yeung, Zhou,
    // "Leaders in Social Networks, the Delicious Case" (PLoS ONE, 2011),
    // рисунок с 6-вершинным графом, 12 направленных рёбер.
    // Вершины в статье пронумерованы 1..6, здесь сдвинуты на -1 под наш 0-based формат:
    //   paper: 1->2, 1->5, 2->3, 3->1, 3->4, 3->5, 4->2, 4->6, 5->2, 5->4, 5->6, 6->1
    //   here:  0->1, 0->4, 1->2, 2->0, 2->3, 2->4, 3->1, 3->5, 4->1, 4->3, 4->5, 5->0
    std::string body =
        "0 1\n0 4\n1 2\n2 0\n2 3\n2 4\n3 1\n3 5\n4 1\n4 3\n4 5\n5 0\n";
    auto path = WriteInputFile("paper_reference", body);

    LeaderRankCounter counter(path, "/tmp/leaderrank_out_paper.csv",
                               /*threads=*/2, /*eps=*/1e-9, /*max_iters=*/1000);
    counter.Run();

    constexpr double kExpected[6] = {1.0426, 1.1787, 0.9909, 0.8929, 0.9745, 0.9205};

    for (int v = 0; v < 6; ++v) {
        EXPECT_NEAR(counter.GetRank(v), kExpected[v], 1e-3)
            << "vertex " << (v + 1) << " (paper numbering) mismatch: "
            << "expected " << kExpected[v] << ", got " << counter.GetRank(v);
    }

    CleanupArtifacts(2);
    std::remove(path.c_str());
}

TEST(Run, ForestOfPaperGraphsStressTestsParallelism) {
    // 100 независимых копий эталонного графа из статьи (12 рёбер, 6 вершин
    // каждая), без рёбер между копиями. По симметрии (все копии стартуют
    // идентично и связаны только через общий ground node, чей вклад g/N
    // масштабируется ровно пропорционально числу копий) каждая копия должна
    // сойтись к ТЕМ ЖЕ САМЫМ S1..S6, что и одиночный граф — это даёт большой
    // объём данных (600 вершин, 1200 рёбер) с точно известным ответом,
    // хорошо нагружающий параллельную обработку по чанкам/потокам.
    constexpr int kCopies = 100;
    constexpr int kNodesPerCopy = 6;

    // paper: 1->2, 1->5, 2->3, 3->1, 3->4, 3->5, 4->2, 4->6, 5->2, 5->4, 5->6, 6->1
    // here (0-indexed within a copy): 0->1, 0->4, 1->2, 2->0, 2->3, 2->4,
    //                                 3->1, 3->5, 4->1, 4->3, 4->5, 5->0
    const std::vector<std::pair<int, int>> template_edges = {
        {0, 1}, {0, 4}, {1, 2}, {2, 0}, {2, 3}, {2, 4},
        {3, 1}, {3, 5}, {4, 1}, {4, 3}, {4, 5}, {5, 0},
    };

    std::string body;
    for (int copy = 0; copy < kCopies; ++copy) {
        int base = copy * kNodesPerCopy;
        for (auto& [u, v] : template_edges) {
            body += std::to_string(base + u) + " " + std::to_string(base + v) + "\n";
        }
    }
    auto path = WriteInputFile("forest_stress", body);

    LeaderRankCounter counter(path, "/tmp/leaderrank_out_forest.csv",
                               /*threads=*/8, /*eps=*/1e-9, /*max_iters=*/1000);
    counter.Run();

    constexpr double kExpected[6] = {1.0426, 1.1787, 0.9909, 0.8929, 0.9745, 0.9205};

    ASSERT_EQ(counter.VertexCount(), static_cast<size_t>(kCopies * kNodesPerCopy));

    for (int copy = 0; copy < kCopies; ++copy) {
        int base = copy * kNodesPerCopy;
        for (int k = 0; k < kNodesPerCopy; ++k) {
            EXPECT_NEAR(counter.GetRank(base + k), kExpected[k], 1e-3)
                << "copy " << copy << ", local vertex " << (k + 1)
                << " (paper numbering) mismatch";
        }
    }

    CleanupArtifacts(8);
    std::remove(path.c_str());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}