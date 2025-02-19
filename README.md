# LLVM Code Coverage Instrumentation Pass

## Overview

This project implements an LLVM pass that instruments programs to collect runtime code coverage information and static control-flow graph (CFG) data. The instrumentation pass operates by:

- **Basic Block Instrumentation:**  
  Each basic block in the program is assigned a unique 32-bit identifier. At runtime, an instrumentation call is inserted at the beginning of every basic block. This call (to the helper function `__coverage_push`) pushes the block’s unique ID into a shared memory trace.

- **CFG Data Collection:**  
  In addition to runtime trace collection, the pass analyzes each function to record:
  - The **entry block** (the first basic block of the function).
  - The **exit blocks** (basic blocks that end with a `return`, `unreachable`, or similar terminator).

  This CFG information is output during compilation into a JSON file for external analysis.

## Runtime Trace and CFG Data

### Shared Memory Trace

- **Shared Memory Layout:**  
  The instrumentation runtime expects a fixed-size shared memory region (4KB). The layout is as follows:
  - **Index 0:** Contains the trace length (i.e., the number of basic block IDs recorded).
  - **Indexes 1 to N:** Contain the sequence of executed basic block IDs in the order they were encountered.

- **Thread Safety:**  
  The push operation is implemented using atomic operations (via a C helper function) so that the trace remains consistent even in a multi-threaded environment.

### CFG Data Output

- **Output File:**  
  The pass writes CFG information into a JSON file named `cfg.json`.

- **JSON Format Example:**

```json
{
  "functions": [
    {
      "name": "foo",
      "entry_block": 42,
      "exit_blocks": [43, 44]
    },
    {
      "name": "bar",
      "entry_block": 100,
      "exit_blocks": [101]
    }
  ]
}
```

Each function is identified by its name, along with its entry block ID and a list of exit block IDs.

## Compilation and Usage

### 1. Build the Instrumentation Pass Plugin

The pass is implemented in `CodeCoveragePass.cpp`. To compile it as a shared library (using LLVM 19), run:

```bash
clang++-19 -fPIC -shared CodeCoveragePass.cpp -o libCodeCoveragePass.so `llvm-config-19 --cxxflags --ldflags --system-libs --libs all`
```

### 2. Build the Runtime Support Library

The runtime helper (implemented in `coverage_runtime.c`) provides the `__coverage_push(uint32_t block_id)` function. Compile it as follows:

```bash
clang-19 -O0 -c coverage_runtime.c -o coverage_runtime.o
```

### 3. Compile Your Program with Instrumentation

To build and instrument your project without modifying its Makefile:

1. **Build the Instrumentation Components:**
   - **Instrumentation Pass Plugin:**  
     ```bash
     clang++-19 -fPIC -shared CodeCoveragePass.cpp -o libCodeCoveragePass.so `llvm-config-19 --cxxflags --ldflags --system-libs --libs all`
     ```
   - **Runtime Helper:**  
     ```bash
     clang-19 -O0 -c coverage_runtime.c -o coverage_runtime.o
     ```
2. **Set Environment Variables:**  
   In your shell, run:
   ```bash
   export CC=/full/path/to/clang_instrument.sh
   ```

### 4. How to Run Instrumented Programs and Get the Trace

Below is an example showing how to get the collected trace.

1. **Initialize Shared Memory:**  
   Run the provided utility (e.g., `init_shm`) to create and initialize a shared memory file (e.g., `/tmp/coverage_shm.bin`) of 4KB filled with zeros.

2. **Run the Instrumented Program:**  
   Execute `instrumented_program` with the required input. The runtime helper will map the shared memory file and record the execution trace.

3. **Retrieve the Trace:**  
   Run the provided trace reader utility (e.g., `read_shm`) to print the collected trace. The utility reads:
   - The trace length from index 0.
   - The subsequent basic block IDs from the shared memory region.

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
  The pass is implemented as a module-level pass using LLVM’s new pass manager. By subclassing `PassInfoMixin<YourPass>`, the pass implements:

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
  To output CFG information, the pass uses `raw_fd_ostream` to write a JSON file (`cfg.json`) containing each function’s entry block and exit blocks.

Understanding these LLVM concepts and APIs helps in grasping the design and implementation of our instrumentation pass, and offers insights into how similar passes can be developed and extended.