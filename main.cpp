#include "src/Transpiler.hpp"
#include <iostream>
#include <string>

#define OX_IMPLEMENTATION_VERSION "0.0.1"
#define OX_IMPLEMENTATION_DATE __DATE__ " " __TIME__

void printUsage(const std::string& programName = "oxc") {
    std::cout << "usage: " + programName + " <input.ox> [-o <output.c>]\n";
}

void printVersion() {
    std::cout << OX_IMPLEMENTATION_VERSION << "\n";
}

int main(const int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    const std::string inputPath = argv[1];
    std::string outputPath = "out.c";

    for (int i = 0; i < argc; ++i) {
        if (std::string arg = argv[i]; arg == "-o" && i + 1 < argc)
            outputPath = argv[++i];
        else if (arg == "-v") {
            printVersion();
            return 0;
        }
    }

    onyx::TranspilerConfig config;
    config.verbose = true;

    if (onyx::Transpiler transpiler(config);
        transpiler.processFile(inputPath, outputPath)
    ) {
        return 0;
    }

    std::cerr << "failed to transpile - " << inputPath << "\n";
    return 1;
}