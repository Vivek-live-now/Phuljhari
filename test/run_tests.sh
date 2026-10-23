#!/usr/bin/env bash
# ==============================================================================
# Project Phuljhari ✨ Native Protocol & Safety Unit Test Runner
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build_test"
TEST_BIN="${BUILD_DIR}/test_protocol"

echo "--------------------------------------------------------"
echo "Building Phuljhari Native Test Suite..."
echo "Repo root: ${REPO_ROOT}"
echo "--------------------------------------------------------"

mkdir -p "${BUILD_DIR}"

# Choose compiler (clang++ or g++)
if command -v clang++ >/dev/null 2>&1; then
    CXX=clang++
elif command -v g++ >/dev/null 2>&1; then
    CXX=g++
else
    echo "ERROR: Neither clang++ nor g++ found in PATH!" >&2
    exit 1
fi

echo "Compiling with ${CXX}..."
${CXX} -std=c++17 \
    -Wall -Wextra -Werror \
    -I"${REPO_ROOT}/shared" \
    -I"${REPO_ROOT}/test" \
    "${REPO_ROOT}/test/test_protocol.cpp" \
    -o "${TEST_BIN}"

echo "Compilation successful. Executing tests..."
echo "--------------------------------------------------------"
"${TEST_BIN}"
TEST_EXIT_CODE=$?

echo "--------------------------------------------------------"
if [ ${TEST_EXIT_CODE} -eq 0 ]; then
    echo "All unit tests PASSED successfully."
else
    echo "Unit tests FAILED with exit code ${TEST_EXIT_CODE}."
fi
echo "--------------------------------------------------------"

exit ${TEST_EXIT_CODE}
