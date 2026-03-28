from __future__ import annotations

import json
import shutil
from pathlib import Path

DATA_DIR = Path(__file__).parent.parent.parent / "data"
DATASETS_DIR = DATA_DIR / "datasets"
GENERATIONS_DIR = DATA_DIR / "generations"


def ensure_dirs():
    DATASETS_DIR.mkdir(parents=True, exist_ok=True)
    GENERATIONS_DIR.mkdir(parents=True, exist_ok=True)


def list_datasets() -> list[dict]:
    ensure_dirs()
    results = []
    for d in sorted(DATASETS_DIR.iterdir()):
        meta_path = d / "meta.json"
        if meta_path.exists():
            with open(meta_path) as f:
                results.append(json.load(f))
    return results


def get_dataset(dataset_id: str) -> dict | None:
    meta_path = DATASETS_DIR / dataset_id / "meta.json"
    if not meta_path.exists():
        return None
    with open(meta_path) as f:
        return json.load(f)


def get_dataset_dir(dataset_id: str) -> Path:
    return DATASETS_DIR / dataset_id


def delete_dataset(dataset_id: str) -> bool:
    d = DATASETS_DIR / dataset_id
    if d.exists():
        shutil.rmtree(d)
        return True
    return False


def list_generations() -> list[dict]:
    ensure_dirs()
    results = []
    for d in sorted(GENERATIONS_DIR.iterdir()):
        meta_path = d / "meta.json"
        if meta_path.exists():
            with open(meta_path) as f:
                results.append(json.load(f))
    return results


def get_generation_weights(name: str) -> dict | None:
    weights_path = GENERATIONS_DIR / name / "weights.json"
    if not weights_path.exists():
        return None
    with open(weights_path) as f:
        return json.load(f)


def delete_generation(name: str) -> bool:
    d = GENERATIONS_DIR / name
    if d.exists():
        shutil.rmtree(d)
        return True
    return False
