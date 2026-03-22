# Web Development

## Start the dev server

```bash
cd web
npm run dev
```

Runs on http://localhost:5173 by default.

## Rebuilding after C++ changes

When you modify engine, ai, or wasm C++ code, rebuild the WASM and copy the output:

```bash
# From project root — activate Emscripten first
source emsdk/emsdk_env.sh
emcmake cmake -B build-wasm && cmake --build build-wasm

# Copy updated WASM artifacts into the web app
cp build-wasm/wasm/skipbo.js build-wasm/wasm/skipbo.wasm web/public/wasm/
```

Vite will hot-reload after the files are copied — no need to restart the dev server.

### Full rebuild from scratch

```bash
source emsdk/emsdk_env.sh
rm -rf build-wasm
emcmake cmake -B build-wasm && cmake --build build-wasm
cp build-wasm/wasm/skipbo.js build-wasm/wasm/skipbo.wasm web/public/wasm/
```

## Killing a running server

```bash
# Check what's on port 5173
lsof -i :5173

# Kill it
kill $(lsof -ti :5173)

# Force kill if it won't stop
kill -9 $(lsof -ti :5173)
```
