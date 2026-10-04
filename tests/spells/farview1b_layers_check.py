#!/usr/bin/env python3
"""FARVIEW1b twin 1: switchable far lights as summed layers. docs/cloud/FARVIEW1b_DESIGN.md.

  python3 tests/spells/farview1b_layers_check.py           # green + every red; exit 0 only when green passes
                                                           # every check AND every red fails the check it targets
  FARVIEW1B_RED=<name> python3 tests/spells/farview1b_layers_check.py   # one red alone; exit 1 when it fails

Light is linear: with the geometry fixed, the settled surfel light of a set of lights is the sum of the settled
light of each part of the set. So the far light is stored as an always-on layer plus one layer per SWITCH GROUP,
and the runtime sums the layers whose group is on. This twin proves that on the synthetic street of
farview1_check.py (imported, not copied: same scene, surfels, light evaluator, tracer), with 500 lights, an
enable-parent graph built in code, and four fixed bounce passes:

  G  group key   collapsing each light's enable-parent chain to (root, parity) gives the same on/off state as
                 walking the chain light by light (the cell view's start rule), for six root states
  L  layers      for five switch states: always-on layer + the layers of the groups that are on == a full rebake
                 (fresh direct light, fresh bounce rays, same passes) with only the lights that are on;
                 max |difference| / max |rebake| <= 1e-6 (float64)
  Q  stored      the same sum from what the files would hold (always-on layer trimmed at 1e-4, switched layers
                 at 3e-5 of the all-on p99, packed RGB9E5, summed in float32) against the rebake on lit surfels
                 (relative error with farview1's floor, 2% of p95): p99 <= 1%, total energy within 0.5%
  X  shared rec  the GPU relight's direct kernel fed from the shared light record (parameters + visible
                 surfel-light pairs, precomputed once) equals the bake's direct light for every state (<= 1e-9)

Reds (each must FAIL its targets):
  drop       one group's layer (the smallest one that is on) left out of the sum          -> L
  settle     layers and rebake each settled to BOUNCE2's bar (1e-3 of the brightest)
             instead of a fixed pass count: the stop rule is not linear                  -> L
  nobounce   the switch layers hold direct light only                                     -> L
  noparity   the group key ignores the XESP opposite bits                                 -> G
  nopairvis  the pairs keep no shadow test (every surfel in reach counts as visible)      -> X
Peak memory about 170 MB; about 80 s.
"""
import os
import sys
import time

import numpy as np

try:
    import resource  # Unix: peak RSS from getrusage (KB on Linux)

    def peak_mb():
        return peak_mb()
except ImportError:  # Windows (lane FARVIEW1): no 'resource'; the peak working set from the process' own counters
    import ctypes
    import ctypes.wintypes as _wt

    class _PMC(ctypes.Structure):
        _fields_ = [('cb', _wt.DWORD), ('PageFaultCount', _wt.DWORD), ('PeakWorkingSetSize', ctypes.c_size_t),
                    ('WorkingSetSize', ctypes.c_size_t), ('QuotaPeakPagedPoolUsage', ctypes.c_size_t),
                    ('QuotaPagedPoolUsage', ctypes.c_size_t), ('QuotaPeakNonPagedPoolUsage', ctypes.c_size_t),
                    ('QuotaNonPagedPoolUsage', ctypes.c_size_t), ('PagefileUsage', ctypes.c_size_t),
                    ('PeakPagefileUsage', ctypes.c_size_t)]

    def peak_mb():
        c = _PMC()
        c.cb = ctypes.sizeof(c)
        k32 = ctypes.windll.kernel32
        k32.GetCurrentProcess.restype = ctypes.c_void_p   # the pseudo-handle -1: an int return would truncate it
        k32.K32GetProcessMemoryInfo.argtypes = [ctypes.c_void_p, ctypes.POINTER(_PMC), _wt.DWORD]
        if not k32.K32GetProcessMemoryInfo(k32.GetCurrentProcess(), ctypes.byref(c), c.cb):
            return 0.0
        return c.PeakWorkingSetSize / (1024 * 1024)

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import farview1_check as fv  # noqa: E402  (scene, surfels, tracer, light evaluator: shared with FARVIEW1)

