#!/bin/bash
cd "$(dirname "$0")"

# Kill any processes that might have the plugins locked
killall -9 AudioComponentRegistrar 2>/dev/null

# Remove stale plugin bundles to prevent linker errors
rm -rf ~/Library/Audio/Plug-Ins/AU/FT2*.component
rm -rf ~/Library/Audio/Plug-Ins/VST3/FT2*.vst3

# Build universal for modern macOS (11.0+)
echo "=== Building Universal (macOS 11.0+) ==="
rm -rf build-universal
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
      "-DCMAKE_OSX_ARCHITECTURES=x86_64;arm64" \
      -B build-universal -S .
cmake --build build-universal --config Release --parallel

# Build x86_64 only for older macOS (10.14 Mojave+)
echo "=== Building x86_64 Legacy (macOS 10.14+) ==="
rm -rf build-legacy
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=10.14 \
      -DCMAKE_OSX_ARCHITECTURES=x86_64 \
      -B build-legacy -S .
cmake --build build-legacy --config Release --parallel

echo ""
echo "=== Build complete ==="
echo "Universal (macOS 11.0+): build-universal/FT2Plugin_artefacts/Release/"
echo "Legacy x86_64 (macOS 10.14+): build-legacy/FT2Plugin_artefacts/Release/"