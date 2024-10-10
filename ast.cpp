
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
        case '<':
            L = Builder->CreateFCmpULT(L, R, "cmptmp");
            return Builder->CreateUIToFP(L, Type::getDoubleTy(*TheContext), "booltmp");
        case '>':
            L = Builder->CreateFCmpUGT(L, R, "cmptmp");
            return Builder->CreateUIToFP(L, Type::getDoubleTy(*TheContext), "booltmp");
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

Value *IfExprAST::codegen() {
    // evaluate the condition
    Value *CondV = Cond->codegen();
    if (!CondV) return nullptr;

    CondV = Builder->CreateFCmpONE(CondV, ConstantFP::get(*TheContext, APFloat(0.0)), "ifcond");
    Function *TheFunction = Builder->GetInsertBlock()->getParent();

    BasicBlock *ThenBB = BasicBlock::Create(*TheContext, "then", TheFunction);
    BasicBlock *ElseBB = BasicBlock::Create(*TheContext, "else");
    BasicBlock *MergeBB = BasicBlock::Create(*TheContext, "ifcont");

    Builder->CreateCondBr(CondV, ThenBB, ElseBB);

    Builder->SetInsertPoint(ThenBB);

    Value *ThenV = Then->codegen();
    if (!ThenV) return nullptr;

    Builder->CreateBr(MergeBB);
    ThenBB = Builder->GetInsertBlock();

    TheFunction->insert(TheFunction->end(), ElseBB);
    Builder->SetInsertPoint(ElseBB);

    Value *ElseV = Else->codegen();
    if (!ElseV) return nullptr;

    Builder->CreateBr(MergeBB);
    ElseBB = Builder->GetInsertBlock();

    TheFunction->insert(TheFunction->end(), MergeBB);
    Builder->SetInsertPoint(MergeBB);

    PHINode *PN = Builder->CreatePHI(Type::getDoubleTy(*TheContext), 2, "iftmp");

    PN->addIncoming(ThenV, ThenBB);
    PN->addIncoming(ElseV, ElseBB);
    return PN;
}

Value *ForExprAST::codegen() {
    // evaluate the start
    Value *StartVal = Start->codegen();
    if (!StartVal) return nullptr;
    auto oldVal = NamedValues[VarName];


    // create the basic block
    Function *TheFunction = Builder->GetInsertBlock()->getParent();
    BasicBlock *LoopBB = BasicBlock::Create(*TheContext, "loop", TheFunction);
    BasicBlock *StepBB = BasicBlock::Create(*TheContext, "loopstep");
    BasicBlock *BodyBB = BasicBlock::Create(*TheContext, "loopbody");
    BasicBlock *AfterBB = BasicBlock::Create(*TheContext, "loopafter");
    BasicBlock *CurBB = Builder->GetInsertBlock();
    Builder->CreateBr(LoopBB);

    // Evaluate the condition
    Builder->SetInsertPoint(LoopBB);

    PHINode *Variable = Builder->CreatePHI(Type::getDoubleTy(*TheContext), 2, VarName.c_str());
    Variable->addIncoming(StartVal, CurBB);

    // set the variable
    NamedValues[VarName] = Variable;

    Value *CondV = Cond->codegen();
    if (!CondV) return nullptr;
    CondV = Builder->CreateFCmpONE(CondV, ConstantFP::get(*TheContext, APFloat(0.0)), "loopcond");
    Builder->CreateCondBr(CondV, BodyBB, AfterBB);

    // Evaluate the body
    Builder->SetInsertPoint(BodyBB);
    if (!Body->codegen()) return nullptr;
    Builder->CreateBr(StepBB);
    // add the body block to the function
    TheFunction->insert(TheFunction->end(), BodyBB);

    // Evaluate the step
    Builder->SetInsertPoint(StepBB);
    Value *StepVal = Step->codegen();
    if (!StepVal) return nullptr;
    Value *NextVar = Builder->CreateFAdd(Variable, StepVal, "nextvar");
    Variable->addIncoming(NextVar, Builder->GetInsertBlock());

    Builder->CreateBr(LoopBB);
    TheFunction->insert(TheFunction->end(), StepBB);

    // Evaluate the after loop
    Builder->SetInsertPoint(AfterBB);
    if (oldVal) {
        NamedValues[VarName] = oldVal;
    } else {
        NamedValues.erase(VarName);
    }
    TheFunction->insert(TheFunction->end(), AfterBB);

    auto resp = Constant::getNullValue(Type::getDoubleTy(*TheContext));
    return resp;
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