# ---------------------------------------------------------------- pre-registered bars (set before the first run)
BAR_L = 1e-6           # layers vs rebake, max abs / max (the brief's tolerance)
BAR_Q_P99 = 0.01       # stored layers vs rebake, relative error p99 on lit surfels
BAR_Q_ENERGY = 0.005   # stored layers vs rebake, |sum ratio - 1|
BAR_X = 1e-9           # GPU-relight direct from the shared record vs the bake's direct (float64)
# The trim (a storage knob, not a bar): an entry is kept when its luminance > trim x p99 of the all-on light.
# FVL v1 kept surfels over 1e-3. A first run with 1e-3 for every layer failed Q (each switched layer drops its
# own dim tail, and the tails of the groups that overlap a surfel add up); the trade-off table this script prints
# chose 1e-4 for the always-on layer and 3e-5 for the switched layers (design doc section 3.5).
TRIM_V1 = 1e-3
TRIM_BASE = 1e-4
TRIM_LAYER = 3e-5
Q_FLOOR = 0.02         # Q's relative error has the floor farview1 uses: 0.02 x p95 of the state's light
PASSES = 4             # bounce passes, fixed (design section 3.4)
SETTLE_BAR = 1e-3      # BOUNCE2's stop rule, used only by the red `settle`
K_RAYS = 64            # bounce rays a surfel (as farview1)
N_LIGHTS = 500
SECTOR = 4096.0


# ---------------------------------------------------------------- RGB9E5 (EXT_texture_shared_exponent)
def rgb9e5_encode(c):
    N, B, EMAX = 9, 15, 31
    mx = (2 ** N - 1) / 2 ** N * 2.0 ** (EMAX - B)
    c = np.clip(np.asarray(c, np.float64), 0, mx)
    maxc = c.max(1)
    ep = np.maximum(-B - 1, np.floor(np.log2(np.maximum(maxc, 1e-30)))) + 1 + B
    maxm = np.floor(maxc / 2.0 ** (ep - B - N) + 0.5)
    e = np.where(maxm == 2 ** N, ep + 1, ep)
    m = np.floor(c / 2.0 ** (e - B - N)[:, None] + 0.5).astype(np.uint32)
    return (m[:, 0] | (m[:, 1] << 9) | (m[:, 2] << 18) | (e.astype(np.uint32) << 27)).astype(np.uint32)


def rgb9e5_decode(u):
    u = np.asarray(u, np.uint32)
    e = (u >> 27).astype(np.float64)
    s = 2.0 ** (e - 15 - 9)
    return np.stack([(u & 511) * s, ((u >> 9) & 511) * s, ((u >> 18) & 511) * s], 1)


# ---------------------------------------------------------------- the enable-parent graph, made in code
KIND_ALWAYS, KIND_PARENT, KIND_SELF = 0, 1, 2
KIND_NAME = {KIND_ALWAYS: 'always', KIND_PARENT: 'parent', KIND_SELF: 'self'}


