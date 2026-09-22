#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
INDEXER_REPO_NAME="fsindexer"
MOS_REPO_NAME="MemObjStorage"

TRIPLE=$(rustc -vV | sed -n 's/host: //p')

INDEXER_DIR="$REPO_ROOT/$INDEXER_REPO_NAME"
BUILD_DIR="$INDEXER_DIR/build-debug-server"
MOS_DIR="$REPO_ROOT/$MOS_REPO_NAME"
TAURI_BINARIES_DIR="$INDEXER_DIR/src/tauri-app/fsi-tauri/src-tauri/binaries"
 
echo "==> Rebuilding libmos.a (debug)"
(cd "$MOS_DIR" && make clean && make lib)

echo "==> Cleaning previous debug build ($BUILD_DIR)"
rm -rf "$BUILD_DIR"

echo "==> Configuring (Debug)"
cmake -S "$INDEXER_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug_Server

echo "==> Building"
cmake --build "$BUILD_DIR" -- -j"$(nproc)"

echo "==> Verifying build type actually took effect"
if grep -q "CMAKE_BUILD_TYPE:STRING=Debug_Server" "$BUILD_DIR/CMakeCache.txt"; then
    echo "    CMAKE_BUILD_TYPE=Debug_Server confirmed"
else
    echo "    WARNING: CMAKE_BUILD_TYPE is not Debug_Server — check CMakeCache.txt"
fi

mkdir -p "$TAURI_BINARIES_DIR"

echo "Copying indexer binary to tauri binaries"
cp "$BUILD_DIR/indexer" \
   "$TAURI_BINARIES_DIR/indexer-${TRIPLE}"

echo "==> Done. Binary: $BUILD_DIR/indexer"