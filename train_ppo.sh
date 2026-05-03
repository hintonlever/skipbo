#!/bin/bash
cd "$(dirname "$0")/server"
source venv/bin/activate
python -m ppo --num-games 256 --num-batches 200 --opponent heuristic --output ../data/generations/ppo-v1/weights.json "$@"