def make_switch_graph(scene, lights, seed=31):
    """Each light is a reference. Wall lights of some buildings hang under that building's 'power' marker,
    a quarter of them with the XESP opposite bit (emergency lights: on when the power is off); street lamps in
    one stretch hang under a 'quest' marker; some through an intermediate reference (a chain); a dozen others
    are switched one by one (scripted). Everything else is always on."""
    rng = np.random.default_rng(seed)
    n = len(lights['r'])
    parent = {}    # ref -> (parent ref, opposite bit)
    scripted = set()
    nb = len(scene['lo'])
    # the building a wall light sits on: nearest box in x/y
    ctr = (scene['lo'] + scene['hi']) / 2
    power = {b: 100000 + b for b in range(nb) if rng.random() < 0.45}
    quest, nxt = 200000, 300000
    for i in range(n):
        p = lights['pos'][i]
        if i % 2 == 1:
            b = int(np.argmin(np.abs(ctr[:, 0] - p[0]) / (scene['hi'][:, 0] - scene['lo'][:, 0])
                              + 10 * (np.sign(ctr[:, 1]) != np.sign(p[1]))))
            if b in power:
                opp = int(rng.random() < 0.25)
                if rng.random() < 0.3:          # through an intermediate reference with its own bit
                    mid = nxt; nxt += 1
                    o2 = int(rng.random() < 0.5)
                    parent[mid] = (power[b], o2)
                    parent[i] = (mid, opp ^ o2)  # net parity = opp
                else:
                    parent[i] = (power[b], opp)
                continue
        elif 2048 <= p[0] < 4096:
            if rng.random() < 0.3:
                mid = nxt; nxt += 1
                parent[mid] = (quest, 1)
                parent[i] = (mid, 1)             # 1 xor 1: on with the quest marker
            else:
                parent[i] = (quest, 0)
            continue
        if rng.random() < 0.03:
            scripted.add(i)
    roots = sorted({r for r in list(power.values()) + [quest]})
    return dict(parent=parent, scripted=scripted, roots=roots, n=n)


def group_key(graph, ref, red=''):
    """Walk the chain to its root; parity = xor of the opposite bits. Key = (kind, root, parity)."""
    par, node = 0, ref
    while node in graph['parent']:
        node, o = graph['parent'][node]
        par ^= (0 if red == 'noparity' else o)
    if node != ref:
        return (KIND_PARENT, node, par)
    if ref in graph['scripted']:
        return (KIND_SELF, ref, 0)
    return (KIND_ALWAYS, 0, 0)


def shown_by_chain(graph, ref, root_on, self_on):
    """The cell view's start rule (docs/PRTP_PLAN.md, enable parents): a child is shown when its parent's state
    differs from the XESP opposite bit; walked reference by reference, no shortcut."""
    if ref in graph['parent']:
        p, o = graph['parent'][ref]
        return shown_by_chain(graph, p, root_on, self_on) != bool(o)
    if ref in root_on:
        return root_on[ref]
    if ref in graph['scripted']:
        return self_on[ref]
    return True


def group_on(key, root_on, self_on):
    kind, ref, par = key
    if kind == KIND_ALWAYS:
        return True
    if kind == KIND_SELF:
        return self_on[ref]
    return root_on[ref] != bool(par)


# ---------------------------------------------------------------- bake pieces
def subset(lights, idx):
    return {k: v[idx] for k, v in lights.items()}


def bounce_hits(scene, S, tab, seed):
    """Every surfel's K_RAYS cosine rays, traced once: the surfel each one lands on (-1 = none). Geometry only."""
    rng = np.random.default_rng(seed)
    n = len(S['p'])
    J = np.full((n, K_RAYS), -1, np.int64)
    CH = 1500
    for s in range(0, n, CH):
        p, nn = S['p'][s:s + CH], S['n'][s:s + CH]
        D = fv.cos_dirs(nn, K_RAYS, rng)
        O = np.repeat(p + nn * 0.5, K_RAYS, 0)
        t, side, _ = fv.trace(scene, O, D)
        hit = np.isfinite(t)
        j = np.full(len(O), -1, np.int64)
        j[hit] = tab.find(O[hit] + D[hit] * t[hit, None], side[hit])
        J[s:s + CH] = j.reshape(len(p), K_RAYS)
    return J


def settle(alb, Ed, J, red=''):
    """B_0 = a E_d; each pass: E_b = mean of B over the surfel's rays (as farview1's one bounce), B = a (E_d + E_b).
    Fixed PASSES passes; the red `settle` stops at BOUNCE2's bar instead."""
    B = alb * Ed
    Eb = np.zeros_like(Ed)
    passes = 0
    for k in range(64 if red == 'settle' else PASSES):
        Bp = np.vstack([B, np.zeros((1, 3))])
        Eb = Bp[J].mean(1)
        Bn = alb * (Ed + Eb)
        passes += 1
        done = red == 'settle' and np.max(np.abs(Bn - B)) <= SETTLE_BAR * max(np.max(Bn), 1e-30)
        B = Bn
        if done:
            break
    return Ed + Eb, passes


