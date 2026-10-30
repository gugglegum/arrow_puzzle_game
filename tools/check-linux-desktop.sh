#!/usr/bin/env bash
# Exercise both storage modes; keep developer hooks confined to Debug binaries.
set -euo pipefail
project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd -- "$project_root"

python3 tools/check-production-binary.py build/linux/arrow_puzzle
python3 tools/check-production-binary.py build/linux/arrow_puzzle_portable --portable
cmake --build build/linux --parallel 2 --target arrow_render_tests arrow_ui_text_tests
xvfb-run -a -s '-screen 0 1280x1024x24' ./build/linux/arrow_render_tests
xvfb-run -a -s '-screen 0 1280x1024x24' ./build/linux/arrow_ui_text_tests out/localization-ui
cmake --preset linux-debug "$@" \
  -DFETCHCONTENT_SOURCE_DIR_RAYLIB="$project_root/build/linux/_deps/raylib-src"
cmake --build build/linux-debug --parallel 2 --target arrow_puzzle arrow_puzzle_portable
xvfb-run -a -s '-screen 0 1280x1024x24' python3 tools/smoke-linux.py \
  build/linux-debug/arrow_puzzle --debug-hooks --output-dir out/linux-qa-smoke/user
xvfb-run -a -s '-screen 0 1280x1024x24' python3 tools/smoke-linux.py \
  build/linux-debug/arrow_puzzle_portable --debug-hooks --portable --output-dir out/linux-qa-smoke/portable
