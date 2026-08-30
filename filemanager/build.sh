#!/bin/bash
set -e

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
BUILD_DIR="${SCRIPT_DIR}/build"
HOST_DIR="${HOME}/imx6ull_work/buildroot/output/host"
QMAKE="${HOST_DIR}/bin/qmake"

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

echo "=== Running qmake ==="
export SYSROOT="${HOST_DIR}/usr/arm-buildroot-linux-gnueabihf/sysroot"
"${QMAKE}" -o "${BUILD_DIR}/Makefile" "${SCRIPT_DIR}/filemanager.pro"

echo "=== Running make ==="
make -j$(nproc)

echo "=== Build complete ==="
echo "Binary: ${BUILD_DIR}/filemanager"