def rebake(scene, lights, on, S, seed, red=''):
    """THE REFERENCE: a full bake of the lights that are on, from scratch: a fresh surfel table, fresh direct
    light (one call over the whole on-set), fresh bounce rays (same seed: the bake is deterministic), passes."""
    tab = fv.SurfelTable(S)
    idx = np.nonzero(on)[0]
    Ed = fv.direct_bake(scene, subset(lights, idx), S['p'], S['n']) if len(idx) else np.zeros_like(S['p'])
    J = bounce_hits(scene, S, tab, seed)
    E, passes = settle(S['alb'], Ed, J, 'settle' if red == 'settle' else '')
    return E, passes


# ---------------------------------------------------------------- the shared record's pairs (GPU relight kernel 1)
def light_pairs(scene, lights, S, red=''):
    """Per light, once per bake: the surfels in reach that face it, and whether the shadow segment is free.
    Geometry only: nothing about color, dimmer or on/off is kept (those are read live from the record).
    Written separately from farview1's direct_bake (slab test per segment, not a box loop)."""
    pairs = []
    o_all = S['p'] + S['n'] * fv.SURF_OFF
    for i in range(len(lights['r'])):
        q = lights['pos'][i]
        d = np.linalg.norm(q - S['p'], axis=1)
        idx = np.nonzero(d < lights['r'][i])[0]
        ndl = np.einsum('ij,ij->i', S['n'][idx], q - S['p'][idx]) / np.maximum(d[idx], 1e-3)
        idx = idx[ndl > 0]
        if red == 'nopairvis':
            pairs.append(idx)
            continue
        bl = fv.blocked_slab(scene, o_all[idx], np.repeat(q[None], len(idx), 0)) if len(idx) else np.zeros(0, bool)
        pairs.append(idx[~bl])
    return pairs


def direct_from_pairs(lights, S, pairs, on):
    """GPURELIGHT1 kernel 1 from the record: E_s = sum over visible pairs of color x curve x max(N.L, 0)."""
    E = np.zeros((len(S['p']), 3))
    for i in np.nonzero(on)[0]:
        idx = pairs[i]
        if len(idx) == 0:
            continue
        v = lights['pos'][i] - S['p'][idx]
        d = np.linalg.norm(v, axis=1)
        ndl = np.einsum('ij,ij->i', S['n'][idx], v) / np.maximum(d, 1e-3)
        a = fv.curve(np.minimum(d / lights['r'][i], 1.0), lights['bse'][i]) * np.maximum(ndl, 0)
        np.add.at(E, idx, a[:, None] * lights['c'][i])
    return E


# ---------------------------------------------------------------- the world
def build():
    t0 = time.time()
    scene = fv.make_street()
    lights = fv.make_lights(scene, N_LIGHTS, seed=1500)   # the same lights as farview1's 500-light world
    S = fv.make_surfels(scene)
    graph = make_switch_graph(scene, lights)
    keys = [group_key(graph, i) for i in range(N_LIGHTS)]
    gkeys = sorted(set(keys))
    gid = np.array([gkeys.index(k) for k in keys])
    tab = fv.SurfelTable(S)
    J = bounce_hits(scene, S, tab, seed=202)
    print('world: %d boxes, %d surfels, %d lights, %d switch groups + always-on (%d parent, %d self); '
          '%d chained references; bounce rays traced once: %d (%.1f s)'
          % (len(scene['lo']), len(S['p']), N_LIGHTS, len(gkeys) - 1,
             sum(k[0] == KIND_PARENT for k in gkeys), sum(k[0] == KIND_SELF for k in gkeys),
             sum(1 for r in graph['parent'] if r >= 300000), J.size, time.time() - t0))
    return dict(scene=scene, lights=lights, S=S, graph=graph, keys=keys, gkeys=gkeys, gid=gid, tab=tab, J=J)


