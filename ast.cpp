
#include <iostream>
#include "ast.hpp"


// LLVMContext is necessary for managing the LLVM context
std::unique_ptr<llvm::LLVMContext> TheContext;

// IRBuilder is used to build LLVM instructions
std::unique_ptr<llvm::IRBuilder<>> Builder;

// Module is the top-level container for code in LLVM
std::unique_ptr<llvm::Module> TheModule;

// NamedValues is used to store the values of variables
std::map<std::string, llvm::Value *> NamedValues;

// JIT serves as the interface to the JIT engine    
std::unique_ptr<llvm::orc::KaleidoscopeJIT> TheJIT;

// FPM is the FunctionPassManager, used to optimize the generated LLVM IR
std::unique_ptr<llvm::FunctionPassManager> TheFPM;

std::unique_ptr<llvm::LoopAnalysisManager> TheLAM;
std::unique_ptr<llvm::FunctionAnalysisManager> TheFAM;
std::unique_ptr<llvm::CGSCCAnalysisManager> TheCGAM;
std::unique_ptr<llvm::ModuleAnalysisManager> TheMAM;

std::unique_ptr<llvm::PassInstrumentationCallbacks> ThePIC;
std::unique_ptr<llvm::StandardInstrumentations> TheSI;
std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;

llvm::ExitOnError ExitOnErr;

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
        return (Function*)LogErrorV(("Function cannot be redefined: " + Proto->getName()).c_str());

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

        // optimize
        TheFPM->run(*TheFunction, *TheFAM);

        return TheFunction;
    }

    // delete the function.
    TheFunction->eraseFromParent();
    return nullptr;
}


void InitializeJIT() {
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();

    TheJIT = ExitOnErr(llvm::orc::KaleidoscopeJIT::Create());
}

void InitializeModule() {
    // Open a new context and module.
    TheContext = std::make_unique<LLVMContext>();
    TheModule = std::make_unique<Module>("my cool jit", *TheContext);
    TheModule->setDataLayout(TheJIT->getDataLayout());

    // Create a new builder for the module.
    Builder = std::make_unique<IRBuilder<>>(*TheContext);

    TheFPM = std::make_unique<FunctionPassManager>();
    TheLAM = std::make_unique<LoopAnalysisManager>();
    TheFAM = std::make_unique<FunctionAnalysisManager>();
    TheCGAM = std::make_unique<CGSCCAnalysisManager>();
    TheMAM = std::make_unique<ModuleAnalysisManager>();
    ThePIC = std::make_unique<PassInstrumentationCallbacks>();
    TheSI = std::make_unique<StandardInstrumentations>(*TheContext, /*DebugLogging*/ true);

    // InstCombinePass is a pass that combines instructions to reduce the number of instructions in the generated code.
    TheFPM->addPass(InstCombinePass());
    // ReassociatePass is a pass that reassociates expressions to improve performance.
    TheFPM->addPass(ReassociatePass());
    // GVNPass is a pass that performs global value numbering to optimize the generated code.
    TheFPM->addPass(GVNPass());
    // SimplifyCFGPass is a pass that simplifies the control flow graph to improve performance.
    TheFPM->addPass(SimplifyCFGPass());

    PassBuilder PB;
    PB.registerModuleAnalyses(*TheMAM);
    PB.registerFunctionAnalyses(*TheFAM);
    PB.crossRegisterProxies(*TheLAM, *TheFAM, *TheCGAM, *TheMAM);
}
