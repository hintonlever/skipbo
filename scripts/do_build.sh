#!/bin/bash
set -e
cd "$(dirname "$0")/.."
ROOT=$(pwd)

# Kill existing processes on our ports
echo "Stopping existing servers..."
lsof -ti:5173 | xargs kill -9 2>/dev/null || true
lsof -ti:8000 | xargs kill -9 2>/dev/null || true

# Build WASM
echo "Building WASM..."
./scripts/build-wasm.sh

# Set up Python venv if needed
if [ ! -d "venv" ]; then
    echo "Creating Python virtual environment..."
    python3 -m venv venv
fi
source venv/bin/activate
pip install -q -r server/requirements.txt

# Start Python server in background
echo "Starting Python server on :8000..."
cd "$ROOT"
python -m uvicorn server.main:app --reload --port 8000 &
PYTHON_PID=$!

# Start Vite dev server in background
echo "Starting Vite dev server on :5173..."
cd "$ROOT/web"
npm run dev &
VITE_PID=$!

echo ""
echo "All running:"
echo "  WASM:   built and copied to web/public/wasm/"
echo "  Python: http://localhost:8000 (PID $PYTHON_PID)"
echo "  Vite:   http://localhost:5173 (PID $VITE_PID)"
echo ""
echo "Press Ctrl+C to stop all servers."

# Trap Ctrl+C to kill both servers
trap "echo 'Shutting down...'; kill $PYTHON_PID $VITE_PID 2>/dev/null; exit" INT TERM

# Wait for either to exit
wait
