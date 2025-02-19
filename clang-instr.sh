#!/bin/bash
# clang_instrument.sh
#
# This wrapper intercepts clang invocations.
#
# When compiling (with "-c"), it calls clang-19 with:
#   -flto -fembed-bitcode
# so that object files contain LLVM IR bitcode.
#
# When linking (without "-c"), it assumes the object files are already LLVM IR bitcode.
# It links them using llvm-link-19, runs the instrumentation pass via opt-19,
# and finally compiles the instrumented bitcode into the final executable.
#
# Requirements:
#   - clang-19, llvm-link-19, and opt-19 must be in your PATH.
#   - The instrumentation pass plugin (libCodeCoveragePass.so) must be in the current directory.
#
# Usage:
#   Set CC and CXX to the full path of this script before building your project.

set -e

# If the invocation includes "-c", we're compiling a source file.
if [[ " $@ " =~ " -c " ]]; then
    # Compile with LTO and embed bitcode.
    exec clang-19 -flto -fembed-bitcode "$@"
fi

# Otherwise, we are in the linking phase.
output=""
object_files=()
args=("$@")
skip=0
for (( i=0; i < ${#args[@]}; i++ )); do
    if [ $skip -eq 1 ]; then
        skip=0
        continue
    fi
    if [[ "${args[$i]}" == "-o" ]]; then
        output="${args[$((i+1))]}"
        skip=1
    elif [[ "${args[$i]}" == *.o ]]; then
        object_files+=("${args[$i]}")
    fi
done

if [ -z "$output" ]; then
    echo "Error: No output file specified (use -o <file>)" >&2
    exit 1
fi

if [ ${#object_files[@]} -eq 0 ]; then
    echo "Error: No object files provided for linking" >&2
    exit 1
fi

echo "Linking object files: ${object_files[*]}"
echo "Final executable: $output"

# Create a temporary directory for intermediate files.
tmpdir=$(mktemp -d)
echo "Using temporary directory: $tmpdir"

# Since our object files are already LLVM IR bitcode,
# simply copy them to the temporary directory with a .bc extension.
for obj in "${object_files[@]}"; do
    base=$(basename "$obj" .o)
    outbc="$tmpdir/$base.bc"
    echo "Copying $obj to $outbc (already LLVM IR bitcode)..."
    cp "$obj" "$outbc"
done

# Link all bitcode files into one combined bitcode file.
echo "Linking bitcode files..."
llvm-link-19 "$tmpdir"/*.bc -o "$tmpdir/combined.bc"

# Run the instrumentation pass using opt-19.
echo "Running instrumentation pass..."
opt-19 -load-pass-plugin=./libCodeCoveragePass.so -passes=code-coverage-pass "$tmpdir/combined.bc" -o "$tmpdir/instrumented.bc"

# Compile the instrumented bitcode into the final executable.
echo "Compiling instrumented bitcode into executable..."
clang-19 -flto "$tmpdir/instrumented.bc" "coverage_runtime.o" -o "$output"

# Clean up temporary files.
rm -rf "$tmpdir"
echo "Instrumentation and linking complete. Executable: $output"

exit 0
