#!/usr/bin/env bash
# Build + run Nova Engine core tests without CMake (plain g++/clang++).
# Used by CI and by developers without a full Android toolchain.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT/core/build/headless"
mkdir -p "$BUILD_DIR"

CXX="${CXX:-g++}"
FLAGS="-std=c++17 -O2 -Wall -Wextra -I$ROOT/core/include"

SOURCES=(
    "$ROOT/core/src/json.cpp"
    "$ROOT/core/src/util/fs.cpp"
    "$ROOT/core/src/scene/Node.cpp"
    "$ROOT/core/src/scene/SceneManager.cpp"
    "$ROOT/core/src/physics2d/World2D.cpp"
    "$ROOT/core/src/script/novascript.cpp"
    "$ROOT/core/src/script/novascript_compiler.cpp"
    "$ROOT/core/src/render/null_renderer.cpp"
    "$ROOT/core/src/engine/Engine.cpp"
    "$ROOT/core/src/audio/audio_manager.cpp"
)

echo "[nova] compiling core..."
"$CXX" $FLAGS -o "$BUILD_DIR/nova_tests" "${SOURCES[@]}" "$ROOT/core/tests/test_main.cpp"

echo "[nova] running core tests..."
"$BUILD_DIR/nova_tests"
