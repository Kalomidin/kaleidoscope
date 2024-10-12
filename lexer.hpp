// lexer.hpp
#ifndef LEXER_HPP
#define LEXER_HPP
#include "ast.hpp"
#include <string>

// Declare external variables to avoid multiple definitions
extern std::string IdentifierStr; 
extern double NumVal;
extern int CurTok;

// The lexer returns tokens [0-255] if it is an unknown character, otherwise one
// of these for known things.
enum Token {
    tok_eof = -1,
    tok_def = -2,
    tok_extern = -3,
    tok_identifier = -4,
    tok_number = -5,
    tok_close = -6,
    tok_if = -7,
    tok_then = -8,
    tok_else = -9,
    tok_for = -10,
    tok_in = -11,
};


// Declare functions
int gettokn();
int getNextToken();
void readFile(const std::string &filename);
void closeFile();
bool isFileSet();

#endif // LEXER_HPP
