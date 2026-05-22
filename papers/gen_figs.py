#!/usr/bin/env python3
"""Generate Paper 1 figures."""
import csv, os, sys
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

DD = os.path.join(os.path.dirname(__file__), '..', 'data')
OD = os.path.join(os.path.dirname(__file__), 'figures')
os.makedirs(OD, exist_ok=True)

def read_csv(path):
    with open(path) as f:
        return list(csv.DictReader(f))

def fig_a1():
    d = read_csv(os.path.join(DD, 'a1', 'sweep2d.csv'))
    gev = sorted({int(r['ga_every']) for r in d})
    mut = sorted({int(r['init_mut']) for r in d})
    Z = np.zeros((len(gev), len(mut)))
    for r in d:
        Z[gev.index(int(r['ga_every'])), mut.index(int(r['init_mut']))] = float(r['improvement_pct'])
    fig, ax = plt.subplots(figsize=(6,4))
    im = ax.imshow(Z, cmap='RdYlGn', aspect='auto', vmin=0, vmax=2000)
    ax.set_xticks(range(len(mut))); ax.set_xticklabels(mut)
    ax.set_yticks(range(len(gev))); ax.set_yticklabels(gev)
    ax.set_xlabel('Initial mutation rate'); ax.set_ylabel('GA interval (ticks)')
    ax.set_title('Signal improvement (%)')
    for i in range(len(gev)):
        for j in range(len(mut)):
            ax.text(j, i, f'{Z[i,j]:.0f}', ha='center', va='center', fontsize=7, color='white' if Z[i,j] > 1000 else 'black')
    plt.colorbar(im, ax=ax, label='Improvement %')
    plt.tight_layout(); plt.savefig(os.path.join(OD, 'fig_a1.png'), dpi=300); plt.close()
    print('fig_a1.png')

def fig_a2():
    modes = ['stability', 'activity', 'density', 'edge']
    vals = []
    for m in modes:
        rows = read_csv(os.path.join(DD, 'a2', f'{m}.csv'))
        vals.append([float(r['ga_mean']) for r in rows])
    fig, ax = plt.subplots(figsize=(6,4))
    bp = ax.boxplot(vals, labels=modes, patch_artist=True, showmeans=True)
    for p, c in zip(bp['boxes'], ['#e74c3c','#e67e22','#2ecc71','#3498db']):
        p.set_facecolor(c); p.set_alpha(0.6)
    ax.set_ylabel('Mean signal (GA)'); ax.set_title('Fitness mode comparison')
    ax.axhline(0, color='gray', ls='--', lw=0.5)
    plt.tight_layout(); plt.savefig(os.path.join(OD, 'fig_a2.png'), dpi=300); plt.close()
    print('fig_a2.png')

def read_pgm(p):
    with open(p, 'rb') as f:
        assert f.readline().strip() == b'P5'
        l = f.readline()
        while l.startswith(b'#'): l = f.readline()
        w, h = map(int, l.split())
        f.readline()
        return np.frombuffer(f.read(w*h), np.uint8).reshape((h,w))

def fig_a3():
    pairs = [('Baseline rule', 'density_base_g1_rule.pgm'), ('Evolved rule', 'density_ga_g1_rule.pgm'),
             ('Baseline weight', 'density_base_g1_weight.pgm'), ('Evolved weight', 'density_ga_g1_weight.pgm')]
    fig, axes = plt.subplots(2,2, figsize=(7,6))
    for ax, (title, fn) in zip(axes.flat, pairs):
        d = read_pgm(os.path.join(DD, 'a2', fn))
        im = ax.imshow(d, cmap='viridis', interpolation='nearest')
        ax.set_title(title); ax.axis('off')
        plt.colorbar(im, ax=ax, fraction=0.046)
    plt.tight_layout(); plt.savefig(os.path.join(OD, 'fig_a3.png'), dpi=300); plt.close()
    print('fig_a3.png')

def fig_a4():
    rows = read_csv(os.path.join(DD, 'a4', 'convergence_ga.csv'))
    t = [float(r['tick']) for r in rows]
    fig, axes = plt.subplots(3,1, figsize=(8,7), sharex=True)
    axes[0].plot(t, [float(r['density_g1']) for r in rows], label='Density', color='C0')
    axes[0].plot(t, [float(r['signal_g1']) for r in rows], label='Signal', color='C1')
    axes[0].set_ylabel('Density / Signal'); axes[0].legend(); axes[0].set_title('Convergence dynamics (GA active)')
    axes[1].plot(t, [float(r['fit_mean']) for r in rows], color='C2'); axes[1].set_ylabel('Fitness mean')
    axes[1].axvline(200, color='gray', ls='--', lw=0.8, label='~convergence')
    axes[2].plot(t, [float(r['rule_entropy']) for r in rows], label='Rule entropy', color='C3')
    axes[2].plot(t, [float(r['weight_entropy']) for r in rows], label='Weight entropy', color='C4')
    axes[2].plot(t, [float(r['moran_rule']) for r in rows], label='Moran rule', color='C5')
    axes[2].plot(t, [float(r['moran_weight']) for r in rows], label='Moran weight', color='C6')
    axes[2].set_xlabel('Tick'); axes[2].set_ylabel('Entropy / Moran I'); axes[2].legend(fontsize=8)
    plt.tight_layout(); plt.savefig(os.path.join(OD, 'fig_a4.png'), dpi=300); plt.close()
    print('fig_a4.png')

def fig_a5():
    fig, ax = plt.subplots(figsize=(5,4))
    labels = ['Frozen learned\n(a)', 'Continuous GA\n(b)', 'Fresh random\n(c)']
    vals = [0.3899, 0.3534, 0.0]
    colors = ['#2ecc71', '#3498db', '#e74c3c']
    bars = ax.bar(labels, vals, color=colors, alpha=0.8)
    ax.set_ylabel('Mean signal'); ax.set_title('Static replay comparison')
    for bar, v in zip(bars, vals):
        ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.01, f'{v:.3f}', ha='center', va='bottom')
    plt.tight_layout(); plt.savefig(os.path.join(OD, 'fig_a5.png'), dpi=300); plt.close()
    print('fig_a5.png')

if __name__ == '__main__':
    fig_a1(); fig_a2(); fig_a3(); fig_a4(); fig_a5()
    print('All figures generated.')
