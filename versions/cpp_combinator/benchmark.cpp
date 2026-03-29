#include <iostream>
#include <fstream>
#include <chrono>
#include <sstream>
#include "lexer.cpp"

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input_file> <iterations>\n";
        return 1;
    }

    std::ifstream t(argv[1]);
    if (!t.is_open()) return 1;
    std::stringstream buffer;
    buffer << t.rdbuf();
    std::string source = buffer.str();

    int iterations = std::atoi(argv[2]);
    long tokens_count = 0;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        CloudPolLexer lexer(source);
        while (true) {
            Token tok = lexer.nextToken();
            if (tok.code == TokenCode::END_OF_FILE) break;
            tokens_count++;
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    long duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    std::cout << "Native Combinator (C++) Parsed " << tokens_count << " tokens in " << iterations << " iterations.\n";
    std::cout << "Time taken: " << duration_ms << " ms\n";

    return 0;
}