def bake_layers(W, red=''):
    """One layer per group (group 0 = always on): direct from the group's lights only, then the passes."""
    t0 = time.time()
    L = []
    for g in range(len(W['gkeys'])):
        idx = np.nonzero(W['gid'] == g)[0]
        Ed = fv.direct_bake(W['scene'], subset(W['lights'], idx), W['S']['p'], W['S']['n'])
        if red == 'nobounce' and W['gkeys'][g][0] != KIND_ALWAYS:
            L.append(Ed)
        else:
            L.append(settle(W['S']['alb'], Ed, W['J'], 'settle' if red == 'settle' else '')[0])
    return L, time.time() - t0


def states(W):
    """Five switch states: all on, all switched groups off, two random, every root marker disabled."""
    g = W['graph']
    out = []
    rng = np.random.default_rng(77)
    allr = {r: True for r in g['roots']}
    alls = {r: True for r in g['scripted']}
    out.append(('all on', allr, alls))
    # 'switched off': every root in the state that turns its parity-0 children off, scripted off
    out.append(('roots off', {r: False for r in g['roots']}, {r: False for r in g['scripted']}))
    for k in range(2):
        out.append(('random %d' % (k + 1), {r: bool(rng.random() < 0.5) for r in g['roots']},
                    {r: bool(rng.random() < 0.5) for r in g['scripted']}))
    out.append(('half', {r: (i % 2 == 0) for i, r in enumerate(g['roots'])},
                {r: (i % 2 == 1) for i, r in enumerate(sorted(g['scripted']))}))
    return out


def trim_of(W, g, base=TRIM_BASE, layer=TRIM_LAYER):
    return base if W['gkeys'][g][0] == KIND_ALWAYS else layer


def stored_sum(W, L, on_groups, scale, base=TRIM_BASE, layer=TRIM_LAYER):
    """What the files hold: each layer trimmed at its trim x scale, RGB9E5, summed in float32."""
    tot = np.zeros((len(W['S']['p']), 3), np.float32)
    for g in on_groups:
        keep = fv.lum(L[g]) > trim_of(W, g, base, layer) * scale
        q = np.zeros_like(L[g])
        q[keep] = rgb9e5_decode(rgb9e5_encode(L[g][keep]))
        tot += q.astype(np.float32)
    return tot.astype(np.float64)


