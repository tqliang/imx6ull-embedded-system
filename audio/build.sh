#!/bin/bash
# Cross-compile script for imx6ull WM8960 audio Qt5 application
# Buildroot must be built first with Qt5 and Qt5Multimedia enabled

set -e

BUILDROOT_DIR="${HOME}/imx6ull_work/buildroot"
OUTPUT_DIR="${BUILDROOT_DIR}/output"
HOST_DIR="${OUTPUT_DIR}/host"
QMAKE="${HOST_DIR}/bin/qmake"

if [ ! -x "${QMAKE}" ]; then
    echo "Error: qmake not found at ${QMAKE}"
    echo "Please build buildroot with Qt5 and Qt5Multimedia enabled first."
    exit 1
fi

BUILD_DIR="$(pwd)/build"

echo "=== Cleaning ==="
rm -rf "${BUILD_DIR}"

echo "=== Running qmake ==="
"${QMAKE}" -o "${BUILD_DIR}/Makefile" "$(pwd)/audio.pro"

echo "=== Building ==="
make -C "${BUILD_DIR}" -j$(nproc)

echo ""
echo "=== Done ==="
echo "Binary: ${BUILD_DIR}/audio"
echo ""
echo "=== Deploy to NFS ==="
echo "   cp ${BUILD_DIR}/audio ~/nfs_rootfs/usr/bin/"
echo ""
echo "=== Run ==="
echo "   export QT_QPA_FB_TSLIB=1"
echo "   export TSLIB_TSDEVICE=/dev/input/event1"
echo "   ./audio -platform linuxfb"