# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Test

```bash
# Configure and build (from project root)
cmake -B build && cmake --build build

# Run all tests
ctest --test-dir build --output-on-failure

# Run a single test executable
./build/engine/engine_tests
./build/ai/ai_tests

# Run a specific Catch2 test case
./build/engine/engine_tests "test case name"

# Run tournament
./build/tournament/skipbo_tournament --matches 100 --mcts-iters 500 --mcts-dets 20
./build/tournament/skipbo_tournament --round-robin --heuristic 0.7 --seed 42

# WASM build (requires Emscripten)
emcmake cmake -B build-wasm && cmake --build build-wasm
```

## Architecture

C++20 project with three layered modules, built as static libraries plus one executable:

- **engine** (`skipbo_engine`) — Game rules, state, and move generation. No dependencies. Cards are `uint8_t` (SKIPBO=0, 1–12 numbered). `GameState` holds all observable state; `Game` manages turn flow. Two move-application paths: `Game::apply_move()` for real play (draws cards, RNG) and `Game::apply_move_to_state()` for fast simulation (no RNG, no side effects beyond state mutation).
- **ai** (`skipbo_ai`) — Links `skipbo_engine`. Abstract `Player` interface with `RandomPlayer` and `MCTSPlayer`. MCTS uses information-set determinization: `Determinizer` samples complete game states from observable info by shuffling hidden cards (opponent hand, stock interiors, draw pile). Multiple determinizations are aggregated by visit count. `MCTSPlayer::analyze_moves()` exposes per-move win% for UI.
- **tournament** (`skipbo_tournament`) — Executable linking both libraries. Runs matches between player types with ELO rating tracking.
- **wasm** — Emscripten build target. `WasmGameController` wraps Game+MCTSPlayer for JavaScript: state queries, move application, AI turns, and `analyzeMoves()` returning per-move win probabilities. Bound via `emscripten::class_`.

Each module follows `include/<module>/`, `src/`, `tests/` layout. Tests use Catch2 v3.5.2 (fetched via CMake FetchContent).

### Game rules encoded in engine

- 162 total cards: 12 copies each of 1–12, plus 18 Skip-Bo wilds. Card type is `uint8_t` (`CARD_SKIPBO=0`, `CARD_NONE=255` sentinel).
- Each player starts with 30 stock cards. Hand refills to 5 at turn start.
- Win condition: first player to empty their stock pile. Building piles that reach 12 are recycled into the draw pile.
- A discard move (`target >= DiscardPile0`) ends the current turn.
- Stalemate: if no player can make progress (consecutive passes), the player with fewer stock pile cards wins.

### Key design decisions

- **Compact state for fast MCTS rollouts**: `uint8_t` cards, small fixed-size arrays, minimal allocations in hot paths.
- **Observable vs. complete state split**: `GameState` is what a player can see; `Determinizer` fills in hidden information for MCTS search. This separation is load-bearing — don't merge them.
- **Dual move-application paths**: The static `apply_move_to_state()` exists specifically so MCTS rollouts avoid touching `Game`'s RNG and turn machinery. Both paths must stay in sync on game logic.
- **Heuristic rollout blending**: `MCTSConfig::rollout_heuristic_rate` (0.0–1.0) blends random rollouts with a priority heuristic (stock→build > any→build > random discard). Tunable via `--heuristic` flag in tournament.
