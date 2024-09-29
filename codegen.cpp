#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace llvm;


static std::unique_ptr<LLVMContext> TheContext;
static std::unique_ptr<IRBuilder<>> Builder;
static std::unique_ptr<Module> TheModule;
static std::map<std::string, Value *> NamedValues;


class ExprAST {
public:
    virtual ~ExprAST() = default;
    // what is Value?
    // Static single assignment (SSA) -> every variable is assigned only once
    virtual Value *codegen() = 0;
};

class NumberExprAST: public ExprAST {
    double Val;
public:
    NumberExprAST(double Val): Val(Val) {}
    Value *codegen() override;
};