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
#include <fstream>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <system_error>

using namespace llvm;

struct CodeCoveragePass : public PassInfoMixin<CodeCoveragePass> {
  // Map to hold unique IDs for each basic block.
  std::map<const BasicBlock*, unsigned> BlockIDMap;

  // Structure to hold CFG information per function.
  struct CFGInfo {
    unsigned EntryID;
    std::vector<unsigned> ExitIDs;
    std::vector<unsigned> AllBlockIDs;  // Add this to store all block IDs
  };
  std::map<std::string, CFGInfo> FunctionCFG;

  // Get environment variable value with error checking
  std::string getEnvVar(const char* name) {
    const char* value = std::getenv(name);
    if (!value) {
      errs() << "Error: Environment variable " << name << " is not set\n";
      return "";
    }
    return std::string(value);
  }

  // Get next block ID with file locking
  unsigned getNextBlockID() {
    std::string counterFile = getEnvVar("BLOCK_COUNTER_FILE");
    if (counterFile.empty()) return 0;

    // Open the file for reading and writing
    int fd = open(counterFile.c_str(), O_RDWR | O_CREAT, 0666);
    if (fd == -1) {
      errs() << "Error: Cannot open counter file: " << counterFile << "\n";
      return 0;
    }

    // Create file lock structure
    struct flock fl;
    fl.l_type = F_WRLCK;    // Write lock
    fl.l_whence = SEEK_SET;  // From beginning of file
    fl.l_start = 0;          // Offset from l_whence
    fl.l_len = 0;           // Lock whole file
    fl.l_pid = getpid();    // Process ID

    // Acquire exclusive lock
    if (fcntl(fd, F_SETLKW, &fl) == -1) {
      errs() << "Error: Cannot lock counter file\n";
      close(fd);
      return 0;
    }

    // Read current value
    char buf[32];
    unsigned currentID = 0;
    ssize_t bytes_read = read(fd, buf, sizeof(buf) - 1);
    if (bytes_read > 0) {
      buf[bytes_read] = '\0';
      currentID = std::stoul(buf);
    }

    // Increment the counter
    currentID++;

    // Write back the incremented value
    std::string newVal = std::to_string(currentID);
    lseek(fd, 0, SEEK_SET);
    write(fd, newVal.c_str(), newVal.length());
    ftruncate(fd, newVal.length());  // Truncate file to new length

    // Release the lock
    fl.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &fl);

    // Close file
    close(fd);

    return currentID - 1;  // Return the ID we're using (not the next one)
  }

  // Append CFG information to the CFG file
  void appendCFGInfo(const Module &M) {
    std::string cfgFile = getEnvVar("CFG_FILE");
    if (cfgFile.empty()) return;

    std::error_code EC;
    
    // Open file in append mode
    raw_fd_ostream Out(cfgFile, EC, sys::fs::OF_Append | sys::fs::OF_Text);
    if (EC) {
      errs() << "Error opening CFG file for writing: " << EC.message() << "\n";
      return;
    }

    // Write module information
    Out << "    {\n";
    Out << "      \"module_name\": \"" << M.getName().str() << "\",\n";
    Out << "      \"functions\": [\n";
    
    bool firstFunc = true;
    for (auto &entry : FunctionCFG) {
      if (!firstFunc)
        Out << ",\n";
      firstFunc = false;
      Out << "        {\n";
      Out << "          \"name\": \"" << entry.first << "\",\n";
      Out << "          \"entry_block\": " << entry.second.EntryID << ",\n";
      Out << "          \"exit_blocks\": [";
      for (size_t i = 0; i < entry.second.ExitIDs.size(); ++i) {
        Out << entry.second.ExitIDs[i];
        if (i + 1 < entry.second.ExitIDs.size())
          Out << ", ";
      }
      Out << "],\n";
      Out << "          \"all_blocks\": [";
      for (size_t i = 0; i < entry.second.AllBlockIDs.size(); ++i) {
        Out << entry.second.AllBlockIDs[i];
        if (i + 1 < entry.second.AllBlockIDs.size())
          Out << ", ";
      }
      Out << "]\n        }";
    }
    Out << "\n      ]\n    }\n";
    
    Out.close();
  }

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    // 1. Traverse the module to assign unique IDs and collect CFG data.
    for (Function &F : M) {
      if (F.isDeclaration())
        continue;
      bool firstBlock = true;
      CFGInfo cfg;
      for (BasicBlock &BB : F) {
        // Get a new unique block ID with locking
        unsigned blockID = getNextBlockID();
        BlockIDMap[&BB] = blockID;
        
        if (firstBlock) {
          cfg.EntryID = blockID;
          firstBlock = false;
        }
        // Add block ID to the list of all blocks
        cfg.AllBlockIDs.push_back(blockID);
        // A block is considered an exit if its terminator is a return, resume, or unreachable.
        Instruction *TI = BB.getTerminator();
        if (isa<ReturnInst>(TI) || isa<ResumeInst>(TI) || isa<UnreachableInst>(TI))
          cfg.ExitIDs.push_back(blockID);
      }
      FunctionCFG[F.getName().str()] = cfg;
    }

    // 2. Declare the external helper function __coverage_push.
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

    // 4. Append the CFG information to the specified file
    appendCFGInfo(M);

    return PreservedAnalyses::none();
  }
};

// Registration for the new pass manager
llvm::PassPluginLibraryInfo getCodeCoveragePassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "CodeCoveragePass", LLVM_VERSION_STRING,
        [](PassBuilder &PB) {
            // Register for explicit pass pipeline use
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "code-coverage-pass") {
                        MPM.addPass(CodeCoveragePass());
                        return true;
                    }
                    return false;
                });

            // Register to run before any optimizations
            PB.registerOptimizerEarlyEPCallback(
                [](ModulePassManager &MPM, OptimizationLevel Level) {
                    MPM.addPass(CodeCoveragePass());
                });
        }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return getCodeCoveragePassPluginInfo();
}
