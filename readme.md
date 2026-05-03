# Skip-Bo AI

Play Skip-Bo against AI opponents: heuristic, MCTS, neural network, and PPO agents.

## Quick Start

### 1. Build WASM (first time, and after C++ changes)

```bash
bash scripts/build-wasm.sh
```

### 2. Start the training server

```bash
cd server
python -m venv venv          # first time only
source venv/bin/activate
pip install -r requirements.txt  # first time only
uvicorn server.main:app --reload
```

### 3. Start the web UI

```bash
cd web
npm install   # first time only
npm run dev
```

Opens at http://localhost:5173

## Features

- **Game** -- Play Skip-Bo against MCTS or heuristic AI
- **Chain Analysis** -- Visualise the MCTS search tree
- **Tournament** -- Pit any combination of AI players against each other (Random, Heuristic, MCTS, NN-MCTS, PPO)
- **Training** -- Train neural networks from the browser:
  - **AlphaZero-style**: Generate datasets via self-play, train value + policy networks
  - **PPO**: Train a reinforcement learning agent via self-play or against the heuristic. Configure games/batch, batches, opponent, device, and watch win rate + losses stream live

## PPO Training

Go to the **Training** tab in the web UI. The PPO panel lets you:

1. Pick a generation name, opponent (self-play or heuristic), and device (cpu/mps/cuda)
2. Click **Start PPO Training**
3. Watch batch results stream in real-time
4. Trained weights are saved to `data/generations/<name>/` and appear in the Generations gallery

To use the trained PPO agent in the tournament: go to **Tournament**, click **Load weights JSON**, and select `data/generations/<name>/weights.json`.

## C++ Tournament (CLI)

```bash
cmake -B build && cmake --build build
./build/tournament/skipbo_tournament --ppo data/generations/ppo-v1/weights.json --round-robin --matches 100
```
