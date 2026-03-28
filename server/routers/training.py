from __future__ import annotations

import asyncio
import json
import threading
from pathlib import Path

from fastapi import APIRouter, HTTPException
from fastapi.responses import StreamingResponse
from pydantic import BaseModel

from ..models.training_loop import train, TrainingConfig, EpochResult
from ..utils.storage import DATASETS_DIR, GENERATIONS_DIR, get_dataset

router = APIRouter(prefix="/api/training", tags=["training"])

# Global training state
_training_thread: threading.Thread | None = None
_stop_flag: list[bool] = [False]
_results: list[EpochResult] = []
_status: dict = {"running": False, "generation": None, "epoch": 0, "total_epochs": 0}
_new_result_event = threading.Event()


class StartTrainingRequest(BaseModel):
    dataset_ids: list[str]
    generation_name: str
    epochs: int = 50
    batch_size: int = 256
    learning_rate: float = 0.001
    weight_decay: float = 0.0001
    value_weight: float = 1.0
    policy_weight: float = 1.0


def _run_training(dataset_dirs: list[Path], generation_name: str, config: TrainingConfig):
    global _status
    try:
        count = 0
        for result in train(dataset_dirs, generation_name, GENERATIONS_DIR, config, _stop_flag):
            _results.append(result)
            _status["epoch"] = result.epoch
            _new_result_event.set()
            count += 1
        if count == 0:
            print(f"[training] WARNING: 0 epochs completed — dataset may be empty or all records filtered")
    except Exception as e:
        print(f"[training] ERROR: {e}")
        import traceback
        traceback.print_exc()
    finally:
        _status["running"] = False
        _new_result_event.set()


@router.post("/start")
def start_training(req: StartTrainingRequest):
    global _training_thread, _stop_flag, _results, _status

    if _status["running"]:
        raise HTTPException(409, "Training already in progress")

    # Validate datasets exist
    dataset_dirs = []
    for ds_id in req.dataset_ids:
        ds = get_dataset(ds_id)
        if ds is None:
            raise HTTPException(404, f"Dataset {ds_id} not found")
        dataset_dirs.append(DATASETS_DIR / ds_id)

    config = TrainingConfig(
        epochs=req.epochs,
        batch_size=req.batch_size,
        learning_rate=req.learning_rate,
        weight_decay=req.weight_decay,
        value_weight=req.value_weight,
        policy_weight=req.policy_weight,
    )

    _stop_flag = [False]
    _results.clear()
    _status = {
        "running": True,
        "generation": req.generation_name,
        "epoch": 0,
        "total_epochs": req.epochs,
    }
    _new_result_event.clear()

    _training_thread = threading.Thread(
        target=_run_training,
        args=(dataset_dirs, req.generation_name, config),
        daemon=True,
    )
    _training_thread.start()

    return {"ok": True, "generation": req.generation_name}


@router.post("/stop")
def stop_training():
    if not _status["running"]:
        raise HTTPException(409, "No training in progress")
    _stop_flag[0] = True
    return {"ok": True}


@router.get("/status")
def get_status():
    return _status


@router.get("/stream")
async def stream_progress():
    """SSE stream of training progress."""

    async def event_generator():
        last_sent = 0
        while True:
            # Wait for new results or training completion
            await asyncio.get_event_loop().run_in_executor(
                None, lambda: _new_result_event.wait(timeout=1.0)
            )
            _new_result_event.clear()

            # Send any new results
            while last_sent < len(_results):
                r = _results[last_sent]
                data = json.dumps({
                    "epoch": r.epoch,
                    "policy_loss": round(r.policy_loss, 6),
                    "value_loss": round(r.value_loss, 6),
                    "total_loss": round(r.total_loss, 6),
                })
                yield f"event: epoch\ndata: {data}\n\n"
                last_sent += 1

            if not _status["running"]:
                yield f"event: done\ndata: {json.dumps(_status)}\n\n"
                break

    return StreamingResponse(
        event_generator(),
        media_type="text/event-stream",
        headers={"Cache-Control": "no-cache", "Connection": "keep-alive"},
    )
