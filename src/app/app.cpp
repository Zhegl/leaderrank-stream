#include <leaderrank/leaderrank.h>
#include <getopt.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace {

struct Args {
    std::string input;
    std::string output;
    size_t threads = std::thread::hardware_concurrency();
    double eps = 1e-6;
    size_t max_iters = 100;
};

void PrintUsage(const char* prog) {
    std::cerr << "Usage: " << prog
              << " --input <path> --output <path>"
                 " [--threads N] [--eps E] [--max-iters N]\n";
}

Args ParseArgs(int argc, char** argv) {
    Args args;
    bool has_input = false;
    bool has_output = false;

    static struct option long_options[] = {
        {"input", required_argument, nullptr, 'i'},
        {"output", required_argument, nullptr, 'o'},
        {"threads", required_argument, nullptr, 't'},
        {"eps", required_argument, nullptr, 'e'},
        {"max-iters", required_argument, nullptr, 'm'},
        {nullptr, 0, nullptr, 0},
    };

    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "i:o:t:e:m:", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'i':
                args.input = optarg;
                has_input = true;
                break;
            case 'o':
                args.output = optarg;
                has_output = true;
                break;
            case 't':
                args.threads = std::stoul(optarg);
                break;
            case 'e':
                args.eps = std::stod(optarg);
                break;
            case 'm':
                args.max_iters = std::stoul(optarg);
                break;
            default:
                PrintUsage(argv[0]);
                std::exit(EXIT_FAILURE);
        }
    }

    if (!has_input || !has_output) {
        std::cerr << "Error: --input and --output are required\n";
        PrintUsage(argv[0]);
        std::exit(EXIT_FAILURE);
    }

    return args;
}

}  // namespace

int main(int argc, char** argv) {
    Args args = ParseArgs(argc, argv);

    try {
        leaderrank::LeaderRankCounter counter(args.input, args.output, args.threads, args.eps,
                                              args.max_iters);
        counter.Run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
