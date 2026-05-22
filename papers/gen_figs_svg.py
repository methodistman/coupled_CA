#!/usr/bin/env python3
"""Generate Paper 1 figures as SVG using only stdlib."""
import csv, os, math

DD = os.path.join(os.path.dirname(__file__), '..', 'data')
OD = os.path.join(os.path.dirname(__file__), 'figures')
os.makedirs(OD, exist_ok=True)

def esc(s):
    return s.replace('&','&amp;').replace('<','&lt;').replace('>','&gt;')

def svg_header(w, h):
    return f'''<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}">
<style>
  text {{ font-family: sans-serif; font-size: 12px; }}
  .title {{ font-size: 14px; font-weight: bold; }}
</style>
'''

def svg_footer():
    return '</svg>'

def line(x1,y1,x2,y2,stroke='#333',width=1):
    return f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{stroke}" stroke-width="{width}"/>'

def rect(x,y,w,h,fill,stroke='none'):
    return f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{fill}" stroke="{stroke}"/>'

def text(x,y,content,anchor='start',cls=''):
    c = f' class="{cls}"' if cls else ''
    return f'<text x="{x}" y="{y}" text-anchor="{anchor}"{c}>{esc(content)}</text>'

def read_csv(path):
    with open(path) as f:
        lines = [l for l in f if not l.startswith('#')]
        return list(csv.DictReader(lines))

