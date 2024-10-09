using namespace std;

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
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
static unique_ptr<IfExprAST> ParseIf();

// + 3 5 -> this returns ExprAST(3)
static unique_ptr<ExprAST> ParseNumberExpr() {
  auto Result = make_unique<NumberExprAST>(NumVal);
  getNextToken(); // consume the number
  return move(Result);
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

// we only support +, - , *, /, <, >
static unique_ptr<ExprAST> ParseExpression() {
    auto LHS = ParsePrimary();
    if (!LHS) return nullptr;
    return ParseBinOpRHS(0, std::move(LHS));
}

static int getTokPrecedence() {
    switch(CurTok) {
    case '+':
        return 10;
    case '-':
        return 10;
    case '*':
        return 20;
    case '/':
        return 20;
    case '<':
        return 0;
    case '>':
        return 0;
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
    case tok_if:
        return ParseIf();
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
        return LogErrorP("Expected ')' in prototype, got " + char(CurTok));

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

// parse if
static unique_ptr<IfExprAST> ParseIf() {
    getNextToken();
    auto Cond = ParseExpression();
    if (!Cond) return nullptr;
    if (CurTok != tok_then) {
        LogError("Expected then");
        return nullptr;
    };
    getNextToken(); // eat then
    auto Then = ParseExpression();
    if (!Then) return nullptr;
    getNextToken(); // eat else
    auto Else = ParseExpression();
    if (!Else) return nullptr;
    return make_unique<IfExprAST>(std::move(Cond), std::move(Then), std::move(Else));
}


static void HandleDefinition() {
    if (auto FnAST = ParseDefinition()) {
        fprintf(stderr, "Parsed a function definition.\n");
        if (auto *FnIR = FnAST->codegen()) {
            fprintf(stderr, "Codegen success handle definition\n");
            FnIR->print(llvm::errs());
            fprintf(stderr, "\n");
        }
    }
}

static void HandleExtern() {
    if (auto ProtoAST = ParseExtern()) {
        fprintf(stderr, "Parsed an extern\n");
        if (auto *FnIR = ProtoAST->codegen()) {
            fprintf(stderr, "Codegen success handle extern\n");
            FnIR->print(llvm::errs());
            fprintf(stderr, "\n");
        }
    } else {
        // Skip token for error recovery.
        getNextToken();
    }
}

/// toplevelexpr ::= expression
static std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
  if (auto E = ParseExpression()) {
    // Make an anonymous proto.
    auto Proto = std::make_unique<PrototypeAST>("__anon_expr",
                                                 std::vector<std::string>());
    return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  }
  return nullptr;
}

static void HandleTopLevelExpression() {
    if (auto Expr = ParseTopLevelExpr()) {
        auto fnName = Expr->getProto()->getName();
        fprintf(stderr, "Parsed a top-level expression.\n");
        if (auto *FnIR = Expr->codegen()) {
            fprintf(stderr, "Codegen success handle top level expression\n");
            FnIR->print(llvm::errs());
            fprintf(stderr, "\n");
            // track the resource
            auto RT = TheJIT->getMainJITDylib().createResourceTracker();
            
            // Other option is to store all functions in a functionProto map
            // and then lookup for the function in the map
            // this will remove dependency to TheModule if only use case of TheModule is to store functions
            auto cpyTheModule = llvm::CloneModule(*TheModule.get());
            auto cpyTheContext = std::make_unique<llvm::LLVMContext>();

            // thread safe module
            // auto TSM = llvm::orc::ThreadSafeModule(move(TheModule), move(TheContext));
            auto TSM = llvm::orc::ThreadSafeModule(move(cpyTheModule), move(cpyTheContext));

            // add the module to the JIT
            ExitOnErr(TheJIT->addModule(std::move(TSM), RT));
            
            // search for the symbol
            auto ExprSymb = ExitOnErr(TheJIT->lookup(fnName));

            double (*FP)() = ExprSymb.getAddress().toPtr<double (*)()>();
            fprintf(stderr, "Evaluated to %f\n", FP());
            
            ExitOnErr(RT->remove());
            // remove the function from the module
            FnIR->eraseFromParent();
        }
    } else {
        // Skip token for error recovery.
        getNextToken();
    }
}

void MainLoop() {
    while(true) {
        // every time before get next token, print the prompt
        fprintf(stderr, "ready>");
        getNextToken();
        switch(CurTok) {
            case tok_eof:
                return;
            case ';':
                break;
            case tok_def:
                HandleDefinition();
                break;
            case tok_extern:
                HandleExtern();
                break;
            case tok_close:
                cout << "Close\n";
                return;
            default:
                HandleTopLevelExpression();
                break;
        }
    }
}