# ---------------------------------------------------------------- the checks
def run(W, red, cache):
    res, out = {}, []

    def row(name, ok, text):
        res[name] = bool(ok)
        out.append('%-3s %-4s %s' % (name, 'PASS' if ok else 'FAIL', text))

    g = W['graph']
    # G -- the group key against the chain walk
    keys = [group_key(g, i, red) for i in range(N_LIGHTS)]
    rng = np.random.default_rng(5)
    bad, tested = 0, 0
    for k in range(6):
        ro = {r: bool(rng.random() < 0.5) for r in g['roots']}
        so = {r: bool(rng.random() < 0.5) for r in g['scripted']}
        for i in range(N_LIGHTS):
            tested += 1
            bad += group_on(keys[i], ro, so) != shown_by_chain(g, i, ro, so)
    nchain = sum(1 for i in range(N_LIGHTS) if i in g['parent'] and g['parent'][i][0] >= 300000)
    row('G', bad == 0, 'group key (root, parity) vs walking the chain: %d of %d light-states differ (bar 0); '
        '%d lights hang under a chain of two, %d with net opposite parity'
        % (bad, tested, nchain, sum(1 for k in keys if k[0] == KIND_PARENT and k[2] == 1)))

    # layers (cached per red flavour that changes them)
    lk = red if red in ('settle', 'nobounce') else ''
    if lk not in cache:
        cache[lk] = bake_layers(W, lk)
    L, t_layers = cache[lk]
    scale = cache['scale']
    pairs = cache['pairs_red' if red == 'nopairvis' else 'pairs']

    worstL, worstQ, worstX, lines = 0.0, [], 0.0, []
    okL = okQ = okX = True
    for si, (name, ro, so) in enumerate(states(W)):
        on_g = [k for k, key in enumerate(W['gkeys']) if group_on(key, ro, so)]
        on_light = np.array([group_on(W['keys'][i], ro, so) for i in range(N_LIGHTS)])
        rk = (name, 'settle' if red == 'settle' else '')
        if rk not in cache:
            t0 = time.time()
            cache[rk] = rebake(W['scene'], W['lights'], on_light, W['S'], seed=202, red=rk[1]) + (time.time() - t0,)
        R, rpasses, tr = cache[rk]
        use = list(on_g)
        dropped = None
        if red == 'drop':
            sw = [k for k in on_g if W['gkeys'][k][0] != KIND_ALWAYS]
            if sw:
                dropped = min(sw, key=lambda k: fv.lum(L[k]).sum())
                use.remove(dropped)
        Ssum = sum(L[k] for k in use)
        dev = float(np.max(np.abs(Ssum - R)) / max(np.max(np.abs(R)), 1e-30))
        worstL = max(worstL, dev)
        okL &= dev <= BAR_L
        # Q -- the stored form
        St = stored_sum(W, L, use, scale)
        p99, en = q_metric(St, R)
        okQ &= p99 <= BAR_Q_P99 and abs(en - 1) <= BAR_Q_ENERGY
        worstQ.append((p99, en))
        # X -- the GPU relight's direct kernel from the shared record
        key = ('direct', name)
        if key not in cache:
            cache[key] = fv.direct_bake(W['scene'], subset(W['lights'], np.nonzero(on_light)[0]), W['S']['p'], W['S']['n'])
        Ed = cache[key]
        Ex = direct_from_pairs(W['lights'], W['S'], pairs, on_light)
        dx = float(np.max(np.abs(Ex - Ed)) / max(np.max(np.abs(Ed)), 1e-30))
        worstX = max(worstX, dx)
        okX &= dx <= BAR_X
        lines.append('     %-9s %3d of %3d lights on, %2d of %2d groups; rebake %d passes (%.1f s), bounce share %.1f%%; '
                     'L %.1e  Q p99 %.4f energy %.4f  X %.1e%s'
                     % (name, on_light.sum(), N_LIGHTS, len(on_g), len(L), rpasses, tr,
                        100 * (1 - fv.lum(cache[key]).sum() / max(fv.lum(R).sum(), 1e-30)), dev, p99, en, dx,
                        ('  (dropped group %d: %s, %d lights)' % (dropped, KIND_NAME[W['gkeys'][dropped][0]],
                                                                   (W['gid'] == dropped).sum()) if dropped is not None else '')))
    row('L', okL, 'always-on + the layers that are on vs a full rebake, 5 states: worst max|diff|/max %.1e (bar %.0e)'
        % (worstL, BAR_L))
    row('Q', okQ, 'stored (trim %.0e / %.0e x p99, RGB9E5, float32 sum) vs rebake on lit surfels: worst p99 %.4f '
        '(bar %.2f), energy %.4f..%.4f (bar 1 +- %.3f)' % (TRIM_BASE, TRIM_LAYER, max(q[0] for q in worstQ), BAR_Q_P99, min(q[1] for q in worstQ),
                                                max(q[1] for q in worstQ), BAR_Q_ENERGY))
    row('X', okX, 'GPU-relight direct from the shared record (params + %d visible pairs, built once) vs the bake\'s '
        'direct, 5 states: worst %.1e (bar %.0e)' % (sum(len(p) for p in pairs), worstX, BAR_X))
    out.extend(lines)
    return res, out


