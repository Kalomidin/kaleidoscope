using namespace std;

#include "lexer.hpp"
#include "parser.hpp"
#include <iostream> 

int main() {
  // Prime the first token.
  fprintf(stderr, "ready> ");
  getNextToken();

  // Run the main "interpreter loop" now.
  MainLoop();

  return 0;
}