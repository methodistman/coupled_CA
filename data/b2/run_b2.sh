#!/bin/bash
# B2: Intent-mode comparison
# Runs exp_signal for each intent mode, averages over 30 seeds

cd /home/gregory/code/wavebuffer/coupled_ca

MODES="replace add weighted threshold"
STEPS=500
SIZE=64
SEEDS=30

for mode in $MODES; do
    echo "# mode=$mode seeds=$SEEDS steps=$STEPS"
    for seed in $(seq 1 $SEEDS); do
        ./exp_signal --size $SIZE --steps $STEPS --grids 2 \
            --connect "0:bottom->1:top" \
            --pipeline analyze \
            --intent-mode $mode \
            --seed $seed
    done > data/b2/${mode}.csv
done

echo "B2 complete."
