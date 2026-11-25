#!/usr/bin/env bash
# run_all.sh — Run all experiment configs N times with fixed seeds.
# Output: results/<config_name>_<run>.jsonl + summary.csv
#
# Usage: ./experiments/run_all.sh [config_file]
#   e.g. ./experiments/run_all.sh experiments/configs/baseline.yaml

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENGINE="${SCRIPT_DIR}/../engine/build/adaptivex"
RESULTS="${SCRIPT_DIR}/results"
SUMMARY="${RESULTS}/summary.csv"
mkdir -p "$RESULTS"

if [[ ! -x "$ENGINE" ]]; then
    echo "ERROR: engine binary not found at $ENGINE"
    echo "       Run: cd engine && cmake -B build -G Ninja && cmake --build build"
    exit 1
fi

# Parse simple YAML key: value format (no arrays)
get_yaml() { grep "^${1}:" "$2" | head -1 | awk '{print $2}'; }

printf "config,run,seed,avg_tput,avg_p50_us,avg_p99_us,avg_fairness,avg_starved,switches\n" | tee "$SUMMARY"

if [[ $# -gt 1 ]]; then
    echo "Usage: $0 [config_file]" >&2
    exit 2
fi

if [[ $# -eq 1 ]]; then
    config_files=("$1")
    if [[ "${config_files[0]}" != /* ]]; then
        config_files[0]="${PWD}/${config_files[0]}"
    fi
else
    shopt -s nullglob
    config_files=("${SCRIPT_DIR}"/configs/*.yaml)
fi

found_config=false
for cfg_file in "${config_files[@]}"; do
    found_config=true
    if [[ ! -f "$cfg_file" ]]; then
        echo "ERROR: config file not found: $cfg_file" >&2
        exit 1
    fi
    name=$(get_yaml name    "$cfg_file")
    rate=$(get_yaml rate    "$cfg_file")
    burst=$(get_yaml burst  "$cfg_file")
    sec=$(get_yaml seconds  "$cfg_file")
    seed=$(get_yaml seed    "$cfg_file")
    workers=$(get_yaml workers "$cfg_file")
    sync=$(get_yaml sync    "$cfg_file")
    policy=$(get_yaml policy "$cfg_file")
    hw=$(get_yaml high_water "$cfg_file")
    lw=$(get_yaml low_water  "$cfg_file")
    sym=$(get_yaml symbols   "$cfg_file")
    reps=$(get_yaml repeats  "$cfg_file")
    reps="${reps:-3}"

    echo "=== Running: $name ($reps repeats) ===" >&2

    for run in $(seq 1 "$reps"); do
        run_seed=$(( seed + run - 1 ))
        out="${RESULTS}/${name}_run${run}.jsonl"

        echo "  run $run / $reps (seed=$run_seed) → $out" >&2

        "$ENGINE" \
            --rate "$rate" \
            --burst "$burst" \
            --seconds "$sec" \
            --seed "$run_seed" \
            --workers "$workers" \
            --sync "$sync" \
            --policy "$policy" \
            --high-water "$hw" \
            --low-water "$lw" \
            --symbols "$sym" \
            > "$out" 2>/dev/null

        # Compute averages from the JSONL output using awk
        awk -v cfg="$name" -v r="$run" -v s="$run_seed" '
        BEGIN { n=0; st=0; sp50=0; sp99=0; sf=0; ss=0; sw=0 }
        { n++
          split($0, a, /[,:{}"]/)
          for (i in a) {
            if (a[i]=="tput")    { st  += a[i+2]+0 }
            if (a[i]=="p50_us")  { sp50 += a[i+2]+0 }
            if (a[i]=="p99_us")  { sp99 += a[i+2]+0 }
            if (a[i]=="fairness"){ sf   += a[i+2]+0 }
            if (a[i]=="starved") { ss   += a[i+2]+0 }
            if (a[i]=="switches"){ sw    = a[i+2]+0 }
          }
        }
        END {
          if (n>0) printf "%s,%d,%d,%.1f,%.0f,%.0f,%.4f,%.1f,%d\n",
            cfg,r,s, st/n, sp50/n, sp99/n, sf/n, ss/n, sw
        }' "$out" | tee -a "$SUMMARY"
    done
done

if [[ "$found_config" == false ]]; then
    echo "ERROR: no experiment configs found" >&2
    exit 1
fi

echo ""
echo "Done. Results in $RESULTS/" >&2
