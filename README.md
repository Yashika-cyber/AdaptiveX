# AdaptiveX

AdaptiveX is a C++17 order-matching and scheduling simulator with a Python
WebSocket bridge and a React dashboard. The C++ engine owns order generation,
scheduling, matching, synchronization, cache/fairness measurements, and JSON
metrics. The Python service only launches the engine and forwards its output.

## Project layout

```text
engine/       C++ headers, sources, CMake build, and tests
server/       FastAPI WebSocket bridge
dashboard/    React + Vite dashboard
experiments/  YAML run configurations and batch runner
```

The selectable `--sync` mode applies to the generator-to-scheduler ingress
channel. `locked` uses a mutex queue; `lockfree` uses a bounded C++ atomic
queue. Policy scheduler queues and per-symbol order books remain mutex
protected. The WAL appends JSON-line records to `adaptivex.wal`, calls `fsync`
at each flush boundary, and drains pending records during shutdown.

## Requirements

- CMake 3.20+
- C++17 compiler (Xcode Command Line Tools on macOS, build-essential on Ubuntu)
- Python 3.11+
- Node.js 20+

On macOS, install prerequisites with:

```bash
xcode-select --install
brew install cmake python@3.12 node
```

## Build and verify the C++ engine

From the project root:

```bash
make build
ctest --test-dir engine/build --output-on-failure
```

Run a short standalone simulation:

```bash
./engine/build/adaptivex --rate 1000 --burst 5000 --seconds 3 \
  --seed 42 --workers 4 --sync locked --policy adaptive
```

The engine writes one JSON snapshot per second to stdout. Confirm each line is
valid JSON and contains `tput`, `p50_us`, `p99_us`, and `fairness`.

Useful options:

```text
--rate N --burst N --seconds N --seed N --workers N
--sync locked|lockfree --policy priority|sjf|rr|adaptive
--high-water N --low-water N --symbols N
```

`low-water` must be less than `high-water`; symbol count must be 1–20.

## Run the API and dashboard

Build the engine first. In terminal 1, start the API:

```bash
cd server
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements.txt
uvicorn main:app --host 127.0.0.1 --port 8000
```

Verify the API responds:

```bash
curl http://127.0.0.1:8000/health
```

In terminal 2, start the React app:

```bash
cd dashboard
npm install
npm run dev
```

Open the Vite URL shown in the terminal (normally
`http://localhost:5173`). The dashboard connects to
`ws://localhost:8000/ws`. If the engine binary is missing, the server emits
synthetic demo metrics instead; build the engine to see real engine output.
The dashboard now has Overview, Performance, Fairness, Engine Internals, and
Order Book tabs. Order Book receives real aggregated bid/ask levels from the
C++ engine rather than generated sample data. Overview and Performance expose
validated controls for rate, burst rate, workers, symbols, synchronization,
policy, and watermarks, with Start, Stop, and Reset actions.

## API controls

```text
GET  /config       Current engine configuration
PUT  /config       Validate and save a configuration
POST /start        Start/restart with an optional configuration body
POST /stop         Stop the current run
POST /reset        Restore defaults and start a fresh run
POST /restart      Compatibility alias for /start
```

Snapshots include p50/p90/p95/p99 latency, the actual cumulative WAL flush
count (`wal_flushes`), and a `book` object with a selected symbol plus top-five
bid and ask levels.

## Experiments

The sample configurations are in `experiments/configs/`. Build the engine,
then run:

```bash
bash experiments/run_all.sh
```

Results are written under `experiments/results/`. Each configuration repeats
the specified duration and repeat count, so a full run can take several
minutes.

## Sanitizers

AddressSanitizer and ThreadSanitizer build targets are available:

```bash
make asan
make tsan
```

Sanitizer support depends on the selected compiler/runtime. For final
ThreadSanitizer checks, use a supported Linux toolchain if macOS reports a
runtime limitation.
