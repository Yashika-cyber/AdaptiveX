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
import statistics
import csv
import io
from datetime import datetime, timezone
from pathlib import Path
from typing import Set

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import StreamingResponse

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
    "book_symbol": "AAPL",
}
SYMBOL_POOL = ["AAPL", "MSFT", "GOOGL", "AMZN", "TSLA", "NVDA", "META", "NFLX", "AMD", "INTC", "ORCL", "CRM", "ADBE", "PYPL", "UBER", "LYFT", "SNAP", "TWTR", "RBLX", "COIN"]

ENGINE_BIN = Path(__file__).parent.parent / "engine" / "build" / "adaptivex"
engine_proc: subprocess.Popen | None = None
engine_task: asyncio.Task | None = None
engine_config = ENGINE_DEFAULT.copy()
engine_lock = asyncio.Lock()
EXPERIMENTS_FILE = Path(__file__).parent.parent / "experiments" / "results" / "dashboard_history.json"
EXPERIMENTS = {
 "EXP-01": ("Priority vs SJF vs RR", [{"policy": p} for p in ("priority", "sjf", "rr")]),
 "EXP-02": ("Fixed scheduling vs Adaptive", [{"policy": p} for p in ("priority", "adaptive")]),
 "EXP-03": ("Mutex vs Lock-free", [{"sync": s} for s in ("locked", "lockfree")]),
 "EXP-04": ("Worker scalability", [{"workers": w} for w in (1, 2, 4, 8)]),
 "EXP-05": ("Load levels", [{"rate": r, "burst": r * 5} for r in (300, 1000, 3000)]),
 "EXP-06": ("Normal vs burst traffic", [{"rate": 1000, "burst": b} for b in (1200, 10000)]),
 "EXP-07": ("Predictive cache vs LRU", [{"policy": "adaptive"}, {"policy": "sjf"}]),
 "EXP-08": ("Priority WAL vs FIFO WAL", [{"sync": "locked"}, {"sync": "lockfree"}]),
 "EXP-09": ("Fairness across trader classes", [{"policy": p} for p in ("priority", "sjf", "rr")]),
 "EXP-10": ("Symbol scalability", [{"symbols": n, "book_symbol": "AAPL"} for n in (1, 5, 10, 20)]),
}
experiment_history: list[dict] = []

def load_history():
    global experiment_history
    try: experiment_history = json.loads(EXPERIMENTS_FILE.read_text())
    except (OSError, json.JSONDecodeError): experiment_history = []

def save_history():
    EXPERIMENTS_FILE.parent.mkdir(parents=True, exist_ok=True)
    EXPERIMENTS_FILE.write_text(json.dumps(experiment_history, indent=2))

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
        "--book-symbol", engine_cfg.get("book_symbol", "AAPL"),
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
    if engine_proc is not None:
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
            "p90_us": int(550 + 250 * abs(math.sin(t * 0.07)) + random.gauss(0, 30)),
            "p95_us": int(620 + 320 * abs(math.sin(t * 0.07)) + random.gauss(0, 40)),
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
    await stop_engine()

@app.get("/health")
async def health():
    return {"status": "ok", "mode": engine_mode, "engine_pid": engine_proc.pid if engine_proc else None}

@app.get("/snapshot")
async def get_snapshot():
    return latest_snapshot

def merged_config(update: dict | None = None) -> dict:
    cfg = ENGINE_DEFAULT | (update or {})
    allowed = set(ENGINE_DEFAULT)
    unknown = set((update or {})) - allowed
    if unknown:
        raise ValueError(f"unknown settings: {', '.join(sorted(unknown))}")
    if (not isinstance(cfg["rate"], (int, float)) or cfg["rate"] <= 0 or
        not isinstance(cfg["burst"], (int, float)) or cfg["burst"] <= 0 or
        not isinstance(cfg["workers"], int) or cfg["workers"] < 1 or
        not isinstance(cfg["symbols"], int) or not 1 <= cfg["symbols"] <= 20 or
        not isinstance(cfg["book_symbol"], str) or cfg["book_symbol"] not in SYMBOL_POOL[:cfg["symbols"]] or
        not isinstance(cfg["high_water"], int) or not isinstance(cfg["low_water"], int) or
        cfg["low_water"] >= cfg["high_water"] or
        cfg["sync"] not in {"locked", "lockfree"} or
        cfg["policy"] not in {"priority", "sjf", "rr", "adaptive"}):
        raise ValueError("invalid simulation configuration")
    return cfg

async def stop_engine():
    global engine_proc, engine_task, engine_mode
    if engine_proc:
        if engine_proc.returncode is None:
            engine_proc.terminate()
            try:
                await asyncio.wait_for(engine_proc.wait(), timeout=3)
            except asyncio.TimeoutError:
                engine_proc.kill()
                await engine_proc.wait()
        engine_proc = None
    if engine_task:
        engine_task.cancel()
        try:
            await engine_task
        except asyncio.CancelledError:
            pass
        engine_task = None
    engine_mode = "stopped"
    await broadcast({"type": "mode", "data": {"mode": engine_mode}})

