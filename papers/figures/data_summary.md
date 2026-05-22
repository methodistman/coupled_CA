# Figure Data Summaries

Install matplotlib/numpy, then run:
```bash
pip install matplotlib numpy
python3 papers/gen_figs.py
```

## Fig 2: A1 Hyperparameter Sweep
Source: `data/a1/sweep2d.csv`
Best: ga_every=1, init_mut=2 → +1909.98%

## Fig 3: A2 Fitness Mode Comparison
Source: `data/a2/{stability,activity,density,edge}.csv`
- density: avg 0.498 (30 trials, all wins)
- edge: avg ~0.35
- activity: avg ~0.10
- stability: avg ~0.02

## Fig 4: A3 Genome Heatmaps
Source: `data/a2/density_*_g1_{rule,weight}.pgm`

## Fig 5: A4 Convergence Dynamics
Source: `data/a4/convergence_ga.csv`
Stabilization after ~200 ticks.

## Fig 6: A5 Static Replay
- Frozen learned: 0.3899
- Continuous GA: 0.3534
- Fresh random: 0.0000
