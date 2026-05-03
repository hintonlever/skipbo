from __future__ import annotations

import asyncio
import json
import threading
from pathlib import Path

from fastapi import APIRouter, HTTPException
from fastapi.responses import StreamingResponse
from pydantic import BaseModel

from ..utils.storage import GENERATIONS_DIR, ensure_dirs

router = APIRouter(prefix="/api/ppo", tags=["ppo"])

_training_thread: threading.Thread | None = None
_stop_flag: list[bool] = [False]
_log: list[dict] = []
_status: dict = {"running": False, "batch": 0, "total_batches": 0, "generation": None}
_new_log_event = threading.Event()


class PPOStartRequest(BaseModel):
    generation_name: str
    num_games: int = 256
    num_batches: int = 200
    lr: float = 3e-4
    ppo_epochs: int = 4
    minibatch_size: int = 512
    gamma: float = 0.99
    gae_lambda: float = 0.95
    clip_epsilon: float = 0.2
    entropy_coef: float = 0.01
    opponent: str = "self"  # "self" or "heuristic"
    device: str = "cpu"


def _run_ppo(req: PPOStartRequest):
    """Run PPO training in background thread, pushing logs as we go."""
    global _status
    import sys, os
    sys.path.insert(0, str(Path(__file__).parent.parent))

    try:
        from ppo.game_env import NUM_ACTIONS, SkipBoEnv, is_discard_action
        from ppo.encoding import encode_state
        from ppo.networks import ActorCritic, export_weights_for_cpp
        from ppo.train import (
            PPOConfig, collect_batch, ppo_update, HeuristicOpponent,
        )
        import torch

        config = PPOConfig(
            num_games_per_batch=req.num_games,
            num_batches=req.num_batches,
            lr=req.lr,
            ppo_epochs=req.ppo_epochs,
            minibatch_size=req.minibatch_size,
            gamma=req.gamma,
            gae_lambda=req.gae_lambda,
            clip_epsilon=req.clip_epsilon,
            entropy_coef=req.entropy_coef,
            opponent=req.opponent,
            device=req.device,
        )

        device = torch.device(config.device)
        model = ActorCritic().to(device)
        optimizer = torch.optim.Adam(model.parameters(), lr=config.lr)

        heuristic = HeuristicOpponent() if config.opponent == "heuristic" else None
        opponent_model = None

        total_params = sum(p.numel() for p in model.parameters())
        _log.append({"type": "info", "message": f"PPO: {total_params:,} params, opponent={config.opponent}"})
        _new_log_event.set()

        gen_dir = GENERATIONS_DIR / req.generation_name
        gen_dir.mkdir(parents=True, exist_ok=True)

        best_win_rate = 0.0

        for batch_idx in range(config.num_batches):
            if _stop_flag[0]:
                break

            import time
            t0 = time.time()

            steps, collect_stats = collect_batch(config, model, opponent_model, device, heuristic)
            update_stats = ppo_update(config, model, optimizer, steps, device)

            elapsed = time.time() - t0
            wr = collect_stats["win_rate"]

            _status["batch"] = batch_idx + 1

            entry = {
                "type": "batch",
                "batch": batch_idx + 1,
                "win_rate": round(wr, 4),
                "steps": collect_stats["steps"],
                "avg_game_length": round(collect_stats["avg_game_length"], 1),
                "pg_loss": round(update_stats.get("pg_loss", 0), 6),
                "v_loss": round(update_stats.get("v_loss", 0), 6),
                "entropy": round(update_stats.get("entropy", 0), 4),
                "elapsed": round(elapsed, 2),
            }
            _log.append(entry)
            _new_log_event.set()

            if wr > best_win_rate:
                best_win_rate = wr

            # Save checkpoint every 25 batches
            if (batch_idx + 1) % 25 == 0:
                weights_path = gen_dir / "weights.json"
                export_weights_for_cpp(model, weights_path)

        # Final save
        weights_path = gen_dir / "weights.json"
        export_weights_for_cpp(model, weights_path)

        # Save meta
        meta = {
            "name": req.generation_name,
            "type": "ppo",
            "batches": _status["batch"],
            "opponent": req.opponent,
            "best_win_rate": round(best_win_rate, 4),
        }
        with open(gen_dir / "meta.json", "w") as f:
            json.dump(meta, f, indent=2)

        # Save torch checkpoint
        torch.save({"model": model.state_dict(), "optimizer": optimizer.state_dict()},
                    gen_dir / "checkpoint.pt")

        _log.append({"type": "done", "message": f"Training complete. Best WR: {best_win_rate:.1%}"})
        _new_log_event.set()

    except Exception as e:
        import traceback
        traceback.print_exc()
        _log.append({"type": "error", "message": str(e)})
        _new_log_event.set()
    finally:
        _status["running"] = False
        _new_log_event.set()


@router.post("/start")
def start_ppo(req: PPOStartRequest):
    global _training_thread, _stop_flag, _log, _status

    if _status["running"]:
        raise HTTPException(409, "PPO training already in progress")

    ensure_dirs()

    _stop_flag = [False]
    _log.clear()
    _status = {
        "running": True,
        "batch": 0,
        "total_batches": req.num_batches,
        "generation": req.generation_name,
    }
    _new_log_event.clear()

    _training_thread = threading.Thread(target=_run_ppo, args=(req,), daemon=True)
    _training_thread.start()

    return {"ok": True, "generation": req.generation_name}


@router.post("/stop")
def stop_ppo():
    if not _status["running"]:
        raise HTTPException(409, "No PPO training in progress")
    _stop_flag[0] = True
    return {"ok": True}


@router.get("/status")
def ppo_status():
    return _status


@router.get("/stream")
async def ppo_stream():
    """SSE stream of PPO training progress."""

    async def event_generator():
        last_sent = 0
        while True:
            await asyncio.get_event_loop().run_in_executor(
                None, lambda: _new_log_event.wait(timeout=1.0)
            )
            _new_log_event.clear()

            while last_sent < len(_log):
                entry = _log[last_sent]
                data = json.dumps(entry)
                yield f"event: {entry['type']}\ndata: {data}\n\n"
                last_sent += 1

            if not _status["running"]:
                yield f"event: finished\ndata: {json.dumps(_status)}\n\n"
                break

    return StreamingResponse(
        event_generator(),
        media_type="text/event-stream",
        headers={"Cache-Control": "no-cache", "Connection": "keep-alive"},
    )
