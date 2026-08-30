#!/bin/bash
# Cross-compile script for imx6ull OV5640 camera Qt5 application
# Buildroot must be built first with Qt5 enabled

set -e

# Buildroot output path
BUILDROOT_DIR="${HOME}/imx6ull_work/buildroot"
OUTPUT_DIR="${BUILDROOT_DIR}/output"
HOST_DIR="${OUTPUT_DIR}/host"
QMAKE="${HOST_DIR}/bin/qmake"
TARGET_DIR="${OUTPUT_DIR}/target"

# Toolchain
TOOLCHAIN_DIR="${HOST_DIR}/usr/bin/arm-buildroot-linux-gnueabihf"
SYSROOT="${HOST_DIR}/usr/arm-buildroot-linux-gnueabihf/sysroot"

if [ ! -x "${QMAKE}" ]; then
    echo "Error: qmake not found at ${QMAKE}"
    echo "Please build buildroot with Qt5 enabled first."
    exit 1
fi

BUILD_DIR="$(pwd)/build"

echo "=== Cleaning ==="
rm -rf "${BUILD_DIR}"

echo "=== Running qmake ==="
export SYSROOT="${HOST_DIR}/usr/arm-buildroot-linux-gnueabihf/sysroot"
"${QMAKE}" -o "${BUILD_DIR}/Makefile" "$(pwd)/camera.pro"

echo "=== Building ==="
make -C "${BUILD_DIR}" -j$(nproc)

echo ""
echo "=== Done ==="
echo "Binary: ${BUILD_DIR}/camera"
echo ""
echo "=== Deploy to NFS ==="
echo "   cp ${BUILD_DIR}/camera ~/nfs_rootfs/usr/bin/"
echo ""
echo "=== On target, first load modules ==="
echo "   modprobe ov5640_camera_mipi"
echo "   modprobe ov5640_camera"
echo "   modprobe mx6s_capture"
echo ""
echo "=== Run ==="
echo "   export TSLIB_TSDEVICE=/dev/input/event1"
echo "   export QT_QPA_FB_TSLIB=1"
echo "   ./camera -platform linuxfb"