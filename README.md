# LLVM Code Coverage Instrumentation Pass

## Overview

This project implements an LLVM pass that instruments programs to collect runtime code coverage information and static control-flow graph (CFG) data. This instrumentation was originally developed for an [experimental fuzzer](https://github.com/fEst1ck/coverage-playground).

Our pass operates by:

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
  The pass writes CFG information into a file of a sequence of JSON objects (default: `$HOME/.coverage_data/coverage.json`, not really JSON though).

- **Example:**

```json
{
  "module_name": "example1.c",
  "functions": [
    {
      "name": "foo",
      "entry_block": 42,
      "exit_blocks": [43, 44]
    }
  ]
}
{
  "module_name": "example2.c",
  "functions": [
    {
      "name": "bar",
      "entry_block": 1,
      "exit_blocks": [1]
    }
  ]
}
```

## Compilation and Usage

### 1. Build the Components

The project includes a Makefile to build the instrumentation pass, runtime, and utilities:

```bash
# Build everything
make

# Or build components individually
make libCodeCoveragePass.so  # Just the pass
make coverage_runtime.o      # Just the runtime
make init_shm                # Shared memory initialization utility
make read_shm                # Shared memory reading utility

# Clean build artifacts
make clean
```

### 2. Using the Instrumentation Scripts

Simply set the environment variables and run `make`:
```bash
export CC=/path/to/path-clang
export CXX=/path/to/path-clang++
make
```

Or use directly:
```bash
./path-clang -O2 -c example.c -o example.o
```

### 3. Running Instrumented Programs

We provide utility programs for printing the collected execution trace. This is an easy way to check if a program is properly instrumented.

1. **Initialize Shared Memory:**
   ```bash
   ./init_shm
   ```
   This creates a shared memory segment for storing the execution trace.

2. **Run Your Instrumented Program:**
   ```bash
   ./your_program [args...]
   ```
   The program will record basic block execution in the shared memory.

3. **Read the Execution Trace:**
   ```bash
   ./read_shm
   ```
   This displays the sequence of executed basic blocks.

### Environment Variables

The instrumentation uses these environment variables (automatically set by the scripts):
- `BLOCK_COUNTER_FILE`: Path to the global block ID counter
- `CFG_FILE`: Path to the CFG JSON file

Default location: `$HOME/.coverage_data/`
