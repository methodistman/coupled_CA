#!/usr/bin/env bash
# Large parameter sweep for exp_composable_stage0
# Target: ~10 GB of CSV traces
#
# Fixed: size=32, pop=100, gens=2000, steps=50, kfields=8, elite=2, tour=3
# Varied: seed, mut, reset-interval, path-coherence, fitness-version
#
# Runs 2 sweeps in parallel via background jobs.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTDIR="${SCRIPT_DIR}/large_sweep_dataset"
EXE="${SCRIPT_DIR}/exp_composable_stage0"

mkdir -p "$OUTDIR"

# --- Parameter grids ---
MUTS=(0.05 0.10 0.15 0.20 0.30)
RESETS=(1 5 10 15 20 25 30)
COHERENCES=(0.0 0.1 0.2 0.3 0.5)
FITNESSES=(2 3)
SEEDS_PER_COMBO=280   # 350 combos × 280 seeds ≈ 98,000 runs → ~10 GB

# Base seed offset to spread seeds across a large range
BASE_SEED=100000

# --- Parallelism control ---
MAX_JOBS=2

run_one() {
    local fitness="$1"
    local mut="$2"
    local reset="$3"
    local coherence="$4"
    local seed="$5"

    local fname="f${fitness}_m${mut}_r${reset}_c${coherence}_s${seed}.csv"
    local outpath="${OUTDIR}/${fname}"

    # Skip if already exists (resumable)
    if [[ -f "$outpath" ]]; then
        return 0
    fi

    "$EXE" \
        --size 32 \
        --pop 100 \
        --gens 2000 \
        --steps 50 \
        --kfields 8 \
        --mut "$mut" \
        --elite 2 \
        --tour 3 \
        --seed "$seed" \
        --fitness "$fitness" \
        --reset-interval "$reset" \
        --path-coherence "$coherence" \
        > "$outpath" 2>"${outpath%.csv}.err"
}
export -f run_one
export EXE OUTDIR BASE_SEED

# --- Main sweep ---
echo "Starting large sweep..."
echo "Output dir: $OUTDIR"
echo "Total combos: $((${#FITNESSES[@]} * ${#MUTS[@]} * ${#RESETS[@]} * ${#COHERENCES[@]}))"
echo "Seeds per combo: $SEEDS_PER_COMBO"
echo "Max parallel jobs: $MAX_JOBS"
echo ""

declare -a PIDS=()
total_runs=0

for fitness in "${FITNESSES[@]}"; do
    for mut in "${MUTS[@]}"; do
        for reset in "${RESETS[@]}"; do
            for coherence in "${COHERENCES[@]}"; do
                for s in $(seq 0 $((SEEDS_PER_COMBO - 1))); do
                    seed=$((BASE_SEED + s))

                    # Wait for a job slot if at capacity (robust: reap finished PIDs)
                    while [[ ${#PIDS[@]} -ge $MAX_JOBS ]]; do
                        new_pids=()
                        for pid in "${PIDS[@]}"; do
                            if kill -0 "$pid" 2>/dev/null; then
                                new_pids+=("$pid")
                            fi
                        done
                        PIDS=("${new_pids[@]}")
                        if [[ ${#PIDS[@]} -ge $MAX_JOBS ]]; then
                            sleep 0.1
                        fi
                    done

                    run_one "$fitness" "$mut" "$reset" "$coherence" "$seed" &
                    PIDS+=("$!")
                    total_runs=$((total_runs + 1))

                    # Progress heartbeat every 100 runs
                    if (( total_runs % 100 == 0 )); then
                        echo "[$total_runs] f=$fitness m=$mut r=$reset c=$coherence s=$seed  jobs=${#PIDS[@]}"
                    fi
                done
            done
        done
    done
done

# Wait for all remaining jobs
echo "Waiting for remaining jobs..."
wait
echo "Sweep complete. Total runs: $total_runs"
echo "Data in: $OUTDIR"
ls -lh "$OUTDIR" | head -20
du -sh "$OUTDIR"
