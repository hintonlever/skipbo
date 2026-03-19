#!/bin/bash
set -e
cd "$(dirname "$0")/.."

# Source emsdk if available
if [ -f "emsdk/emsdk_env.sh" ]; then
    source emsdk/emsdk_env.sh
fi

mkdir -p build/wasm
cd build/wasm
emcmake cmake ../.. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel

# Copy to web public dir
mkdir -p ../../web/public/wasm
cp wasm/skipbo.js wasm/skipbo.wasm ../../web/public/wasm/
echo "WASM build complete. Files copied to web/public/wasm/"
