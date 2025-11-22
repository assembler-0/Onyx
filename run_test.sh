#!/bin/bash

# A script to transpile, compile, run, and verify an Onyx test case.

set -e # Exit immediately if a command exits with a non-zero status.

# --- Configuration ---
# Ensure we are running from the project root
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: This script must be run from the project root directory."
    exit 1
fi

TEST_NAME=$1
BUILD_DIR="build"
TEST_DIR="tests"
OXC_PATH="${BUILD_DIR}/oxc"

# Source, intermediate, and final paths
ONYX_SRC="${TEST_DIR}/${TEST_NAME}.ox"
C_OUT="${BUILD_DIR}/${TEST_DIR}/${TEST_NAME}.c"
EXECUTABLE="${BUILD_DIR}/${TEST_DIR}/${TEST_NAME}"
EXPECTED_OUTPUT="${TEST_DIR}/${TEST_NAME}.expected"
ACTUAL_OUTPUT="${BUILD_DIR}/${TEST_DIR}/${TEST_NAME}.actual"

# --- Pre-flight Checks ---
if [ -z "$TEST_NAME" ]; then
    echo "Usage: $0 <test_name>"
    exit 1
fi

if [ ! -f "$ONYX_SRC" ]; then
    echo "Error: Test source file not found at ${ONYX_SRC}"
    exit 1
fi

if [ ! -f "$OXC_PATH" ]; then
    echo "Error: Transpiler executable not found at ${OXC_PATH}. Please build the project first."
    exit 1
fi

# Create directories if they don't exist
mkdir -p "${BUILD_DIR}/${TEST_DIR}"

# --- Test Execution ---
echo "--- Running Test: ${TEST_NAME} ---"

# 1. Transpile Onyx to C
echo "[1/4] Transpiling ${ONYX_SRC}..."
${OXC_PATH} ${ONYX_SRC} -o ${C_OUT}
echo "    > Transpiled C code at ${C_OUT}"

# 2. Compile C code
echo "[2/4] Compiling ${C_OUT}..."
# Use clang, respecting environment variables if set
${CC:-clang} ${C_OUT} -o ${EXECUTABLE} -lm
echo "    > Compiled executable at ${EXECUTABLE}"

# 3. Run the executable
echo "[3/4] Running executable..."
${EXECUTABLE} > ${ACTUAL_OUTPUT}
echo "    > Execution complete. Output captured in ${ACTUAL_OUTPUT}"

# 4. Verify the output
if [ -f "$EXPECTED_OUTPUT" ]; then
    echo "[4/4] Verifying output..."
    diff -u "${EXPECTED_OUTPUT}" "${ACTUAL_OUTPUT}"
    echo "    > Success: Output matches expected result."
else
    echo "[4/4] No expected output file found. Skipping verification."
fi

echo "--- Test Passed: ${TEST_NAME} ---"
