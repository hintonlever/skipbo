from __future__ import annotations

import json
import uuid
from datetime import datetime, timezone
from pathlib import Path

import numpy as np
from fastapi import APIRouter, HTTPException
from pydantic import BaseModel

from ..utils.storage import (
    DATASETS_DIR,
    ensure_dirs,
    list_datasets,
    get_dataset,
    delete_dataset as delete_dataset_files,
)

router = APIRouter(prefix="/api/datasets", tags=["datasets"])

STATE_SIZE = 158
CHAIN_SIZE = 29


class CreateDatasetRequest(BaseModel):
    name: str
    p0_type: str
    p1_type: str
    num_games: int
    max_chains_per_turn: int = 50


class UploadBatchRequest(BaseModel):
    records: list[dict]  # [{state: float[], chains: float[][], chosen: int, outcome: float}]


@router.get("")
def list_all():
    return list_datasets()


@router.post("")
def create(req: CreateDatasetRequest):
    ensure_dirs()
    dataset_id = f"ds_{datetime.now(timezone.utc).strftime('%Y%m%d_%H%M%S')}_{uuid.uuid4().hex[:6]}"
    ds_dir = DATASETS_DIR / dataset_id
    ds_dir.mkdir(parents=True)
    (ds_dir / "games").mkdir()

    meta = {
        "id": dataset_id,
        "name": req.name,
        "created": datetime.now(timezone.utc).isoformat(),
        "p0_type": req.p0_type,
        "p1_type": req.p1_type,
        "num_games": req.num_games,
        "max_chains_per_turn": req.max_chains_per_turn,
        "total_turns": 0,
        "batches": 0,
    }
    with open(ds_dir / "meta.json", "w") as f:
        json.dump(meta, f, indent=2)

    return meta


@router.get("/{dataset_id}")
def get_one(dataset_id: str):
    ds = get_dataset(dataset_id)
    if ds is None:
        raise HTTPException(404, "Dataset not found")
    return ds


@router.delete("/{dataset_id}")
def delete(dataset_id: str):
    if not delete_dataset_files(dataset_id):
        raise HTTPException(404, "Dataset not found")
    return {"ok": True}


@router.post("/{dataset_id}/upload")
def upload_batch(dataset_id: str, req: UploadBatchRequest):
    ds_dir = DATASETS_DIR / dataset_id
    if not ds_dir.exists():
        raise HTTPException(404, "Dataset not found")

    records = req.records
    if not records:
        return {"ok": True, "added": 0}

    # Find max chains across all records for padding
    max_chains = max(len(r["chains"]) for r in records)
    if max_chains == 0:
        return {"ok": True, "added": 0}

    n = len(records)
    states = np.zeros((n, STATE_SIZE), dtype=np.float32)
    chains = np.zeros((n, max_chains, CHAIN_SIZE), dtype=np.float32)
    chain_mask = np.zeros((n, max_chains), dtype=np.bool_)
    chosen = np.zeros(n, dtype=np.int32)
    outcome = np.zeros(n, dtype=np.float32)

    for i, rec in enumerate(records):
        states[i] = rec["state"][:STATE_SIZE]
        nc = len(rec["chains"])
        for j in range(nc):
            chains[i, j] = rec["chains"][j][:CHAIN_SIZE]
            chain_mask[i, j] = True
        chosen[i] = rec["chosen"]
        outcome[i] = rec["outcome"]

    # Find next batch number
    games_dir = ds_dir / "games"
    existing = list(games_dir.glob("batch_*.npz"))
    batch_num = len(existing)

    np.savez_compressed(
        games_dir / f"batch_{batch_num:04d}.npz",
        state=states,
        chains=chains,
        chain_mask=chain_mask,
        chosen=chosen,
        outcome=outcome,
    )

    # Update meta
    meta_path = ds_dir / "meta.json"
    with open(meta_path) as f:
        meta = json.load(f)
    meta["total_turns"] = meta.get("total_turns", 0) + n
    meta["batches"] = batch_num + 1
    with open(meta_path, "w") as f:
        json.dump(meta, f, indent=2)

    return {"ok": True, "added": n, "batch": batch_num}
