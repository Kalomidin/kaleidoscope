#include "lexer.hpp"
#include "parser.hpp"
#include <iostream>

int main(int argc, char *argv[]) {
    InitializeJIT();
    InitializeModule();
    auto inputFile = argc > 1 ? std::string(argv[1]) : "";
    if (!inputFile.empty()) {
        std::cout << "Reading from file: " << inputFile << std::endl;
        readFile(inputFile);
    }
    MainLoop();
    if (!inputFile.empty()) {
        closeFile();
    }
    return 0;
}