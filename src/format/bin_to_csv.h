#pragma once

#include <fstream>
#include <iomanip>
#include <string>
#include <io/mmap_file.h>

namespace leaderrank {

void ConvertBinToCsv(MMapFile& input, const std::string& output_path) {
    std::ofstream out(output_path, std::ios::binary);
    out << "vertex_id,rank\n";
    out << std::fixed << std::setprecision(15);

    size_t count = input.GetSizeFor<double>();
    for (size_t i = 0; i < count; ++i) {
        out << i << "," << input.Read<double>(i) << "\n";
    }
}
}  // namespace leaderrank