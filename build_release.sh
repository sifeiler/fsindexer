#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build-release"
MOS_DIR="../MemObjStorage"
 
echo "==> Rebuilding libmos.a (release)"
(cd "$MOS_DIR" && make clean && make lib-release)

echo "==> Cleaning previous release build ($BUILD_DIR)"
rm -rf "$BUILD_DIR"

echo "==> Configuring (Release)"
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release

echo "==> Building"
cmake --build "$BUILD_DIR" -- -j"$(nproc)"

echo "==> Verifying build type actually took effect"
if grep -q "CMAKE_BUILD_TYPE:STRING=Release" "$BUILD_DIR/CMakeCache.txt"; then
    echo "    CMAKE_BUILD_TYPE=Release confirmed"
else
    echo "    WARNING: CMAKE_BUILD_TYPE is not Release — check CMakeCache.txt"
fi

echo "==> Done. Binary: $BUILD_DIR/indexer"