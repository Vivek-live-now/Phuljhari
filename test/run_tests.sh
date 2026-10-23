#!/usr/bin/env bash
# ==============================================================================
# Project Phuljhari ✨ Comprehensive Native Test Suite Runner
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build_test"
TEST_PROTOCOL_BIN="${BUILD_DIR}/test_protocol"
TEST_RECEIVER_SM_BIN="${BUILD_DIR}/test_receiver_sm"
TEST_CONTROLLER_SM_BIN="${BUILD_DIR}/test_controller_sm"
TEST_ACCEPTANCE_BIN="${BUILD_DIR}/test_acceptance"

echo "--------------------------------------------------------"
echo "Building Phuljhari Native Test Suite..."
echo "Repo root: ${REPO_ROOT}"
echo "--------------------------------------------------------"

mkdir -p "${BUILD_DIR}"

if command -v clang++ >/dev/null 2>&1; then
    CXX=clang++
elif command -v g++ >/dev/null 2>&1; then
    CXX=g++
else
    echo "ERROR: Neither clang++ nor g++ found in PATH!" >&2
    exit 1
fi

echo "Compiling protocol tests with ${CXX}..."
${CXX} -std=c++17 \
    -Wall -Wextra -Werror \
    -I"${REPO_ROOT}/shared" \
    -I"${REPO_ROOT}/test" \
    "${REPO_ROOT}/test/test_protocol.cpp" \
    -o "${TEST_PROTOCOL_BIN}"

echo "Compiling receiver state machine tests with ${CXX}..."
${CXX} -std=c++17 \
    -Wall -Wextra -Werror \
    -I"${REPO_ROOT}/shared" \
    -I"${REPO_ROOT}/receiver/include" \
    -I"${REPO_ROOT}/receiver/src" \
    -I"${REPO_ROOT}/test" \
    "${REPO_ROOT}/receiver/src/receiver_state_machine.cpp" \
    "${REPO_ROOT}/test/test_receiver_sm.cpp" \
    -o "${TEST_RECEIVER_SM_BIN}"

echo "Compiling controller state machine tests with ${CXX}..."
${CXX} -std=c++17 \
    -Wall -Wextra -Werror \
    -I"${REPO_ROOT}/shared" \
    -I"${REPO_ROOT}/controller/include" \
    -I"${REPO_ROOT}/controller/src" \
    -I"${REPO_ROOT}/test" \
    "${REPO_ROOT}/controller/src/controller_state_machine.cpp" \
    "${REPO_ROOT}/test/test_controller_sm.cpp" \
    -o "${TEST_CONTROLLER_SM_BIN}"

echo "Compiling acceptance & integration tests with ${CXX}..."
${CXX} -std=c++17 \
    -Wall -Wextra -Werror \
    -I"${REPO_ROOT}/shared" \
    -I"${REPO_ROOT}/receiver/include" \
    -I"${REPO_ROOT}/receiver/src" \
    -I"${REPO_ROOT}/controller/include" \
    -I"${REPO_ROOT}/controller/src" \
    -I"${REPO_ROOT}/test" \
    "${REPO_ROOT}/receiver/src/receiver_state_machine.cpp" \
    "${REPO_ROOT}/controller/src/controller_state_machine.cpp" \
    "${REPO_ROOT}/test/test_acceptance.cpp" \
    -o "${TEST_ACCEPTANCE_BIN}"

echo "Compilation successful. Executing tests..."
echo "--------------------------------------------------------"
"${TEST_PROTOCOL_BIN}"
"${TEST_RECEIVER_SM_BIN}"
"${TEST_CONTROLLER_SM_BIN}"
"${TEST_ACCEPTANCE_BIN}"

echo "--------------------------------------------------------"
echo "All 4 native test suites PASSED successfully."
echo "--------------------------------------------------------"
exit 0
