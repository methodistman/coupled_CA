#!/usr/bin/env bash
# A1 2D sweep: (ga_every × initial_mutation) with density fitness.
set -euo pipefail

EXE="/home/gregory/code/wavebuffer/coupled_ca/exp_local_ga"
OUTDIR="$(dirname "$0")"
SIZE=64
STEPS=600
WINDOW=150
TRIALS=10
FITNESS=density

echo "ga_every,init_mut,improvement_pct,ga_wins,moran_rule,moran_weight,top_rule_frac,distinct" > "$OUTDIR/sweep2d.csv"

for GA_EVERY in 1 2 5 10 20; do
  for MUT in 0 1 2 4 8; do
    OUT="$OUTDIR/run2d_g${GA_EVERY}_m${MUT}.txt"
    "$EXE" --size $SIZE --steps $STEPS --window $WINDOW --ga-every $GA_EVERY \
           --seed 1 --trials $TRIALS --fitness $FITNESS \
           --initial-mutation $MUT > "$OUT" 2>&1
    IMPROV=$(grep "delta_mean" "$OUT" | sed 's/.*improvement=\(.*\)%.*/\1/')
    WINS=$(grep "delta_mean" "$OUT" | sed 's/.*ga_wins=\([0-9]*\)\/.*/\1/')
    MORANRULE=$(grep "avg_moran_rule_g1" "$OUT" | awk -F'ga=' '{print $2}')
    MORANWT=$(grep "avg_moran_weight_g1" "$OUT" | awk -F'ga=' '{print $2}')
    TOPFRAC=$(grep "avg_top_rule_frac_g1" "$OUT" | awk -F'ga=' '{print $2}' | awk '{print $1}')
    DISTINCT=$(grep "avg_distinct_rules_g1" "$OUT" | awk -F'ga=' '{print $2}' | awk '{print $1}')
    echo "$GA_EVERY,$MUT,$IMPROV,$WINS,$MORANRULE,$MORANWT,$TOPFRAC,$DISTINCT" >> "$OUTDIR/sweep2d.csv"
    printf "ga_every=%-3d mut=%-2d wins=%2s/%2d improv=%s%% Mrule=%s Mw=%s\n" \
        $GA_EVERY $MUT $WINS $TRIALS "$IMPROV" "$MORANRULE" "$MORANWT"
  done
done

echo "=== Sweep done: $OUTDIR/sweep2d.csv ==="
