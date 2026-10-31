"""Lane TREELEAF4 (FIX1031A, 2026-10-31), TAABLENDMOVER1: which velocity should a MOVING BLENDED card write over the
background behind it? Offline, the REAL taa2.frag (Ultra, the tier that reads TAAINPUTS1's velocity + mask) through the
real driver in a hidden GL 4.1 context (TAA3's rig, notes/taa3/sim/taa3_gates.py, loaded without its main). No NifSkope.

Scenes (480x270, 8x8 supersampled reference): a still camera over a background (`grain`: 2 px random cells; `edges`:
8 px bars of 0.2 / 0.8), a 48x32 px card whose alpha falls from 1 at its centre to 0 at its rim and corners (a wing /
glow card: its triangles cover the corners at alpha 0), its texture `fine` (2 cells a px) or `smooth` (a soft gradient),
moving 0.5 / 2 / 6 px a frame on its own. Blend "over" (src alpha, 1 - src alpha) and "add" (src alpha, one).
The app's mask is reproduced as TAAINPUTS1 writes it: R = the card's alpha (taa_reactive.frag), B = 1 where the
object-vector pass wrote (taa_objvel.frag's flag).

Policies for the object vector over the card's triangles (prev - cur, uv):
  full   today's code: every covered pixel takes the card's vector
  covNN  only where the card's composed alpha >= NN/100 (the rest keeps the camera's vector)
  none   blended movers write no object vector
  wavg   alpha-weighted mix of the card's and the camera's vector
Regions (frames 8..31, mean |Y - Yref| of the 8x8 reference composite):
  core   card pixels with alpha >= 0.5          fringe  card-rect pixels with alpha < 0.5 (the background shows)
  trail  8 px behind the card's trailing edge (the background it just left);  bg = (fringe + trail) / 2
Gate (pre-registered before the first run): a policy may replace `full` only if, in EVERY scene, it beats `full` on bg
by >= 10 % and costs the core <= 10 %. Otherwise the row is refused with these numbers and today's code stays.

usage: python -I tests/treeleaf4/blendmover_gate.py <out.txt> [--quick]   (env RIG=<taa3_gates.py>, TREE=<worktree>)
Prints `BLENDMOVER RESULT KEEP full` (no policy passes everywhere: today's code is the measured best) or
`BLENDMOVER RESULT SHIP <policy>`; rc 0 either way (a measurement), 2 on a rig failure."""
import os
import sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.environ.get('TREE', os.path.dirname(os.path.dirname(HERE)))
os.environ['TREE'] = TREE
RIG = os.environ.get('RIG', 'E:/Projects/Claude/notes/taa3/sim/taa3_gates.py')
_src = open(RIG, encoding='utf-8').read().rstrip()
assert _src.endswith('main()')
R = {'__name__': 'taa3_rig', '__file__': RIG}
exec(compile(_src[:-len('main()')], RIG, 'exec'), R)
W, H, SS = R['W'], R['H'], R['SS']
Yf = R['Yf']

