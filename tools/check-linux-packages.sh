#!/usr/bin/env bash
# Package and launch both modes. Add --appimage to verify all four Linux packages.
set -euo pipefail
project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd -- "$project_root"
version=${1:?Usage: check-linux-packages.sh VERSION [--appimage]}
shift
make_appimage=false
if [[ ${1:-} == --appimage ]]; then
  make_appimage=true
  shift
fi
[[ $# == 0 ]] || { echo 'Unexpected argument' >&2; exit 1; }
unpack_root=$(mktemp -d "${RUNNER_TEMP:-$project_root/build}/arrow-packages-XXXXXX")

for mode in user portable; do
  options=()
  suffix=''
  if [[ $mode == portable ]]; then
    options+=(--portable)
    suffix=-portable
  fi
  output="$project_root/out/linux-package/$mode"
  name="ArrowPuzzle-$version-linux-x64$suffix"
  archive="$output/$name.tar.gz"
  python3 tools/package-linux.py --version "$version" --output-dir "$output" "${options[@]}"
  (cd -- "$output" && sha256sum --check --ignore-missing SHA256SUMS.txt)
  unpacked="$unpack_root/$mode"
  mkdir -- "$unpacked"
  tar -xzf "$archive" -C "$unpacked" --strip-components=1
  cp "$output/SHA256SUMS.txt" "$unpacked/"
  (cd -- "$unpacked" && sha256sum --check --ignore-missing SHA256SUMS.txt)
  executable="$unpacked/ArrowPuzzle$suffix"
  python3 tools/check-production-binary.py "$executable" "${options[@]}"
  xvfb-run -a -s '-screen 0 1280x1024x24' python3 tools/smoke-linux.py \
    "$executable" "${options[@]}" --output-dir "out/linux-smoke/$mode"
  if [[ -n ${GITHUB_OUTPUT:-} ]]; then
    printf '%s_exe=%s\n%s_archive=%s\n' "$mode" "$executable" "$mode" "$archive" >> "$GITHUB_OUTPUT"
  fi
  if $make_appimage; then
    python3 tools/package-appimage.py "$archive"
    image="$output/$name.AppImage"
    (cd -- "$output" && sha256sum --check --ignore-missing SHA256SUMS.txt)
    xvfb-run -a -s '-screen 0 1280x1024x24' python3 tools/smoke-linux.py \
      "$image" --appimage "${options[@]}" --output-dir "out/appimage-smoke/$mode"
    if [[ -n ${GITHUB_OUTPUT:-} ]]; then
      printf '%s_appimage=%s\n' "$mode" "$image" >> "$GITHUB_OUTPUT"
    fi
  fi
done
