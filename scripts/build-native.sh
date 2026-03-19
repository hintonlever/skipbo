#!/bin/bash
set -e
cd "$(dirname "$0")/.."
mkdir -p build/native
cd build/native
cmake ../.. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
echo "Native build complete. Binary: build/native/tournament/skipbo_tournament"
