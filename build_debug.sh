#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build"
MOS_DIR="../MemObjStorage"
 
echo "==> Rebuilding libmos.a (debug)"
(cd "$MOS_DIR" && make clean && make lib)

echo "==> Cleaning previous debug build ($BUILD_DIR)"
rm -rf "$BUILD_DIR"

cmake -B "$BUILD_DIR"

echo "==> Building"
cmake --build "$BUILD_DIR" -- -j"$(nproc)"

echo "==> Verifying build type actually took effect"
if grep -q "CMAKE_BUILD_TYPE:STRING=Debug" "$BUILD_DIR/CMakeCache.txt"; then
    echo "    CMAKE_BUILD_TYPE=Debug confirmed"
else
    echo "    WARNING: CMAKE_BUILD_TYPE is not Debug — check CMakeCache.txt"
fi

echo "==> Done. Binary: $BUILD_DIR/indexer"