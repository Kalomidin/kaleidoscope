// lexer.cpp
#include <string>
#include <iostream>
#include "lexer.hpp"

using namespace std;  // Safe to use in cpp files

// Define global variables here, to avoid multiple definitions
string IdentifierStr; 
double NumVal;
int CurTok;

// Helper functions
bool is_alpha(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool is_alnum(int c) {
    return is_alpha(c) || (c >= '0' && c <= '9');
}

// gettokn - Return the next token from standard input.
int gettokn() {
    static int LastChar = ' ';

    // Skip any whitespace.
    while (isspace(LastChar))
        LastChar = getchar();

    // Check if the character is an alphabet
    if (is_alpha(LastChar)) {  // identifier: [a-zA-Z][a-zA-Z0-9]*
        IdentifierStr = LastChar;
        while (is_alnum((LastChar = getchar())))
            IdentifierStr += LastChar;

        if (IdentifierStr == "def")
            return tok_def;
        if (IdentifierStr == "extern")
            return tok_extern;
        if (IdentifierStr == "done" and LastChar == ';')
            return tok_done;
        return tok_identifier;
    }

    // Check if the character is a number
    if (isdigit(LastChar) || LastChar == '.') {
        string NumStr;
        do {
            NumStr += LastChar;
            LastChar = getchar();
        } while (isdigit(LastChar) || LastChar == '.');

        NumVal = strtod(NumStr.c_str(), 0);
        return tok_number;
    }

    // Check if the character is a comment
    if (LastChar == '#') {
        // Comment until end of line.
        do
            LastChar = getchar();
        while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

        if (LastChar != EOF)
            return gettokn();
    }

    // Check for end of file. Don't eat the EOF.
    if (LastChar == EOF)
        return tok_eof;

    // Otherwise, just return the character as its ASCII value.
    int ThisChar = LastChar;
    LastChar = getchar();
    return ThisChar;
}

// Define getNextToken here instead of in the header
int getNextToken() {
    return CurTok = gettokn();
}