def q_metric(St, R):
    floor = Q_FLOOR * np.percentile(fv.lum(R), 95)
    lit = fv.lum(R) > floor
    rel = np.abs(fv.lum(St) - fv.lum(R))[lit] / fv.lum(R)[lit]
    return float(np.percentile(rel, 99)), float(fv.lum(St)[lit].sum() / fv.lum(R)[lit].sum())


TARGET = {'drop': ['L'], 'settle': ['L'], 'nobounce': ['L'], 'noparity': ['G'], 'nopairvis': ['X']}


def cost_report(W, L, scale):
    """Per group: lights, entries kept, bytes, sectors, how far the layer reaches past its lights' radii."""
    S = W['S']
    sec = np.floor(S['p'][:, :2] / SECTOR).astype(int)
    seckey = sec[:, 0] * 1000 + sec[:, 1]
    rows = []
    union = np.zeros(len(S['p']), bool)
    total_entries = 0
    for g, key in enumerate(W['gkeys']):
        keep = fv.lum(L[g]) > trim_of(W, g) * scale
        idx = np.nonzero(W['gid'] == g)[0]
        lp, lr = W['lights']['pos'][idx], W['lights']['r'][idx]
        dmin = np.min(np.linalg.norm(S['p'][:, None] - lp[None], axis=2) / lr[None], 1) if len(idx) <= 60 else None
        reach = int((keep & (dmin > 1.0)).sum()) if dmin is not None else -1
        nsec = len(np.unique(seckey[keep]))
        if key[0] == KIND_ALWAYS:
            base_n = int(keep.sum())
        else:
            total_entries += int(keep.sum())
            rows.append((KIND_NAME[key[0]], key[2], len(idx), int(keep.sum()), reach, nsec))
        union |= keep
    return rows, base_n, int(union.sum()), total_entries


