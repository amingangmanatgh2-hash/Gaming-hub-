#!/usr/bin/env bash
# Randomized robustness test (parsers, UTF-8/16, geometry) with AddressSanitizer
# and UndefinedBehaviorSanitizer. Requires g++ or clang++ with sanitizer support.
set -euo pipefail
cd "$(dirname "$0")/.."
CXX="${CXX:-g++}"
OUT="${TMPDIR:-/tmp}/amin_kbc_fuzz"
"$CXX" -std=c++17 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
    -finput-charset=UTF-8 -o "$OUT" tests/fuzz_core.cpp src/core/*.cpp
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1 "$OUT"
