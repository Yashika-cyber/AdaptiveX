# AdaptiveX — Top-level Makefile
# Targets: build, test, tsan, asan, server, dashboard, clean

BUILD_DIR := engine/build
ENGINE    := $(BUILD_DIR)/adaptivex

.PHONY: all build test tsan asan server dashboard clean run experiments help

all: build

## Build the engine (Release)
build:
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release engine
	cmake --build $(BUILD_DIR) --target adaptivex test_scheduler test_fairness test_generator test_order_book test_sync_queue
	@echo "✓ Engine built: $(ENGINE)"

## Run unit tests
test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure
	@echo "✓ All tests passed"

## Build + run with ThreadSanitizer
tsan:
	cmake -B engine/build-tsan -G Ninja -DCMAKE_BUILD_TYPE=Debug engine
	cmake --build engine/build-tsan --target adaptivex_tsan
	engine/build-tsan/adaptivex_tsan --rate 500 --seconds 10 --seed 42

## Build + run with AddressSanitizer
asan:
	cmake -B engine/build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug engine
	cmake --build engine/build-asan --target adaptivex_asan
	engine/build-asan/adaptivex_asan --rate 500 --seconds 10 --seed 42

## Start the FastAPI server (requires Python 3.11+)
server: build
	cd server && \
	  python3 -m venv .venv && \
	  source .venv/bin/activate && \
	  pip install -q -r requirements.txt && \
	  uvicorn main:app --host 0.0.0.0 --port 8000 --reload

## Start the React dashboard dev server
dashboard:
	cd dashboard && npm install && npm run dev

## Run a quick 30s smoke test and print metrics to stdout
run: build
	$(ENGINE) --rate 1000 --seconds 30 --seed 42 --policy adaptive

## Run all experiments (Linux/Ubuntu recommended for benchmarks)
experiments: build
	bash experiments/run_all.sh

## Remove generated build and dependency artifacts
clean:
	rm -rf engine/build engine/build-tsan engine/build-asan
	rm -rf server/.venv
	rm -rf dashboard/node_modules dashboard/dist
	rm -f experiments/results/*.jsonl

help:
	@grep -E '^##' Makefile | sed 's/## /  /'
