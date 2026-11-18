#!/usr/bin/env bash
# The one command that decides whether a change is ready: CI runs exactly this.
# Optimized with debug info: whisper.cpp unoptimized makes the inference tests several times slower.
set -euo pipefail

cd "$(dirname "$0")/.."
llvm_bin="$(brew --prefix llvm)/bin"

cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo "$@"
cmake --build build --parallel
ctest --test-dir build --output-on-failure

sources=$(git ls-files --cached --others --exclude-standard '*.cpp' '*.h' | grep -v '^third_party/')
"$llvm_bin/clang-format" --dry-run --Werror $sources
"$llvm_bin/clang-tidy" -p build --quiet $(echo "$sources" | grep '\.cpp$')
