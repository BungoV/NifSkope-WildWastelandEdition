"""tests/spells/impostor_trunk.sh's instrument (lane IMPOSTORDEPTH1, 2026-09-23; the bar was written in
scratchpad/impostordepth1_20260923/progress.md before any card number). Env MESHASCARD=1 scores the mesh
against itself: the floor that proves the arithmetic can say PASS.
The pre-registered trunk bar + whole-tree numbers (progress.md, PRE-REGISTERED TRUNK BAR).
    python trunkbar.py <label>=<run dir with v_az%03d_el%02d_{card,mesh}.png> ... [--el 0|20]
A dir may be given as <dir>@<el>. Prints one block per dir; last line per dir = TRUNK PASS/FAIL.
Env STEP=n uses every n-th view (default 1). Env REF_POP / REF_TEAR set the tear bar's stipple
references (worst pop share, worst chunk share) for that elevation, else the bar is not judged."""
import os, sys, json, numpy as np
from PIL import Image
from scipy.ndimage import binary_opening
def mask(p):
    a = np.asarray(Image.open(p).convert('RGB')); return (a != a[0, 0]).any(-1)
def runs(row):
    r = row.astype(np.int8).copy()
    # fill gaps <= 2 px
    d = np.diff(np.r_[1, r, 1]); s = np.where(d == -1)[0]; e = np.where(d == 1)[0]
    for a, b in zip(s, e):
        if 0 < a and b < len(r) and b - a <= 2: r[a:b] = 1
    d = np.diff(np.r_[0, r, 0]); s = np.where(d == 1)[0]; e = np.where(d == -1)[0]
    return int(((e - s) >= 3).sum())
st = np.ones((7, 7), bool)
STEP = int(os.environ.get('STEP', '1'))
out = {}
for arg in sys.argv[1:]:
    lab, d = arg.split('=', 1)
    el = 0
    if '@' in d: d, el = d.rsplit('@', 1); el = int(el)
    az = sorted(int(f[4:7]) for f in os.listdir(d) if f.endswith('el%02d_card.png' % el))[::STEP]
    R, RC, RM, CXc, CXm, IOU, HOLE, TEAR, Cs, Ms = [], [], [], [], [], [], [], [], [], []
    for a in az:
        M = mask(os.path.join(d, 'v_az%03d_el%02d_mesh.png' % (a, el)))
        C = M.copy() if os.environ.get('MESHASCARD') else mask(os.path.join(d, 'v_az%03d_el%02d_card.png' % (a, el)))
        ys = np.where(M.any(1))[0]; y0, y1 = ys.min(), ys.max(); H = y1 - y0
        bA = slice(int(y1 - 0.20 * H), int(y1 - 0.08 * H)); bB = range(int(y1 - 0.35 * H), int(y1 - 0.08 * H))
        mA, cA = M[bA], C[bA]
        R.append(cA.sum() / max(1, mA.sum()))
        RM.append(np.median([runs(M[y]) for y in bB])); RC.append(np.median([runs(C[y]) for y in bB]))
        CXm.append(np.where(mA)[1].mean()); CXc.append(np.where(cA)[1].mean() if cA.any() else np.nan)
        IOU.append((C & M).sum() / (C | M).sum()); HOLE.append((M & ~C).sum() / M.sum())
        TEAR.append(binary_opening(M & ~C, st).sum() / M.sum())
        Cs.append(C); Ms.append(M)
    R = np.array(R); RC = np.array(RC); RM = np.array(RM); CXc = np.array(CXc); CXm = np.array(CXm)
    n = len(az); area = np.mean([m.sum() for m in Ms])
    pop = np.array([(Cs[i] ^ Cs[(i + 1) % n]).sum() for i in range(n)]) / area
    popm = np.array([(Ms[i] ^ Ms[(i + 1) % n]).sum() for i in range(n)]) / area
    dc = np.abs(np.diff(np.r_[CXc, CXc[0]])); dm = np.abs(np.diff(np.r_[CXm, CXm[0]]))
    T1 = bool(np.all((R >= 0.85) & (R <= 1.15)))
    T2 = bool(np.all(RC <= RM))
    T3 = bool(np.all(np.isfinite(dc)) and np.nanmax(dc) <= 2 * dm.max())
    TEAR = np.array(TEAR); o = np.argsort(TEAR)[::-1]
    ok = T1 and T2 and T3
    print(f'[{lab}] el{el:02d} {n} views ({d})')
    print(f'  T1 ratio card/mesh: median {np.median(R):.3f} min {R.min():.3f} (az {az[int(R.argmin())]}) max {R.max():.3f} (az {az[int(R.argmax())]}); outside 0.85..1.15 at {int(np.sum((R<0.85)|(R>1.15)))} of {n} -> {"ok" if T1 else "FAIL"}')
    print(f'  T2 runs (median over band B): card > mesh at {int(np.sum(RC>RM))} of {n}; worst card {RC.max():.1f} vs mesh {RM[int(np.argmax(RC-RM))]:.1f} at az {az[int(np.argmax(RC-RM))]} -> {"ok" if T2 else "FAIL"}')
    print(f'  T3 centre step: card worst {np.nanmax(dc):.2f} px (az {az[int(np.nanargmax(dc))]}) mean {np.nanmean(dc):.2f}; mesh worst {dm.max():.2f} mean {dm.mean():.2f}; ratio {np.nanmax(dc)/dm.max():.2f} (bar 2); missing-centre views {int(np.sum(~np.isfinite(CXc)))} -> {"ok" if T3 else "FAIL"}')
    print(f'  whole tree: IoU {np.mean(IOU):.4f} (min {np.min(IOU):.4f}); missing {np.mean(HOLE):.4f} (max {np.max(HOLE):.4f}); pop worst {pop.max():.4f} mean {pop.mean():.4f} [mesh {popm.max():.4f}/{popm.mean():.4f}]; tear worst {TEAR[o[0]]*100:.2f}% az{az[o[0]]} top5 {TEAR[o[:5]].mean()*100:.2f}% mean {TEAR.mean()*100:.2f}%')
    tb = ''
    rp, rt = os.environ.get('REF_POP'), os.environ.get('REF_TEAR')
    if rp and rt:
        tok = pop.max() <= 1.5 * float(rp) and TEAR.max() <= 2 * float(rt)
        tb = f' | TEAR BAR {"PASS" if tok else "FAIL"} (pop {pop.max():.4f} <= {1.5*float(rp):.4f}; tear {TEAR.max()*100:.2f}% <= {2*float(rt)*100:.2f}%)'
    print(f'  TRUNK {"PASS" if ok else "FAIL"}{tb}', flush=True)
    out[lab + '@%d' % el] = dict(ratio_min=float(R.min()), ratio_max=float(R.max()), ratio_out=int(np.sum((R<0.85)|(R>1.15))),
        runs_over=int(np.sum(RC > RM)), step_ratio=float(np.nanmax(dc) / dm.max()), iou=float(np.mean(IOU)),
        pop=float(pop.max()), tear=float(TEAR.max()), trunk=ok)
if os.environ.get('JSON'):
    json.dump(out, open(os.environ['JSON'], 'w'), indent=1)
