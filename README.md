# LLVM Code Coverage Instrumentation Pass

## Overview

This project implements an LLVM pass that instruments programs to collect execution path (array of basic block ids) and static control-flow graph (CFG) data. This instrumentation was originally developed for an [experimental fuzzer](https://github.com/fEst1ck/coverage-playground).

Our pass operates by:

- **Basic Block Instrumentation:**
  Each basic block in the program is assigned a globally unique 32-bit identifier. An instrumentation call is inserted at the beginning of every basic block. The call pushes the block's unique ID into a piece of shared memory. See `coverage_runtime.c:__coverage_push`.

- **CFG Data Collection:**
  In addition to runtime trace collection, the pass analyzes each function to record:
  - The **entry block** (the first basic block of the function).
  - The **exit blocks** (basic blocks that end with a `return`, `unreachable`, or similar terminator).

  This CFG information is output during compilation into a text file for external analysis.

## Get started

### Build

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

### Instrumentation

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

### Running Instrumented Programs

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

## The Collected Execution Trace

The instrumented program records its execution trace in a file based shared memory. The shared memory:

- starts with a 32-bit counter at offset 0 indicating the number of blocks executed, and
- followed by an array of 32-bit block IDs.

The shared memory file is `/tmp/coverage_shm.bin` by default. It can be set by environment variables:
- `COVERAGE_SHM_BASE`: Path to the base of the shared memory file.
- `FUZZER_ID`: ID of the fuzzer instance.

When `COVERAGE_SHM_BASE` is set, the shared memory file is `${COVERAGE_SHM_BASE}.bin`. If `FUZZER_ID` is also set, the shared memory file is `{COVERAGE_SHM_BASE}_{FUZZER_ID}.bin`.

## CFG Data
CFG information is collected in a text file that accumulates data from all compiled modules.
The CFG file is set by environment variable `CFG_FILE`. When using `path-clang`, the CFG file is set to `$HOME/.coverage_data/coverage.json` by default. It can be configured by:

- `COVERAGE_DIR`: Path to the directory where the coverage data is stored. The directory contains:
  - `block_counter.txt`: global block ID counter
  - `coverage.json`: the CFG data

- **Example CFG Data:**

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