CXX := clang++-19
CC := clang-19

# Get LLVM flags and libraries
LLVM_CXXFLAGS := $(shell llvm-config-19 --cxxflags)
LLVM_LDFLAGS := $(shell llvm-config-19 --ldflags)
LLVM_LIBS := $(shell llvm-config-19 --libs all)
LLVM_SYSTEM_LIBS := $(shell llvm-config-19 --system-libs)

# Combine LLVM flags
LLVM_FLAGS := $(LLVM_CXXFLAGS) $(LLVM_LDFLAGS) $(LLVM_LIBS) $(LLVM_SYSTEM_LIBS)

# Common flags
CXXFLAGS := -Wall -Wextra -fPIC
CFLAGS := -Wall -Wextra -fPIC

# Output files
PASS_LIB := libCodeCoveragePass.so
RUNTIME_OBJ := coverage_runtime.o

# Default target
all: $(PASS_LIB) $(RUNTIME_OBJ)

# Build the instrumentation pass
$(PASS_LIB): CodeCoveragePass.cpp
	$(CXX) $(CXXFLAGS) -shared $< -o $@ $(LLVM_FLAGS)

# Build the runtime
$(RUNTIME_OBJ): coverage_runtime.c
	$(CC) $(CFLAGS) -O0 -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(PASS_LIB) $(RUNTIME_OBJ)

.PHONY: all clean 