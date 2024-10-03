
#include <iostream>
#include "ast.hpp"

using namespace llvm;

// NumberExprAST implementation
Value *NumberExprAST::codegen() {
    return ConstantFP::get(*TheContext, APFloat(Val));
}

// VariableExprAST implementation
Value *VariableExprAST::codegen() {
    // Look this variable up in the NamedValues map.
    Value *V = NamedValues[Name];
    return V ? V : ConstantFP::get(*TheContext, APFloat(0.0));
}

// BinaryExprAST implementation
Value *BinaryExprAST::codegen() {
    Value *L = LHS->codegen();
    Value *R = RHS->codegen();
    if (!L || !R) return nullptr;

    switch (Op) {
        case '+':
            return Builder->CreateFAdd(L, R, "addtmp");
        case '-':
            return Builder->CreateFSub(L, R, "subtmp");
        case '*':
            return Builder->CreateFMul(L, R, "multmp");
        case '/':
            return Builder->CreateFDiv(L, R, "divtmp");
        default:
            return LogErrorV("invalid binary operator");
    }
}

// CallExprAST implementation
Value *CallExprAST::codegen() {
    // Look up the name in the global module table.
    Function *CalleeF = TheModule->getFunction(Callee);
    if (!CalleeF)
        return LogErrorV("Unknown function referenced");

    // If argument mismatch error.
    if (CalleeF->arg_size() != Args.size())
        return LogErrorV("Incorrect # arguments passed");

    std::vector<Value *> ArgsV;
    for (unsigned i = 0, e = Args.size(); i != e; ++i) {
        ArgsV.push_back(Args[i]->codegen());
        if (!ArgsV.back())
            return nullptr;
    }

    return Builder->CreateCall(CalleeF, ArgsV, "calltmp");
}

// PrototypeAST implementation
Function *PrototypeAST::codegen() {
    if (!TheContext) {
        return (Function*)LogErrorV("TheContext is null");
    }

    std::vector<Type *> Doubles(Args.size(), Type::getDoubleTy(*TheContext));
    FunctionType *FT = FunctionType::get(Type::getDoubleTy(*TheContext), Doubles, false);

    Function *F = Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get());
    unsigned Idx = 0;
    for (auto &Arg : F->args())
        Arg.setName(Args[Idx++]);
    return F;
}

Function *FunctionAST::codegen() {

    // First, check for an existing function from a previous declaration.
    Function *TheFunction = TheModule->getFunction(Proto->getName());
    
    // if there is declaration, do nothing.
    // it could imported using extern.
    if (!TheFunction)
        TheFunction = Proto->codegen();

    // if there is no function, return nil.
    if (!TheFunction)
        return nullptr;

    Function *temp = TheModule->getFunction(Proto->getName());

    // if function body has already been generated, return err as we don't allow redefinition.
    if (!TheFunction->empty())
        return (Function*)LogErrorV("Function cannot be redefined.");

    // verify that the function args are the same 
    if (TheFunction->arg_size() != Proto->getArgs().size()) {
        return (Function*)LogErrorV("Unknown variable name.");
    }
    for (unsigned i = 0; i < TheFunction->arg_size(); i++) {
        if (TheFunction->getArg(i)->getName() != Proto->getArgs()[i]) {
            return (Function*)LogErrorV("Unknown variable name.");
        }
    }
    
    // Create a new basic block to start insertion into.
    BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", TheFunction);
    Builder->SetInsertPoint(BB);

    // Record the function arguments in the NamedValues map.
    NamedValues.clear();
    for (auto &Arg : TheFunction->args())
        NamedValues[std::string(Arg.getName())] = &Arg;

    if (Value *RetVal = Body->codegen()) {
        Builder->CreateRet(RetVal);

        // validate the generated code, check for consistency.
        verifyFunction(*TheFunction);

        return TheFunction;
    }

    // delete the function.
    TheFunction->eraseFromParent();
    return nullptr;
}

void InitializeModule() {
  // Open a new context and module.
  TheContext = std::make_unique<LLVMContext>();
  TheModule = std::make_unique<Module>("my cool jit", *TheContext);

  // Create a new builder for the module.
  Builder = std::make_unique<IRBuilder<>>(*TheContext);
}
