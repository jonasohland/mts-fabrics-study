#! /bin/sh

BUILD_TYPE="${1:-Debug}"
VCPKG_ROOT="${VCPKG_ROOT:-${HOME}/vcpkg}"

mkdir -vp "build/Linux-Clang-${BUILD_TYPE}"

cmake -S . -B "build/Linux-Clang-${BUILD_TYPE}" -G Ninja \
    --toolchain "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" \
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON" \
    "-DCMAKE_MINIMUM_REQUIRED_VERSION=3.23.0" \
    "-DCMAKE_CXX_COMPILER=$(llvm-config-19 --bindir)/clang++" \
    "-DCMAKE_C_COMPILER=$(llvm-config-19 --bindir)/clang" \
    "-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wno-missing-braces" \
    "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}" \
    "-DMXL_FABRICS_OFI=ON"

cmake --build "build/Linux-Clang-${BUILD_TYPE}"
