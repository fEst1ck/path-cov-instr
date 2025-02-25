# LLVM Code Coverage Instrumentation Pass

## Overview

This project implements an LLVM pass that instruments programs to collect runtime code coverage information and static control-flow graph (CFG) data. The instrumentation pass operates by:

- **Basic Block Instrumentation:**  
  Each basic block in the program is assigned a unique 32-bit identifier. At runtime, an instrumentation call is inserted at the beginning of every basic block. This call (to the helper function `__coverage_push`) pushes the block's unique ID into a shared memory trace.

- **CFG Data Collection:**  
  In addition to runtime trace collection, the pass analyzes each function to record:
  - The **entry block** (the first basic block of the function).
  - The **exit blocks** (basic blocks that end with a `return`, `unreachable`, or similar terminator).

  This CFG information is output during compilation into a JSON file for external analysis.

## Runtime Trace and CFG Data

### Coverage Data Storage

- **Block Counter:**  
  A global counter file keeps track of basic block IDs to ensure uniqueness across multiple compilation units.

- **CFG Data:**  
  CFG information is collected in a JSON file that accumulates data from all compiled modules.

- **Thread Safety:**  
  File locking mechanisms ensure thread safety during parallel compilation.

### CFG Data Output

- **Output File:**  
  The pass writes CFG information into a JSON file (default: `$HOME/.coverage_data/coverage.json`).

- **JSON Format Example:**

```json
{
  "modules": [
    {
      "module_name": "example.c",
      "functions": [
        {
          "name": "foo",
          "entry_block": 42,
          "exit_blocks": [43, 44]
        }
      ]
    }
  ]
}
```

## Compilation and Usage

### 1. Build the Instrumentation Pass Plugin

The pass is implemented in `CodeCoveragePass.cpp`. To compile it as a shared library (using LLVM 19):

```bash
clang++ -fPIC -shared CodeCoveragePass.cpp -o libCodeCoveragePass.so `llvm-config --cxxflags --ldflags --system-libs --libs all`
```

### 2. Build the Runtime Support Library

Compile the runtime helper:

```bash
clang -O0 -c coverage_runtime.c -o coverage_runtime.o
```

### 3. Using the Instrumentation Scripts

Two wrapper scripts are provided for easy integration:

#### For C Projects (`path-clang`):
```bash
# Set as your C compiler
export CC=/path/to/path-clang

# Or use directly
./path-clang -O2 -c example.c -o example.o
```

#### For C++ Projects (`path-clang++`):
```bash
# Set as your C++ compiler
export CXX=/path/to/path-clang++

# Or use directly
./path-clang++ -O2 -c example.cpp -o example.o
```

Both scripts support:
- Individual file compilation (`-c`)
- Full program linking
- All standard compiler flags
- Parallel compilation (e.g., with `make -j`)

### Environment Variables

The instrumentation uses these environment variables (automatically set by the scripts):
- `BLOCK_COUNTER_FILE`: Path to the global block ID counter
- `CFG_FILE`: Path to the CFG JSON file

Default location: `$HOME/.coverage_data/`

## Appendix: Understanding LLVM IR and API Usage

### Structure of LLVM IR

LLVM IR is a low-level, platform-independent intermediate representation designed for high-level program analysis and optimization. Its hierarchical structure is as follows:

1. **Module:**  
   - The top-level container for LLVM IR.
   - Contains all functions, global variables, type declarations, and metadata.
   - Represents a complete translation unit or program.

2. **Function:**  
   - Represents a callable unit of code (similar to functions in C/C++).
   - Contains a signature (return type, parameters, calling convention) and a list of basic blocks.
   - The pass iterates over functions to instrument basic blocks and collect CFG data.

3. **Basic Block:**  
   - A sequence of instructions with a single entry and single exit.
   - Contains no internal branches; control can only enter at the beginning and exit at the end.
   - Our pass inserts an instrumentation call at the start of each basic block.

4. **Instruction:**  
   - The atomic operations within a basic block (arithmetic, memory access, control flow, etc.).
   - The pass does not modify these instructions but inserts new ones to record execution information.

### LLVM API Usage in the Instrumentation Pass

Our instrumentation pass leverages several LLVM APIs:

- **ModulePass / PassInfoMixin:**  
  The pass is implemented as a module-level pass using LLVM's new pass manager. By subclassing `PassInfoMixin<YourPass>`, the pass implements:

  ```cpp
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
  ```

  This method provides access to the entire module, enabling iteration over all functions.

- **Function and BasicBlock Iteration:**  
  The pass iterates over functions and their basic blocks:

  ```cpp
  for (Function &F : M) {
      if (F.isDeclaration()) continue;
      for (BasicBlock &BB : F) {
          // Assign a unique ID and insert instrumentation.
      }
  }
  ```

  This is used to assign unique IDs to basic blocks and to collect CFG data (entry and exit points).

- **IRBuilder:**  
  `IRBuilder<>` simplifies the creation and insertion of new instructions. In our pass, it is used to insert a call to `__coverage_push(uint32_t)` at the beginning of each basic block.

- **GlobalVariable:**  
  The pass declares or retrieves a global variable (e.g., `__coverage_shm`) representing the shared memory pointer. This is used by the inserted instrumentation code to update the execution trace.

- **Atomic Operations:**  
  Although our pass delegates atomic updates to a runtime helper, LLVM provides APIs (such as `CreateAtomicRMW`) for generating atomic instructions directly in IR if inline atomic operations are desired.

- **File I/O:**  
  To output CFG information, the pass uses `raw_fd_ostream` to write a JSON file (`cfg.json`) containing each function's entry block and exit blocks.

Understanding these LLVM concepts and APIs helps in grasping the design and implementation of our instrumentation pass, and offers insights into how similar passes can be developed and extended.