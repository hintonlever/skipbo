from __future__ import annotations

from fastapi import APIRouter, HTTPException

from ..utils.storage import (
    list_generations,
    get_generation_weights,
    delete_generation as delete_gen_files,
)

router = APIRouter(prefix="/api/generations", tags=["generations"])


@router.get("")
def list_all():
    return list_generations()


@router.get("/{name}/weights")
def get_weights(name: str):
    weights = get_generation_weights(name)
    if weights is None:
        raise HTTPException(404, "Generation not found")
    return weights


@router.delete("/{name}")
def delete(name: str):
    if not delete_gen_files(name):
        raise HTTPException(404, "Generation not found")
    return {"ok": True}