CW, CH, X0, Y0 = 48, 32, 100.0, 119.0
rng = np.random.default_rng(11)
_g = rng.random((H // 2 + 2, W // 2 + 2))
_tex = rng.random((CH * 2, CW * 2))
SC = {}


def bg_at(x, y):
    if SC['bg'] == 'edges':
        return np.where((np.floor(x / 8) % 2) == 0, 0.2, 0.8)
    return 0.2 + 0.6 * _g[(np.clip(y, 0, H - 1) / 2).astype(int), (np.clip(x, 0, W - 1) / 2).astype(int)]


def card_at(x, y, f):
    cx = X0 + SC['speed'] * f
    u, v = (x - cx) / CW, (y - Y0) / CH
    inside = (u >= 0) & (u < 1) & (v >= 0) & (v < 1)
    r2 = ((u - 0.5) * 2) ** 2 + ((v - 0.5) * 2) ** 2
    a = np.where(inside, np.clip(1.0 - r2, 0.0, 1.0), 0.0)
    if SC['tex'] == 'fine':
        t = _tex[np.clip((v * CH * 2).astype(int), 0, CH * 2 - 1), np.clip((u * CW * 2).astype(int), 0, CW * 2 - 1)]
    else:
        t = 0.5 + 0.5 * np.sin(u * 6.283)
    return a, 0.55 + 0.4 * t, inside


def compose(x, y, f, blend):
    b = bg_at(x, y)
    a, c, _ = card_at(x, y, f)
    return np.clip(b + a * c, 0, 1) if blend == 'add' else a * c + (1 - a) * b


def grid(jx, jy):
    """pixel p sees the scene at p - jitter (the rig's convention: x right, y up; rows top-down)"""
    return np.meshgrid(np.arange(W) + 0.5 - jx, np.arange(H) + 0.5 + jy)


def frame(f, jx, jy, blend, policy):
    X, Y = grid(jx, jy)
    rgb = np.repeat(compose(X, Y, f, blend)[..., None], 3, 2)
    a, _, inside = card_at(X, Y, f)
    vcard = -SC['speed'] / W
    vel = np.zeros((H, W, 3))
    msk = np.zeros((H, W, 3))
    msk[..., 0] = np.where(a >= 1.0 / 255.0, a, 0.0)	# taa_reactive.frag: R = the layer's alpha
    # ...r: where the card's triangles cover but it writes no vector, the mask says reactive (R = 1);
    # ...q: the same only where its alpha is >= 1/255 (not on the alpha-0 corners)
    react = policy[-1] in 'rq' and policy != 'none'
    p = policy[:-1] if react else policy
    if policy[-1] == 'q':
        inside_r = inside & (a >= 1.0 / 255.0)
    else:
        inside_r = inside
    if p.startswith('cov'):
        on = inside & (a >= int(p[3:]) / 100.0)
    elif p == 'none':
        on = np.zeros_like(inside)
    else:
        on = inside
    vel[..., 0] = np.where(inside, a * vcard, 0.0) if p == 'wavg' else np.where(on, vcard, 0.0)
    msk[..., 2] = on.astype(float)
    if react:
        msk[..., 0] = np.where(inside_r & ~on, 1.0, msk[..., 0])
    return rgb, 0.0, vel, msk


def reference(f, blend):
    acc = 0.0
    for ky in range(SS):
        for kx in range(SS):
            X, Y = grid(-((kx + 0.5) / SS - 0.5), ((ky + 0.5) / SS - 0.5))
            acc = acc + compose(X, Y, f, blend)
    return acc / (SS * SS)


def regions(f):
    X, Y = grid(0.0, 0.0)
    a, _, inside = card_at(X, Y, f)
    cx = X0 + SC['speed'] * f
    trail = (X >= cx - 8) & (X < cx) & (Y >= Y0) & (Y < Y0 + CH)
    return inside & (a >= 0.5), inside & (a < 0.5), trail


def run(rig, blend, policy, refs):
    R['frame_fn'] = lambda f, jx, jy: frame(f, jx, jy, blend, policy)
    outs = list(rig.run(4, 32, W, H, 0))
    e = {'core': [], 'fringe': [], 'trail': []}
    for f in range(8, 32):
        y, yr = Yf(outs[f]), refs[f]
        for k, m in zip(('core', 'fringe', 'trail'), regions(f)):
            e[k].append(np.abs(y[m] - yr[m]).mean())
    r = {k: float(np.mean(v)) for k, v in e.items()}
    r['bg'] = (r['fringe'] + r['trail']) / 2
    return r


def main(a):
    out = a[0] if a and not a[0].startswith('--') else os.path.join(HERE, 'blendmover_gate.txt')
    quick = '--quick' in a
    rig = R['Rig']()
    lines = []
    say = lambda s: (print(s), lines.append(s), sys.stdout.flush())
    say('TREELEAF4 blend-mover gate: real taa2.frag Ultra (inputs) from %s; card %dx%d px' % (TREE, CW, CH))
    pols = ['full', 'cov25', 'cov50', 'cov75', 'none', 'wavg', 'cov50r', 'noner']
    if os.environ.get('POLS'):
        pols = ['full'] + [p for p in os.environ['POLS'].split(',') if p != 'full']
    scenes = [(bg, tex, sp) for bg in ('grain', 'edges') for tex in ('fine', 'smooth') for sp in (0.5, 2.0, 6.0)]
    if quick:
        scenes = [('grain', 'fine', 2.0), ('edges', 'smooth', 2.0)]
    passes = {p: True for p in pols[1:]}
    wins = {p: 0 for p in pols}
    say('%-22s %-5s %s' % ('scene', 'blend', '  '.join('%-13s' % p for p in pols)))
    say('%-22s %-5s %s' % ('', '', '  '.join('%-13s' % 'bg/core' for p in pols)))
    for bg, tex, sp in scenes:
        SC.update(bg=bg, tex=tex, speed=sp)
        for blend in ('over', 'add'):
            refs = [Yf(np.repeat(reference(f, blend)[..., None], 3, 2)) if f >= 8 else None for f in range(32)]
            res = {p: run(rig, blend, p, refs) for p in pols}
            f0 = res['full']
            cells = []
            for p in pols:
                r = res[p]
                cells.append('%.4f/%.4f' % (r['bg'], r['core']))
                if p != 'full':
                    gain = 1 - r['bg'] / max(f0['bg'], 1e-9)
                    cost = r['core'] / max(f0['core'], 1e-9) - 1
                    passes[p] = passes[p] and gain >= 0.10 and cost <= 0.10
            wins[min(pols, key=lambda p: res[p]['bg'] + res[p]['core'])] += 1
            say('%-22s %-5s %s' % ('%s/%s/%.1fpx' % (bg, tex, sp), blend, '  '.join('%-13s' % c for c in cells)))
    say('\nbest (bg + core) per scene: %s' % ', '.join('%s %d' % kv for kv in wins.items()))
    say('policies passing the pre-registered gate in every scene (bg -10 %%, core <= +10 %% vs full): %s' % (
        ', '.join(p for p, v in passes.items() if v) or 'none'))
    winners = [p for p in pols[1:] if passes[p]]
    say('BLENDMOVER RESULT %s' % ('SHIP ' + winners[0] if winners else 'KEEP full'))
    with open(out, 'w') as fo:
        fo.write('\n'.join(lines) + '\n')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv[1:]))
    except SystemExit:
        raise
    except Exception as e:  # noqa: BLE001
        print('BLENDMOVER RIG FAILURE', repr(e))
        sys.exit(2)
