// большинство тестов писали ллм

#include <io/mmap_file.h>
#include <gtest/gtest.h>

#include <cstdio>
#include <thread>
#include <vector>

using leaderrank::MMapFile;

namespace {

// Уникальное имя файла на тест, чтобы тесты не топтали друг друга при
// параллельном запуске (ctest -j).
std::string TmpPath(const std::string& name) {
    return "/tmp/mmap_file_test_" + name + ".bin";
}

}  // namespace

TEST(MMapFile, WriteThenReadRoundtrip) {
    auto path = TmpPath("roundtrip");
    {
        MMapFile f(path, sizeof(uint32_t) * 4);
        f.Write<uint32_t>(0, 111);
        f.Write<uint32_t>(4, 222);
        f.Write<uint32_t>(8, 333);
        f.Write<uint32_t>(12, 444);
    }  // деструктор закрывает mmap, дальше читаем как будто с чистого листа

    MMapFile f(path);
    EXPECT_EQ(f.GetSize(), sizeof(uint32_t) * 4);
    EXPECT_EQ(f.Read<uint32_t>(0), 111u);
    EXPECT_EQ(f.Read<uint32_t>(4), 222u);
    EXPECT_EQ(f.Read<uint32_t>(8), 333u);
    EXPECT_EQ(f.Read<uint32_t>(12), 444u);

    std::remove(path.c_str());
}

TEST(MMapFile, ZeroInitializedOnCreate) {
    auto path = TmpPath("zero_init");
    MMapFile f(path, sizeof(float) * 10);
    for (size_t i = 0; i < 10; ++i) {
        EXPECT_FLOAT_EQ(f.Read<float>(i * sizeof(float)), 0.0f);
    }
    std::remove(path.c_str());
}

TEST(MMapFile, OpenNonexistentFileThrows) {
    EXPECT_THROW(MMapFile("/tmp/definitely_does_not_exist_leaderrank.bin"), std::runtime_error);
}

TEST(MMapFile, ConcurrentAddIsRace_Free) {
    auto path = TmpPath("concurrent_add");
    MMapFile f(path, sizeof(float));

    constexpr int kThreads = 8;
    constexpr int kIncrementsPerThread = 100000;

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&f] {
            for (int i = 0; i < kIncrementsPerThread; ++i) {
                f.Add<float>(0, 1.0f);
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }

    // Если бы Add был не атомарным, тут почти гарантированно была бы
    // потеря инкрементов из-за гонки (read-modify-write не сериализован).
    float expected = static_cast<float>(kThreads) * kIncrementsPerThread;
    EXPECT_FLOAT_EQ(f.Read<float>(0), expected);

    std::remove(path.c_str());
}

TEST(MMapFile, ConcurrentAddIntegral) {
    auto path = TmpPath("concurrent_add_int");
    MMapFile f(path, sizeof(uint32_t));

    constexpr int kThreads = 8;
    constexpr int kIncrementsPerThread = 100000;

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&f] {
            for (int i = 0; i < kIncrementsPerThread; ++i) {
                f.Add<uint32_t>(0, 1u);
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(f.Read<uint32_t>(0), static_cast<uint32_t>(kThreads * kIncrementsPerThread));

    std::remove(path.c_str());
}

TEST(MMapFile, MultipleFieldsIndependentUnderConcurrency) {
    // Проверяем, что запись в разные offset'ы не мешает друг другу
    // (это ровно паттерн x_new[j] += ... для разных j одновременно).
    auto path = TmpPath("independent_slots");
    constexpr size_t kSlots = 16;
    MMapFile f(path, sizeof(float) * kSlots);

    std::vector<std::thread> threads;
    for (size_t slot = 0; slot < kSlots; ++slot) {
        threads.emplace_back([&f, slot] {
            for (int i = 0; i < 1000; ++i) {
                f.Add<float>(slot * sizeof(float), 1.0f);
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }

    for (size_t slot = 0; slot < kSlots; ++slot) {
        EXPECT_FLOAT_EQ(f.Read<float>(slot * sizeof(float)), 1000.0f);
    }

    std::remove(path.c_str());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();
    return result;
}