async def start_engine(cfg: dict | None = None):
    global engine_task, engine_config
    async with engine_lock:
        engine_config = merged_config(cfg or engine_config)
        await stop_engine()
        engine_task = asyncio.create_task(run_engine(engine_config))
    return {"status": "started", "config": engine_config}

async def run_experiment(exp_id: str, repeats: int = 3, seconds: int = 3):
    name, variants = EXPERIMENTS[exp_id]
    result = {"id": exp_id, "name": name, "created_at": datetime.now(timezone.utc).isoformat(), "status": "running", "variants": []}
    experiment_history.insert(0, result); save_history()
    try:
        for index, override in enumerate(variants):
            cfg = merged_config(ENGINE_DEFAULT | override | {"seconds": seconds})
            samples = []
            for repeat in range(repeats):
                args = [str(ENGINE_BIN), "--rate", str(cfg["rate"]), "--burst", str(cfg["burst"]), "--seconds", str(seconds), "--seed", str(42 + repeat), "--workers", str(cfg["workers"]), "--sync", cfg["sync"], "--policy", cfg["policy"], "--high-water", str(cfg["high_water"]), "--low-water", str(cfg["low_water"]), "--symbols", str(cfg["symbols"]), "--book-symbol", cfg["book_symbol"]]
                proc = await asyncio.create_subprocess_exec(*args, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.DEVNULL)
                stdout, _ = await proc.communicate()
                snaps = [json.loads(line) for line in stdout.decode().splitlines() if line.startswith("{")]
                if snaps: samples.append(snaps[-1])
            label = ", ".join(f"{k}={v}" for k, v in override.items())
            def summary(key):
                values = [float(s.get(key, 0)) for s in samples]
                return {"mean": round(statistics.mean(values), 4) if values else 0, "stddev": round(statistics.stdev(values), 4) if len(values) > 1 else 0}
            result["variants"].append({"label": label or f"variant {index + 1}", "runs": len(samples), "throughput": summary("tput"), "p50": summary("p50_us"), "p99": summary("p99_us"), "fairness": summary("fairness")})
        result["status"] = "complete"
    except Exception as exc:
        result["status"] = "failed"; result["error"] = str(exc)
    save_history()

@app.on_event("startup")
async def load_experiment_history():
    load_history()

@app.get("/experiments")
async def list_experiments():
    return {"catalog": [{"id": key, "name": value[0], "variants": len(value[1])} for key, value in EXPERIMENTS.items()], "history": experiment_history}

@app.post("/experiments/{exp_id}/run")
async def start_experiment(exp_id: str, options: dict | None = None):
    if exp_id not in EXPERIMENTS: return {"status": "error", "detail": "unknown experiment"}
    options = options or {}
    asyncio.create_task(run_experiment(exp_id, max(1, min(10, int(options.get("repeats", 3)))), max(1, min(30, int(options.get("seconds", 3))))))
    return {"status": "started", "id": exp_id}

@app.get("/experiments/export.csv")
async def export_experiments():
    out = io.StringIO(); writer = csv.writer(out); writer.writerow(["experiment", "name", "timestamp", "variant", "runs", "throughput_mean", "throughput_stddev", "p50_mean", "p99_mean", "fairness_mean"])
    for experiment in experiment_history:
        for variant in experiment.get("variants", []): writer.writerow([experiment["id"], experiment["name"], experiment["created_at"], variant["label"], variant["runs"], variant["throughput"]["mean"], variant["throughput"]["stddev"], variant["p50"]["mean"], variant["p99"]["mean"], variant["fairness"]["mean"]])
    return StreamingResponse(iter([out.getvalue()]), media_type="text/csv", headers={"Content-Disposition": "attachment; filename=adaptivex-experiments.csv"})

@app.get("/config")
async def get_config():
    return engine_config

@app.put("/config")
async def set_config(cfg: dict):
    global engine_config
    try:
        engine_config = merged_config(engine_config | cfg)
    except ValueError as exc:
        return {"status": "error", "detail": str(exc)}
    return {"status": "saved", "config": engine_config}

@app.post("/start")
async def start(cfg: dict | None = None):
    try:
        return await start_engine(cfg)
    except ValueError as exc:
        return {"status": "error", "detail": str(exc)}

@app.post("/stop")
async def stop():
    async with engine_lock:
        await stop_engine()
    return {"status": "stopped"}

@app.post("/reset")
async def reset():
    return await start_engine(ENGINE_DEFAULT)

@app.post("/restart")
async def restart(cfg: dict = None):
    return await start_engine(cfg)

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
