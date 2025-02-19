// CodeCoveragePass.cpp
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Passes/PassBuilder.h"
#include <map>
#include <vector>
#include <string>

using namespace llvm;

struct CodeCoveragePass : public PassInfoMixin<CodeCoveragePass> {
  // Map to hold unique IDs for each basic block.
  std::map<const BasicBlock*, unsigned> BlockIDMap;

  // Structure to hold CFG information per function.
  struct CFGInfo {
    unsigned EntryID;
    std::vector<unsigned> ExitIDs;
  };
  std::map<std::string, CFGInfo> FunctionCFG;

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    unsigned currentID = 0;

    // 1. Traverse the module to assign unique IDs and collect CFG data.
    for (Function &F : M) {
      if (F.isDeclaration())
        continue;
      bool firstBlock = true;
      CFGInfo cfg;
      for (BasicBlock &BB : F) {
        BlockIDMap[&BB] = currentID;
        if (firstBlock) {
          cfg.EntryID = currentID;
          firstBlock = false;
        }
        // A block is considered an exit if its terminator is a return, resume, or unreachable.
        Instruction *TI = BB.getTerminator();
        if (isa<ReturnInst>(TI) || isa<ResumeInst>(TI) || isa<UnreachableInst>(TI))
          cfg.ExitIDs.push_back(currentID);
        currentID++;
      }
      FunctionCFG[F.getName().str()] = cfg;
    }

    // 2. Declare the external helper function __coverage_push.
    // The function signature is: void __coverage_push(uint32_t)
    LLVMContext &Ctx = M.getContext();
    Type *VoidTy = Type::getVoidTy(Ctx);
    Type *Int32Ty = Type::getInt32Ty(Ctx);
    FunctionType *PushFuncTy = FunctionType::get(VoidTy, {Int32Ty}, false);
    Function *PushFunc = cast<Function>(
      M.getOrInsertFunction("__coverage_push", PushFuncTy).getCallee());
    PushFunc->setCallingConv(CallingConv::C);

    // 3. Insert a call to __coverage_push at the beginning of every basic block.
    for (Function &F : M) {
      if (F.isDeclaration())
        continue;
      for (BasicBlock &BB : F) {
        IRBuilder<> Builder(&*BB.getFirstInsertionPt());
        unsigned bbID = BlockIDMap[&BB];
        Value *IDVal = ConstantInt::get(Int32Ty, bbID);
        Builder.CreateCall(PushFunc, {IDVal});
      }
    }

    // 4. Write the collected CFG information to an external file (cfg.json).
    std::error_code EC;
    raw_fd_ostream Out("cfg.json", EC, sys::fs::OF_Text);
    if (EC) {
      errs() << "Error opening cfg.json for writing: " << EC.message() << "\n";
    } else {
      Out << "{\n  \"functions\": [\n";
      bool firstFunc = true;
      for (auto &entry : FunctionCFG) {
        if (!firstFunc)
          Out << ",\n";
        firstFunc = false;
        Out << "    {\n";
        Out << "      \"name\": \"" << entry.first << "\",\n";
        Out << "      \"entry_block\": " << entry.second.EntryID << ",\n";
        Out << "      \"exit_blocks\": [";
        for (size_t i = 0; i < entry.second.ExitIDs.size(); ++i) {
          Out << entry.second.ExitIDs[i];
          if (i + 1 < entry.second.ExitIDs.size())
            Out << ", ";
        }
        Out << "]\n    }";
      }
      Out << "\n  ]\n}\n";
      Out.close();
    }

    return PreservedAnalyses::none();
  }
};

// Registration for the new pass manager.
llvm::PassPluginLibraryInfo getCodeCoveragePassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "CodeCoveragePass", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "code-coverage-pass") {
                    MPM.addPass(CodeCoveragePass());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getCodeCoveragePassPluginInfo();
}
