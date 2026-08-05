#!/usr/bin/env bash
# Build VisualComp 2.1 VST3 for macOS — Azazel Audio
# Run this ON a Mac (Xcode Command Line Tools + CMake required).
#
# The build auto-installs the plugin to ~/Library/Audio/Plug-Ins/VST3/
# (handled by COPY_PLUGIN_AFTER_BUILD in CMakeLists.txt).

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$SCRIPT_DIR/cmake-build"

PLUGIN_NAME="VisualComp 2.1"

echo "=== VisualComp 2.1 macOS Build ==="
echo "Source : $ROOT_DIR"
echo "Build  : $BUILD_DIR"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="10.13"

cmake --build "$BUILD_DIR" --config Release --parallel "$(sysctl -n hw.logicalcpu)"

VST3_INSTALLED="$HOME/Library/Audio/Plug-Ins/VST3/${PLUGIN_NAME}.vst3"

if [ -d "$VST3_INSTALLED" ]; then
    echo ""
    echo "SUCCESS — plugin installed at:"
    echo "  $VST3_INSTALLED"
    echo "Rescan plugins in your DAW to load it."
else
    echo ""
    echo "Build finished but plugin not found at:"
    echo "  $VST3_INSTALLED"
    echo "Check the build output above; the artefact should also be at:"
    echo "  $BUILD_DIR/VisualComp_artefacts/Release/VST3/${PLUGIN_NAME}.vst3"
    exit 1
fi
