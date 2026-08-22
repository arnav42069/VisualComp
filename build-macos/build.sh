#!/usr/bin/env bash
# Build VisualComp 2.36 VST3 + AU for macOS — Azazel Audio
# Run this ON a Mac (Xcode Command Line Tools + CMake required).
#
# The build auto-installs both formats (handled by COPY_PLUGIN_AFTER_BUILD in
# CMakeLists.txt):
#   VST3 -> ~/Library/Audio/Plug-Ins/VST3/
#   AU   -> ~/Library/Audio/Plug-Ins/Components/ (needed for Logic Pro, which
#           doesn't load VST3)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$SCRIPT_DIR/cmake-build"

PLUGIN_NAME="VisualComp 2.36"

echo "=== VisualComp 2.36 macOS Build (VST3 + AU) ==="
echo "Source : $ROOT_DIR"
echo "Build  : $BUILD_DIR"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="10.13"

cmake --build "$BUILD_DIR" --config Release --parallel "$(sysctl -n hw.logicalcpu)"

VST3_INSTALLED="$HOME/Library/Audio/Plug-Ins/VST3/${PLUGIN_NAME}.vst3"
AU_INSTALLED="$HOME/Library/Audio/Plug-Ins/Components/${PLUGIN_NAME}.component"

ok=true
echo ""
if [ -d "$VST3_INSTALLED" ]; then
    echo "SUCCESS — VST3 installed at:"
    echo "  $VST3_INSTALLED"
else
    echo "VST3 not found at:"
    echo "  $VST3_INSTALLED"
    ok=false
fi

if [ -d "$AU_INSTALLED" ]; then
    echo "SUCCESS — AU installed at:"
    echo "  $AU_INSTALLED"
else
    echo "AU not found at:"
    echo "  $AU_INSTALLED"
    ok=false
fi

if [ "$ok" = true ]; then
    echo ""
    echo "Rescan plugins in your DAW to load them."
else
    echo ""
    echo "Check the build output above; artefacts should also be at:"
    echo "  $BUILD_DIR/VisualComp_artefacts/Release/VST3/${PLUGIN_NAME}.vst3"
    echo "  $BUILD_DIR/VisualComp_artefacts/Release/AU/${PLUGIN_NAME}.component"
    exit 1
fi
