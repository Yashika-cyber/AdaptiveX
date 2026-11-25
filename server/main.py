"""
AdaptiveX WebSocket Server
Runs the C++ engine as a subprocess, parses its JSON stdout,
and streams snapshots to connected browser clients.

Usage:
    uvicorn main:app --host 0.0.0.0 --port 8000 --reload
"""
import asyncio
import json
import os
import sys
import subprocess
from pathlib import Path
from typing import Set

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware

app = FastAPI(title="AdaptiveX Engine API", version="1.0.0")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# ─── Active WebSocket connections ────────────────────────────
connected: Set[WebSocket] = set()
latest_snapshot: dict = {}
engine_mode = "connecting"

# ─── Engine config (overridden by /start endpoint) ──────────
ENGINE_DEFAULT = {
    "rate": 1000,
    "burst": 5000,
    "seconds": 3600,
    "seed": 42,
    "workers": 4,
    "sync": "locked",
    "policy": "adaptive",
    "high_water": 500,
    "low_water": 100,
    "symbols": 20,
}

ENGINE_BIN = Path(__file__).parent.parent / "engine" / "build" / "adaptivex"
engine_proc: subprocess.Popen | None = None
engine_task: asyncio.Task | None = None

# ─── WebSocket broadcast ──────────────────────────────────────
async def broadcast(payload: dict):
    if not connected:
        return
    msg = json.dumps(payload)
    dead = set()
    for ws in list(connected):
        try:
            await ws.send_text(msg)
        except Exception:
            dead.add(ws)
    connected.difference_update(dead)

# ─── Engine subprocess reader ─────────────────────────────────
async def run_engine(engine_cfg: dict):
    global engine_proc, engine_mode, latest_snapshot

    args = [
        str(ENGINE_BIN),
        "--rate",       str(engine_cfg.get("rate", 1000)),
        "--burst",      str(engine_cfg.get("burst", 5000)),
        "--seconds",    str(engine_cfg.get("seconds", 3600)),
        "--seed",       str(engine_cfg.get("seed", 42)),
        "--workers",    str(engine_cfg.get("workers", 4)),
        "--sync",       engine_cfg.get("sync", "locked"),
        "--policy",     engine_cfg.get("policy", "adaptive"),
        "--high-water", str(engine_cfg.get("high_water", 500)),
        "--low-water",  str(engine_cfg.get("low_water", 100)),
        "--symbols",    str(engine_cfg.get("symbols", 20)),
    ]

    try:
        engine_proc = await asyncio.create_subprocess_exec(
            *args,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
    except FileNotFoundError:
        # Engine binary not built yet — emit synthetic demo data
        print("[server] Engine binary not found — running in demo mode", file=sys.stderr)
        engine_mode = "demo"
        await broadcast({"type": "mode", "data": {"mode": engine_mode}})
        await demo_mode()
        return

    engine_mode = "live"
    await broadcast({"type": "mode", "data": {"mode": engine_mode}})
    assert engine_proc.stdout
    while True:
        line = await engine_proc.stdout.readline()
        if not line:
            break
        try:
            snap = json.loads(line.decode().strip())
            latest_snapshot = snap
            await broadcast({"type": "snapshot", "data": snap})
        except json.JSONDecodeError:
            pass  # skip malformed lines

    await engine_proc.wait()
    await broadcast({"type": "done", "data": {}})

async def demo_mode():
    """Emit synthetic snapshots when engine binary is absent (CI/demo)."""
    import math, random
    t = 0
    policies = ["PRIORITY", "SJF", "RR"]
    policy_idx = 1
    policy_timer = 0
    while True:
        t += 1
        policy_timer += 1
        if policy_timer > random.randint(10, 30):
            policy_timer = 0
            policy_idx = (policy_idx + 1) % 3

        tput = 1200 + 300 * math.sin(t * 0.1) + random.gauss(0, 80)
        snap = {
            "t": t,
            "policy": policies[policy_idx],
            "burst": random.random() < 0.08,
            "p50_us": int(350 + 150 * abs(math.sin(t * 0.07)) + random.gauss(0, 20)),
            "p99_us": int(700 + 400 * abs(math.sin(t * 0.07)) + random.gauss(0, 60)),
            "tput":   round(max(100, tput), 1),
            "fairness": round(0.85 + 0.1 * math.sin(t * 0.05) + random.gauss(0, 0.01), 4),
            "sr_hft":    round(min(1.0, 0.97 + random.gauss(0, 0.01)), 4),
            "sr_retail": round(min(1.0, max(0.1, 0.60 + random.gauss(0, 0.03))), 4),
            "sr_mm":     round(min(1.0, 0.87 + random.gauss(0, 0.02)), 4),
            "starved":   max(0, int(random.gauss(12, 4))),
            "cache_pred": round(min(0.85, 0.65 + t * 0.001 + random.gauss(0, 0.01)), 4),
            "cache_lru":  round(0.50 + random.gauss(0, 0.015), 4),
            "wal_crit_ms": round(0.3 + random.gauss(0, 0.05), 3),
            "wal_rout_ms": round(5.0 + random.gauss(0, 0.5), 3),
            "wal_lift":    round(16 + random.gauss(0, 1), 2),
            "switches":    t // 20,
            "q_len":       max(0, int(200 + 150 * abs(math.sin(t * 0.1)) + random.gauss(0, 30))),
        }
        latest_snapshot = snap
        await broadcast({"type": "snapshot", "data": snap})
        await asyncio.sleep(1)

# ─── API endpoints ────────────────────────────────────────────
@app.on_event("startup")
async def startup():
    global engine_task
    engine_task = asyncio.create_task(run_engine(ENGINE_DEFAULT))

@app.on_event("shutdown")
async def shutdown():
    global engine_proc, engine_task
    if engine_proc:
        engine_proc.terminate()
    if engine_task:
        engine_task.cancel()

@app.get("/health")
async def health():
    return {"status": "ok", "engine_pid": engine_proc.pid if engine_proc else None}

@app.get("/snapshot")
async def get_snapshot():
    return latest_snapshot

@app.post("/restart")
async def restart(cfg: dict = None):
    global engine_proc, engine_task
    if engine_proc:
        engine_proc.terminate()
    if engine_task:
        engine_task.cancel()
    engine_task = asyncio.create_task(run_engine(cfg or ENGINE_DEFAULT))
    return {"status": "restarted"}

@app.websocket("/ws")
async def websocket_endpoint(ws: WebSocket):
    await ws.accept()
    connected.add(ws)
    await ws.send_text(json.dumps({"type": "mode", "data": {"mode": engine_mode}}))
    # Send latest snapshot immediately so the client isn't blank
    if latest_snapshot:
        await ws.send_text(json.dumps({"type": "snapshot", "data": latest_snapshot}))
    try:
        while True:
            await ws.receive_text()  # keep-alive / ignore client messages
    except WebSocketDisconnect:
        connected.discard(ws)
