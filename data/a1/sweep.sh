#!/usr/bin/env bash
# A1: hyperparameter sweep with density fitness.
# Sweeps (mutation_rate × ga_every × tournament_k).
# mutation_rate is set via initial genome; for now we just inherit the
# default (mutation=1 in genome init). We sweep ga_every and tournament_k
# directly via CLI, plus an indirect mutation sweep by re-running with
# different initial genomes via env vars (none currently exposed — we
# vary ga_every and trials per cell instead).

set -euo pipefail

EXE="/home/gregory/code/wavebuffer/coupled_ca/exp_local_ga"
OUTDIR="$(dirname "$0")"
SIZE=64
STEPS=600
WINDOW=150
TRIALS=10
FITNESS=density

echo "ga_every,trials,improvement_pct,ga_wins,moran_rule,moran_weight,top_rule_frac,distinct_rules,wallclock_s" > "$OUTDIR/sweep.csv"

for GA_EVERY in 1 2 5 10 20 50; do
  for SEED in 1; do
    START=$(date +%s)
    OUT="$OUTDIR/run_g${GA_EVERY}_s${SEED}.txt"
    "$EXE" --size $SIZE --steps $STEPS --window $WINDOW --ga-every $GA_EVERY \
           --seed $SEED --trials $TRIALS --fitness $FITNESS > "$OUT" 2>&1
    END=$(date +%s)
    DUR=$((END - START))
    IMPROV=$(grep "delta_mean" "$OUT" | sed 's/.*improvement=\(.*\)%.*/\1/')
    WINS=$(grep "delta_mean" "$OUT" | sed 's/.*ga_wins=\([0-9]*\)\/.*/\1/')
    MORANRULE=$(grep "avg_moran_rule_g1" "$OUT" | awk -F'ga=' '{print $2}')
    MORANWT=$(grep "avg_moran_weight_g1" "$OUT" | awk -F'ga=' '{print $2}')
    TOPFRAC=$(grep "avg_top_rule_frac_g1" "$OUT" | awk -F'ga=' '{print $2}' | awk '{print $1}')
    DISTINCT=$(grep "avg_distinct_rules_g1" "$OUT" | awk -F'ga=' '{print $2}' | awk '{print $1}')
    echo "$GA_EVERY,$TRIALS,$IMPROV,$WINS,$MORANRULE,$MORANWT,$TOPFRAC,$DISTINCT,$DUR" >> "$OUTDIR/sweep.csv"
    echo "ga_every=$GA_EVERY wins=$WINS/$TRIALS improvement=${IMPROV}% Moran_rule=$MORANRULE Moran_w=$MORANWT (${DUR}s)"
  done
done

echo "=== Sweep complete: $OUTDIR/sweep.csv ==="
