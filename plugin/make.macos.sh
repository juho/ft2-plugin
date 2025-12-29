#!/bin/bash
cd "$(dirname "$0")"

# Create build directory if needed
mkdir -p build
cd build

# Run cmake if not configured
if [ ! -f Makefile ]; then
    cmake -DCMAKE_BUILD_TYPE=Release ..
fi

# Kill any processes that might have the plugins locked
killall -9 AudioComponentRegistrar 2>/dev/null

# Remove stale plugin bundles to prevent linker errors
rm -rf ~/Library/Audio/Plug-Ins/AU/FT2*.component
rm -rf ~/Library/Audio/Plug-Ins/VST3/FT2*.vst3

make ft2_core -j8
make FT2Plugin_VST3 -j8
make FT2Plugin_AU -j8