#!/usr/bin/env bash
set -euo pipefail

if [[ $# -gt 1 || ( $# -eq 1 && "$1" != "--build-only" ) ]]; then
    echo "用法: $0 [--build-only]" >&2
    exit 2
fi

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
qt_prefix="${QT_PREFIX_PATH:-${HOME}/Qt/6.8.3/macos}"
macos_sdk="${MACOS_SDK_PATH:-/Library/Developer/CommandLineTools/SDKs/MacOSX14.4.sdk}"
build_dir="${project_root}/build-qml-preview"

cmake -S "${project_root}/preview" -B "${build_dir}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_SYSROOT="${macos_sdk}" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
    -DQt6_DIR="${qt_prefix}/lib/cmake/Qt6"
cmake --build "${build_dir}" --parallel 4

if [[ "${1:-}" != "--build-only" ]]; then
    exec "${build_dir}/XiaoJiPreview.app/Contents/MacOS/XiaoJiPreview"
fi
