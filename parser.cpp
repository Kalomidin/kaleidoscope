using namespace std;

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <iostream>

#include "ast.hpp"
#include "lexer.hpp"

static unique_ptr<ExprAST> ParsePrimary();
static unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, unique_ptr<ExprAST> LHS);
static unique_ptr<ExprAST> ParseExpression();

/// LogError* - These are little helper functions for error handling.
std::unique_ptr<ExprAST> LogError(const char *Str) {
  fprintf(stderr, "Error: %s\n", Str);
  return nullptr;
}
std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
  LogError(Str);
  return nullptr;
}


// + 3 5 -> this returns ExprAST(3)
static unique_ptr<ExprAST> ParseNumberExpr() {
  auto Result = make_unique<NumberExprAST>(NumVal);
  getNextToken(); // consume the number
  return std::move(Result);
}

// identifierexpr
static unique_ptr<ExprAST> ParseIdentifierExpr() {
    string idName = IdentifierStr;
    getNextToken(); // eat identifier
    // if it is not a function call
    if (CurTok != '(') {
        return make_unique<VariableExprAST>(idName);
    }
    // if it is a function call
    getNextToken(); // eat (
    vector<unique_ptr<ExprAST>> Args;
    if (CurTok != ')') {
        while(true) {
            if (auto Arg = ParseExpression()) {
                Args.push_back(std::move(Arg));
            } else {
                return nullptr;
            }
            if (CurTok == ')') break;
            if (CurTok != ',') {
                return LogError("Expected ')' or ',' in argument list");
            }
            getNextToken();
        }
    }
    getNextToken(); // eat )
    return make_unique<CallExprAST>(idName, std::move(Args));
}

// parenexpr ::= '(' expression ')'
// expression
// (a+b)
// (a)
static unique_ptr<ExprAST> ParseParentExpr() {
    getNextToken(); // eat (.
    auto v = ParseExpression();
    if (!v) return nullptr;
    getNextToken(); // eat ).
    return v;
}

// we only support +, - , * and /
static unique_ptr<ExprAST> ParseExpression() {
    auto LHS = ParsePrimary();
    if (!LHS) return nullptr;
    return ParseBinOpRHS(0, std::move(LHS));
}

static int getTokPrecedence() {
    switch(CurTok) {
    case '+':
        return 1;
    case '-':
        return 1;
    case '*':
        return 2;
    case '/':
        return 2;
    default:
        return -1;
    }
}

// primary
static unique_ptr<ExprAST> ParsePrimary() {
    switch (CurTok) {
    case tok_number:
        return ParseNumberExpr();
    case tok_identifier:
        return ParseIdentifierExpr();
    case '(':
        return ParseParentExpr();
        // TODO: Add support for parsing prototype and identifier
    default:
        return LogError("unknown token when expecting an expression");
    }
}

static unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, unique_ptr<ExprAST> LHS) {
    // Does it fail in expressions like a+b# as # is not a valid token
    // the reason it is while loop is because expr could be as a+b*c*d
    while(true) {
        int precedence = getTokPrecedence();
        if (precedence < ExprPrec) {
            return LHS;
        }
        int BinOp = CurTok;
        getNextToken(); // eat binop
        auto RHS = ParsePrimary();
        if (!RHS) return nullptr;
        int NextPrec = getTokPrecedence();
        if (precedence < NextPrec) {
            RHS = ParseBinOpRHS(precedence + 1, std::move(RHS));
            if (!RHS) return nullptr;
        }
        LHS = make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
    }
}

// parse prototype
static unique_ptr<PrototypeAST> ParsePrototype() {
    if (CurTok != tok_identifier)
        return LogErrorP("Expected function name in prototype");
    string FnName = IdentifierStr;
    getNextToken();

    if (CurTok != '(')
        return LogErrorP("Expected '(' in prototype");

    // TODO: Do not allow duplicate arguments
    vector<string> ArgNames;
    while (getNextToken() == tok_identifier)
        ArgNames.push_back(IdentifierStr);
    if (CurTok != ')')
        return LogErrorP("Expected ')' in prototype, got " + CurTok);

    getNextToken(); // eat )
    return make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}

// parse definition
static unique_ptr<FunctionAST> ParseDefinition() {
    getNextToken(); // eat def.
    auto Proto = ParsePrototype();
    if (!Proto) return nullptr;

    if (auto E = ParseExpression())
        return make_unique<FunctionAST>(std::move(Proto), std::move(E));
    return nullptr;
}

// parse extern
static unique_ptr<PrototypeAST> ParseExtern() {
    getNextToken(); // eat extern.
    return ParsePrototype();
}


static void HandleDefinition() {
    if (auto FnAST = ParseDefinition()) {
        fprintf(stderr, "Parsed a function definition.\n");
    } else {
        // Skip token for error recovery.
        getNextToken();
    }
}

static void HandleExtern() {
    if (auto ProtoAST = ParseExtern()) {
        fprintf(stderr, "Parsed an extern\n");
    } else {
        // Skip token for error recovery.
        getNextToken();
    }
}

void MainLoop() {
    while(true) {
        fprintf(stderr, "ready>");
        // cout << "Hello\n";
        switch(CurTok) {
            case tok_eof:
                return;
            case ';':
                getNextToken();
                break;
            case tok_def:
                HandleDefinition();
                break;
            case tok_extern:
                HandleExtern();
                break;
            default:
                cout << "Error: Unknown token " << (char)CurTok << "... failed to create AST\n";
                getNextToken();
                break;
        }
    }
}