def main():
    t_start = time.time()
    W = build()
    cache = {}
    t0 = time.time()
    # the all-on light sets the trim scale (the .fvl v1 rule); pairs once
    cache['pairs'] = light_pairs(W['scene'], W['lights'], W['S'])
    cache['pairs_red'] = light_pairs(W['scene'], W['lights'], W['S'], 'nopairvis')
    cache[''] = bake_layers(W, '')
    L = cache[''][0]
    Eall = sum(L)
    cache['scale'] = float(np.percentile(fv.lum(Eall), 99))
    print('layers: %d (always-on + %d groups) baked in %.1f s; shared-record pairs %d (%.1f a light) built in %.1f s'
          % (len(L), len(L) - 1, cache[''][1], sum(len(p) for p in cache['pairs']),
             sum(len(p) for p in cache['pairs']) / N_LIGHTS, time.time() - t0 - cache[''][1]))

    # cost per group (measured, not gated)
    rows, base_n, union_n, tot = cost_report(W, L, cache['scale'])
    print('\ncost per switch group (entries = surfels the layer keeps after the trim; 8 B an entry in the .fvg):')
    print('  %-6s %-6s %6s %8s %8s %8s %8s %12s' % ('kind', 'parity', 'lights', 'entries', 'per lgt', 'bytes', 'sectors',
                                                  'past radius'))
    for kind, par, nl, ne, reach, nsec in sorted(rows, key=lambda r: -r[3]):
        print('  %-6s %-6d %6d %8d %8.0f %8d %8d %11.0f%%' % (kind, par, nl, ne, ne / max(nl, 1), ne * 8, nsec,
                                                             100.0 * reach / max(ne, 1)))
    ne = np.array([r[3] for r in rows]); nl = np.array([r[2] for r in rows])
    print('  groups %d: entries median %d, max %d; %.0f entries a switched light; all group layers %d entries = %.0f KB'
          % (len(rows), np.median(ne), ne.max(), ne.sum() / nl.sum(), tot, tot * 8 / 1024))
    print('  always-on layer: %d lit surfels; .fvl v2 records (union of every layer) %d = %.0f KB (v1 all-on file: %d = %.0f KB)'
          % (base_n, union_n, union_n * 16 / 1024, int((fv.lum(Eall) > TRIM_V1 * cache['scale']).sum()),
             int((fv.lum(Eall) > TRIM_V1 * cache['scale']).sum()) * 16 / 1024))
    # the trim trade-off (measured, all-on state, where layers == rebake to 1e-15): what chose the two trims
    print('  trim trade-off (all-on state; Q metric; entries in the switched layers):')
    for tb, tl in ((1e-3, 1e-3), (1e-4, 1e-3), (1e-4, 1e-4), (1e-4, TRIM_LAYER), (1e-4, 1e-5)):
        n = sum(int((fv.lum(L[g]) > tl * cache['scale']).sum()) for g in range(len(L)) if W['gkeys'][g][0] != KIND_ALWAYS)
        p99, en = q_metric(stored_sum(W, L, range(len(L)), cache['scale'], tb, tl), Eall)
        print('    always-on %.0e, switched %.0e: %6d entries (%4.0f KB), p99 %.4f, energy %.4f%s'
              % (tb, tl, n, n * 8 / 1024, p99, en, '   <- chosen' if (tb, tl) == (TRIM_BASE, TRIM_LAYER) else ''))
    print('  for contrast, a dense per-light basis (GPURELIGHT1 3.2): %d surfels x %d lights x 12 B = %.0f MB'
          % (len(W['S']['p']), N_LIGHTS, len(W['S']['p']) * N_LIGHTS * 12 / 2 ** 20))
    # a switch: re-sum one sector's current light (float32), measured
    on_layers = [rgb9e5_decode(rgb9e5_encode(Lg)).astype(np.float32) for Lg in L]
    t1, reps = time.perf_counter(), 0
    while time.perf_counter() - t1 < 0.3:
        acc = on_layers[0].copy()
        for Lg in on_layers[1:]:
            acc += Lg
        reps += 1
    print('  a switch re-sums the touched sectors: dense numpy re-sum of all %d layers over %d surfels %.2f ms '
          '(CPU twin, not a GPU number)' % (len(L), len(W['S']['p']), (time.perf_counter() - t1) / reps * 1e3))

    only = os.environ.get('FARVIEW1B_RED', '')
    if only:
        if only not in TARGET:
            raise SystemExit('unknown FARVIEW1B_RED %r (one of %s)' % (only, ', '.join(TARGET)))
        res, out = run(W, only, cache)
        print('\nred %s:' % only)
        print('\n'.join('  ' + o for o in out))
        failed = [c for c in TARGET[only] if not res[c]]
        print('red %s: %s (target checks failing: %s)' % (only, 'FAILS as it must' if failed else 'DID NOT FAIL',
                                                          ', '.join(failed) or 'none'))
        sys.exit(1 if failed else 0)

    print('\ngreen:')
    res, out = run(W, '', cache)
    print('\n'.join('  ' + o for o in out))
    green_ok = all(res.values())
    reds_ok = True
    for red, targets in TARGET.items():
        r, o = run(W, red, cache)
        failed = [c for c in targets if not r[c]]
        print('\nred %s (must FAIL %s):' % (red, ', '.join(targets)))
        print('\n'.join('  ' + x for x in o))
        print('  -> %s' % ('FAILS as it must (%s)' % ', '.join(failed) if len(failed) == len(targets)
                           else 'DID NOT FAIL every target (failing: %s): the gate is blind' % (', '.join(failed) or 'none')))
        reds_ok &= len(failed) == len(targets)
    verdict = green_ok and reds_ok
    print('\nFARVIEW1b layers %s: green %s, reds %s; %.0f s, peak memory %.0f MB'
          % ('PASS' if verdict else 'FAIL', 'PASS' if green_ok else 'FAIL', 'all fail' if reds_ok else 'NOT all fail',
             time.time() - t_start, peak_mb()))
    sys.exit(0 if verdict else 1)


if __name__ == '__main__':
    main()
