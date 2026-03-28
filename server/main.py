from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from .routers import datasets, training, generations
from .utils.storage import ensure_dirs

app = FastAPI(title="Skip-Bo NN Training Server")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(datasets.router)
app.include_router(training.router)
app.include_router(generations.router)


@app.on_event("startup")
def startup():
    ensure_dirs()


@app.get("/api/health")
def health():
    return {"ok": True}
