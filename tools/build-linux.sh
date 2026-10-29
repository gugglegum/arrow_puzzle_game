#!/usr/bin/env bash
# Configure, build and run the default headless suites. No sudo is performed.
set -euo pipefail

project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd -- "$project_root"

preset=linux
if [[ ${1:-} == --debug ]]; then
    preset=linux-debug
    shift
fi
if [[ ${1:-} == --help ]]; then
    echo 'Usage: tools/build-linux.sh [--debug] [additional cmake configure options]'
    echo 'Set CMAKE_BUILD_PARALLEL_LEVEL to override the default of two workers.'
    exit 0
fi
if [[ $(uname -s) != Linux ]]; then
    echo 'This helper requires Linux.' >&2
    exit 1
fi

cmake --preset "$preset" "$@"
cmake --build --preset "$preset" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-2}"
ctest --preset "$preset"
echo "Ready: $project_root/build/$preset/arrow_puzzle"
