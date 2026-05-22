#!/bin/bash
# Pulse-and-reset sweep: 4 concurrent runs max
# Reset intervals, seeds, coherence weights

BASE="/home/gregory/code/wavebuffer/coupled_ca"
OUT="$BASE/data/sweep_pulse"
BIN="$BASE/exp_composable_stage0"

RESETS=(10 15 20 30)
SEEDS=(42 123 2025 9999)
COHS=(0.0 0.3)

MAX_JOBS=4
job_count=0

echo "Starting pulse-reset sweep: $(date)"
echo "Output dir: $OUT"

for R in "${RESETS[@]}"; do
  for S in "${SEEDS[@]}"; do
    for C in "${COHS[@]}"; do
      TAG="R_${R}_s_${S}_c_${C}"
      CSV="$OUT/${TAG}.csv"
      LOG="$OUT/${TAG}.log"
      echo "Launching $TAG ..."
      "$BIN" --size 32 --pop 100 --gens 100 --steps 50 --kfields 8 \
             --seed "$S" --fitness 3 \
             --reset-interval "$R" --path-coherence "$C" \
             > "$CSV" 2> "$LOG" &
      job_count=$((job_count + 1))
      if [ $((job_count % MAX_JOBS)) -eq 0 ]; then
        echo "Batch $((job_count / MAX_JOBS)) running (4 jobs), waiting ..."
        wait
      fi
    done
  done
done

# Wait for any remaining jobs
echo "Waiting for final batch ..."
wait
echo "Sweep complete: $(date)"
