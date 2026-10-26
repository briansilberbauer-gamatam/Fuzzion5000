#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
CONFIG="${CONFIG:-Release}"
GENERATOR="${GENERATOR:-Unix Makefiles}"

"$ROOT/scripts/setup-linux.sh" >/dev/null

# Rootless Ubuntu dev headers and linker stubs live in the offline sysroot.
if [ -d "$ROOT/deps/sysroot/usr" ]; then
    export PKG_CONFIG_PATH="$ROOT/deps/sysroot/usr/lib/x86_64-linux-gnu/pkgconfig:$ROOT/deps/sysroot/usr/share/pkgconfig:${PKG_CONFIG_PATH:-}"
    export PKG_CONFIG_SYSROOT_DIR="$ROOT/deps/sysroot"
    CMAKE_EXTRA_ARGS=(
        "-DCMAKE_INCLUDE_PATH=$ROOT/deps/sysroot/usr/include"
        "-DCMAKE_LIBRARY_PATH=$ROOT/deps/sysroot/usr/lib/x86_64-linux-gnu"
    )
else
    CMAKE_EXTRA_ARGS=()
fi

cmake -S "$ROOT" -B "$BUILD_DIR" -G "$GENERATOR" \
    -DCMAKE_BUILD_TYPE="$CONFIG" \
    -DFUZZION5000_BUILD_TESTS=ON \
    -DFUZZION5000_BUILD_SCREENSHOT=ON \
    -DFUZZION5000_BUILD_HOST_TEST=ON \
    "${CMAKE_EXTRA_ARGS[@]}"

if [ -z "${JOBS:-}" ]; then
    JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
fi

cmake --build "$BUILD_DIR" --config "$CONFIG" --parallel "$JOBS"

VST3="$BUILD_DIR/Fuzzion5000_artefacts/$CONFIG/VST3/Fuzzion-5000.vst3"
STANDALONE="$BUILD_DIR/Fuzzion5000_artefacts/$CONFIG/Standalone/Fuzzion-5000"

mkdir -p "$ROOT/dist"
if [ -d "$VST3" ]; then
    rm -rf "$ROOT/dist/Fuzzion-5000.vst3"
    cp -a "$VST3" "$ROOT/dist/"
fi
if [ -x "$STANDALONE" ]; then
    cp -a "$STANDALONE" "$ROOT/dist/Fuzzion-5000"
fi

echo
echo "Built artefacts:"
[ -d "$ROOT/dist/Fuzzion-5000.vst3" ] && echo "  VST3        $ROOT/dist/Fuzzion-5000.vst3"
[ -x "$ROOT/dist/Fuzzion-5000" ] && echo "  Standalone  $ROOT/dist/Fuzzion-5000"
echo "  Tests       $BUILD_DIR/Fuzzion5000Tests_artefacts/$CONFIG/Fuzzion5000Tests"
if [ -x "$BUILD_DIR/Fuzzion5000HostSmokeTest_artefacts/$CONFIG/Fuzzion5000HostSmokeTest" ]; then
    echo "  Host test   $BUILD_DIR/Fuzzion5000HostSmokeTest_artefacts/$CONFIG/Fuzzion5000HostSmokeTest"
fi
if [ -x "$BUILD_DIR/Fuzzion5000Screenshot_artefacts/$CONFIG/Fuzzion5000Screenshot" ]; then
    echo "  Screenshot  $BUILD_DIR/Fuzzion5000Screenshot_artefacts/$CONFIG/Fuzzion5000Screenshot"
fi
