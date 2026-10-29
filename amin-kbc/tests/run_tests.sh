#!/usr/bin/env bash
# Runs the portable core test suite natively (Linux/macOS) with g++ or clang++.
set -euo pipefail
cd "$(dirname "$0")/.."
CXX="${CXX:-g++}"
OUT="${TMPDIR:-/tmp}/amin_kbc_tests"
"$CXX" -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -finput-charset=UTF-8 \
    -o "$OUT" tests/test_logic.cpp src/core/*.cpp
"$OUT"
