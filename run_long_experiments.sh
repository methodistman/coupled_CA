#!/bin/bash
# Long-run experiment suite for coupled_ca
# Outputs: CSV to stdout (redirect to file), glossary to CWD

set -e

echo "=== Long Experiment Run ==="
echo "Started at $(date)"

mkdir -p long_results

echo ""
echo "[1/9] exp_signal — signal transmission"
./exp_signal --grids 2 --size 128 --steps 500 --seed 42 > long_results/signal.csv

echo "[2/9] exp_sync — phase locking"
./exp_sync --grids 2 --size 128 --steps 500 --seed 42 > long_results/sync.csv

echo "[3/9] exp_memory — cyclic memory"
./exp_memory --size 128 --steps 500 --seed 42 > long_results/memory.csv

echo "[4/9] exp_gates — logic gate search"
./exp_gates --rule0 0 --rule1 1 --size 64 --steps 500 --trials 50 > long_results/gates.csv

echo "[5/9] exp_sweep — rule × topology sweep"
./exp_sweep --size 32 --steps 200 --trials 10 --seed 42 --rules 0-8 --topos none,uni,bi,ring > long_results/sweep.csv 2>long_results/sweep_glossary.log

echo "[6/9] exp_payload — payload-aware coupling"
./exp_payload --grids 2 --size 32 --steps 100 --seed 42 --connect 0:bottom->1:top,1.0 > long_results/payload.csv

echo "[7/9] exp_hybrid_bus — gated data bus"
./exp_hybrid_bus --size 32 --steps 200 --seed 42 --xconnect 'B0:bottom->P0:top' > long_results/hybrid_bus.csv

echo "[8/9] exp_hybrid_glider — payload on moving patterns"
./exp_hybrid_glider --size 64 --steps 200 --seed 42 --payload 200 > long_results/hybrid_glider.csv

echo "[9/9] exp_hybrid_reservoir — reservoir + glossary clustering"
./exp_hybrid_reservoir --size 32 --steps 100 --trials 10 --seed 42 > long_results/hybrid_reservoir.csv 2>long_results/reservoir_glossary.log

echo ""
echo "=== Done ==="
echo "Results in long_results/"
echo "Glossary files: glossary_sweep.md, glossary_reservoir.md"
ls -lh long_results/
