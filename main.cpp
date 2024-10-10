#include "lexer.hpp"
#include "parser.hpp"
#include <iostream>

int main() {
    InitializeJIT();
    InitializeModule();
    MainLoop();
    return 0;
}