# --- Fig 2: A1 Heatmap ---
def fig_a1():
    d = read_csv(os.path.join(DD, 'a1', 'sweep2d.csv'))
    gev = sorted({int(r['ga_every']) for r in d})
    mut = sorted({int(r['init_mut']) for r in d})
    vals = {}
    for r in d:
        vals[(int(r['ga_every']), int(r['init_mut']))] = float(r['improvement_pct'])
    cell_w, cell_h = 70, 35
    margin_l, margin_t = 100, 60
    w = margin_l + len(mut)*cell_w + 40
    h = margin_t + len(gev)*cell_h + 60
    out = [svg_header(w,h)]
    out.append(text(w//2, 25, 'A1: Hyperparameter Sweep (Improvement %)', anchor='middle', cls='title'))
    # Y labels
    for i, g in enumerate(gev):
        out.append(text(margin_l-10, margin_t+i*cell_h+cell_h//2+4, f'GA every {g}', anchor='end'))
    # X labels
    for j, m in enumerate(mut):
        out.append(text(margin_l+j*cell_w+cell_w//2, margin_t-10, f'mut={m}', anchor='middle'))
    # cells
    for i, g in enumerate(gev):
        for j, m in enumerate(mut):
            v = vals.get((g,m), 0)
            # green intensity 0-2000 mapped to 0-255
            intensity = max(0, min(255, int(v / 2000 * 255)))
            fill = f'rgb({255-intensity},{255-intensity//4},{255-intensity//2})'
            out.append(rect(margin_l+j*cell_w, margin_t+i*cell_h, cell_w-2, cell_h-2, fill, '#999'))
            txtcol = '#fff' if v > 1000 else '#000'
            out.append(text(margin_l+j*cell_w+cell_w//2, margin_t+i*cell_h+cell_h//2+4, f'{v:.0f}', anchor='middle'))
    out.append(svg_footer())
    with open(os.path.join(OD, 'fig_a1.svg'), 'w') as f:
        f.write('\n'.join(out))
    print('fig_a1.svg')

# --- Fig 3: A2 Boxplot (simplified as bar+error) ---
def fig_a2():
    modes = ['stability', 'activity', 'density', 'edge']
    labels = ['Stability', 'Activity', 'Density', 'Edge signal']
    stats = []
    for m in modes:
        rows = read_csv(os.path.join(DD, 'a2', f'{m}.csv'))
        vals = [float(r['ga_mean']) for r in rows]
        n = len(vals)
        mean = sum(vals)/n
        var = sum((v-mean)**2 for v in vals)/n
        stats.append((mean, math.sqrt(var), n))
    w, h = 500, 350
    margin_l, margin_b = 80, 60
    chart_w, chart_h = w-margin_l-40, h-margin_b-40
    out = [svg_header(w,h)]
    out.append(text(w//2, 25, 'A2: Fitness Mode Comparison (mean GA signal)', anchor='middle', cls='title'))
    # axes
    out.append(line(margin_l, h-margin_b, margin_l, 40))
    out.append(line(margin_l, h-margin_b, w-20, h-margin_b))
    # Y ticks
    ymax = max(s[0]+s[1] for s in stats) * 1.1
    if ymax < 0.1: ymax = 0.1
    for i in range(6):
        yv = i*ymax/5
        py = h-margin_b - (yv/ymax)*chart_h
        out.append(text(margin_l-8, int(py)+4, f'{yv:.2f}', anchor='end'))
        out.append(line(margin_l-3, int(py), margin_l, int(py), '#ccc'))
    # bars
    bw = chart_w // len(stats) * 0.6
    colors = ['#e74c3c', '#e67e22', '#2ecc71', '#3498db']
    for i, (mean, std, n) in enumerate(stats):
        x = margin_l + (i+0.5)*(chart_w/len(stats)) - bw/2
        y = h - margin_b - (mean/ymax)*chart_h
        bh = (mean/ymax)*chart_h
        out.append(rect(x, y, bw, bh, colors[i]))
        # error bar
        err = (std/ymax)*chart_h
        out.append(line(x+bw/2, y-err, x+bw/2, y, '#333', 2))
        out.append(line(x+bw/2-5, y-err, x+bw/2+5, y-err, '#333', 2))
        out.append(text(x+bw/2, h-margin_b+18, labels[i], anchor='middle'))
        out.append(text(x+bw/2, y-err-8, f'{mean:.3f}', anchor='middle'))
    out.append(svg_footer())
    with open(os.path.join(OD, 'fig_a2.svg'), 'w') as f:
        f.write('\n'.join(out))
    print('fig_a2.svg')

# --- Fig 5: A4 Time series ---
def fig_a4():
    rows = read_csv(os.path.join(DD, 'a4', 'convergence_ga.csv'))
    t = [float(r['tick']) for r in rows]
    dens = [float(r['density_g1']) for r in rows]
    sig = [float(r['signal_g1']) for r in rows]
    fmean = [float(r['fit_mean']) for r in rows]
    rent = [float(r['rule_entropy']) for r in rows]
    went = [float(r['weight_entropy']) for r in rows]
    mrule = [float(r['moran_rule']) for r in rows]
    mweight = [float(r['moran_weight']) for r in rows]
    w, h = 600, 500
    out = [svg_header(w,h)]
    out.append(text(w//2, 20, 'A4: Convergence Dynamics', anchor='middle', cls='title'))
    panels = [
        (dens, sig, 'Density / Signal', 0, 1.0, 40, 180),
        (fmean, None, 'Fitness mean', 0, max(fmean)*1.1, 200, 340),
        (rent, went, 'Entropy', 0, max(max(rent),max(went))*1.1, 360, 500),
    ]
    colors = ['#3498db', '#e74c3c', '#2ecc71']
    for (s1, s2, title, ymin, ymax, ytop, ybot) in panels:
        chart_h = ybot - ytop - 20
        margin_l = 80
        chart_w = w - margin_l - 30
        out.append(text(w//2, ytop-5, title, anchor='middle'))
        out.append(line(margin_l, ybot, margin_l, ytop+10))
        out.append(line(margin_l, ybot, w-20, ybot))
        for i in range(5):
            yv = ymin + i*(ymax-ymin)/4
            py = ybot - (yv-ymin)/(ymax-ymin)*chart_h
            out.append(text(margin_l-5, int(py)+4, f'{yv:.2f}', anchor='end'))
        # plot line
        pts1 = ' '.join(f'{margin_l + (t[i]/t[-1])*chart_w},{ybot - (s1[i]-ymin)/(ymax-ymin)*chart_h}' for i in range(len(t)))
        out.append(f'<polyline points="{pts1}" fill="none" stroke="{colors[0]}" stroke-width="1.5"/>')
        if s2:
            pts2 = ' '.join(f'{margin_l + (t[i]/t[-1])*chart_w},{ybot - (s2[i]-ymin)/(ymax-ymin)*chart_h}' for i in range(len(t)))
            out.append(f'<polyline points="{pts2}" fill="none" stroke="{colors[1]}" stroke-width="1.5"/>')
    out.append(svg_footer())
    with open(os.path.join(OD, 'fig_a4.svg'), 'w') as f:
        f.write('\n'.join(out))
    print('fig_a4.svg')

# --- Fig 6: A5 Bar chart ---
def fig_a5():
    w, h = 400, 300
    labels = ['Frozen learned (a)', 'Continuous GA (b)', 'Fresh random (c)']
    vals = [0.3899, 0.3534, 0.0]
    colors = ['#2ecc71', '#3498db', '#e74c3c']
    margin_l, margin_b = 60, 60
    chart_w = w - margin_l - 40
    chart_h = h - margin_b - 40
    out = [svg_header(w,h)]
    out.append(text(w//2, 20, 'A5: Static Replay Comparison', anchor='middle', cls='title'))
    out.append(line(margin_l, h-margin_b, margin_l, 40))
    out.append(line(margin_l, h-margin_b, w-20, h-margin_b))
    ymax = 0.45
    for i in range(6):
        yv = i*ymax/5
        py = h-margin_b - (yv/ymax)*chart_h
        out.append(text(margin_l-8, int(py)+4, f'{yv:.2f}', anchor='end'))
        out.append(line(margin_l-3, int(py), margin_l, int(py), '#ccc'))
    bw = chart_w // len(vals) * 0.5
    for i, (v, lab, col) in enumerate(zip(vals, labels, colors)):
        x = margin_l + (i+0.5)*(chart_w/len(vals)) - bw/2
        bh = (v/ymax)*chart_h
        y = h - margin_b - bh
        out.append(rect(x, y, bw, bh, col))
        out.append(text(x+bw/2, y-8, f'{v:.4f}', anchor='middle'))
        out.append(text(x+bw/2, h-margin_b+15, lab, anchor='middle'))
    out.append(svg_footer())
    with open(os.path.join(OD, 'fig_a5.svg'), 'w') as f:
        f.write('\n'.join(out))
    print('fig_a5.svg')

if __name__ == '__main__':
    fig_a1(); fig_a2(); fig_a4(); fig_a5()
    print('All SVG figures generated.')
