"""TILING4 step 0 -- the POOL of usable dim-4 chunks, and the frozen
selection / validation split.

WHY A NEW POOL RULE AT ALL.  The brief asks for "a disjoint 7 of TILING2's 22".
TILING2's 22 were picked off the SHIPPED SHEETS alone (mip-3 luminance SD >=
5.25 separates the land population from the ocean/void filler).  That rule says
nothing about whether the cells under a sheet carry LAND PAINT, and the land
sample this lane changes only ever runs on painted ground: TILING3 measured that
59 of 180 census chunks are layerless, and under the shipped default a layerless
chunk copies vanilla's colour byte for byte and never calls the sampler at all.
SPLAT1's offline model reproduces exactly that -- an unpainted chunk comes back
as one grey value (127.5) -- which is why TILING3 could only use 7 of the 22.

So the pool is TILING2's population rule AND "the sampler actually runs here":

  P1  a shipped `Commonwealth.4.<cx>.<cy>.DDS` exists;
  P2  TILING2's population rule: mip-3 luminance SD >= 5.25 (the trough
      midpoint TILING2 read off the histogram of all 2,304 sheets);
  P3  EVERY one of the chunk's 16 cells has a LAND record carrying at least
      one quadrant base texture -- the chunk is fully painted, so no part of
      the offline sheet is the 127.5 stand-in;
  P4  the offline RUNG bake's luminance SD >= 1.0 (SPLAT1/TILING3's
      `refuse_flat` floor: below this it is not a sheet, it is a refusal);
  P5  at most 2 % of the offline rung bake is the flat 127.5 stand-in
      (added before any candidate was scored -- see `p4_ok`).

THE SPLIT, written before any candidate of this lane was scored:

  * SELECTION = TILING3's seven, unchanged and in its order.  Frozen by that
    lane; re-picking it here would let this lane choose the ground it is
    graded on.
  * VALIDATION = seven more from the pool, disjoint from the selection, by
    TILING2's own lattice rule: a deterministic lattice over the bounding box
    of the pool, each lattice point snapped to the nearest pool chunk by chunk
    distance (ties by (cx,cy)), duplicates and selection members skipped by
    taking the next-nearest.  Nothing about the candidates is looked at.

Writes `pool.json` (the pool, the split) and `logs/p0_pool.txt`.

    python p0_pool.py
"""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
T2 = os.path.join(os.path.dirname(HERE), 'tiling2_20260911')
T3 = os.path.join(os.path.dirname(HERE), 'tiling3_20260911')
SP = os.path.join(os.path.dirname(HERE), 'splat1_20260911')
for p in (HERE, T3, T2, SP):
    sys.path.insert(0, p)
import splatlib as S                                          # noqa: E402
import offline_bake as OB                                     # noqa: E402

FLOOR_SD = 5.25          # TILING2's population rule
FLAT_SD = 1.0            # TILING3's refuse_flat
STANDIN_MAX = 0.02       # P5, see p4_ok: the validation set's flat-fill bound
SELECTION = [(-20, 24), (-20, 20), (-36, -20), (-4, -20), (28, -20), (-4, 16), (24, 16)]
NVAL = 7
NX, NY = 4, 2            # 8 lattice points for 7 wanted, so one may be skipped


def all_coords():
    out = []
    for name in os.listdir(S.VAN):
        if not name.startswith('Commonwealth.4.') or not name.endswith('.DDS'):
            continue
        stem = name[len('Commonwealth.4.'):-len('.DDS')]
        if '_' in stem:
            continue
        a, b = stem.split('.')
        out.append((int(a), int(b)))
    return sorted(out)


def coarse_sd(cx, cy):
    try:
        d = S.Dds(S.van_sheet(cx, cy))
    except Exception:
        return None
    m = min(3, d.maxMip)
    return float(S.lum(d.level(m)).std())


def fully_painted(e, cx0, cy0, dim=4):
    for ci in range(dim * dim):
        cx, cy = cx0 + ci % dim, cy0 + ci // dim
        land = e.lands.get((cx, cy))
        if not land:
            return False
        if not any(land['base'][q] for q in range(4)):
            return False
    return True


def main():
    L = ['TILING4 step 0 -- the pool and the frozen split', '']
    coords = all_coords()
    L.append('shipped dim-4 colour sheets: %d' % len(coords))

    # P2
    p2 = []
    for cx, cy in coords:
        sd = coarse_sd(cx, cy)
        if sd is not None and sd >= FLOOR_SD:
            p2.append((cx, cy))
    L.append('P2 mip-3 SD >= %.2f (TILING2 population rule): %d' % (FLOOR_SD, len(p2)))

    # P3 -- needs the ESM walk
    e = OB.esm()
    L.append('ESM LAND records walked: %d cells' % len(e.lands))
    p3 = [(cx, cy) for cx, cy in p2 if fully_painted(e, cx, cy)]
    L.append('P3 all 16 cells have a LAND record with a quadrant base texture: %d' % len(p3))

    # P4 -- the offline rung bake must not be flat.  Evaluated LAZILY, only on
    # the chunks the lattice actually reaches, because a bake is seconds and P3
    # is hundreds of chunks.  A lattice point whose nearest chunk is refused
    # takes the next-nearest, and every refusal is printed by name.
    refused = []
    sd_of = {}
    si_of = {}

    def p4_ok(c):
        """P4 and P5, both on the same offline rung bake.

        P5 was added 00:1x, BEFORE any candidate of this lane was scored, after
        `p0b_coverage.py` measured that P3 does not bound the flat-fill
        fraction: a quadrant whose base texture is absent falls back to the
        chunk's dominant base, and where that is absent too the model leaves
        the 127.5 stand-in.  The first lattice run picked (4,8), which is 23 %
        stand-in.  A flat patch that size is a term in every statistic this
        lane grades on, so the VALIDATION seven -- the set this lane chooses --
        is bounded at 2 %.  TILING3's SELECTION seven is frozen and is NOT
        re-picked; its stand-in fractions are printed beside every number
        instead (1.1 % on (-20,24), 8.5 % on (-4,16), 0.0 % on the other five).
        """
        if c in sd_of:
            return sd_of[c] >= FLAT_SD and si_of[c] <= STANDIN_MAX
        try:
            a = OB.bake(c[0], c[1], dim=4, tile=341.3333, mip='code')
        except Exception as exc:
            sd_of[c] = -1.0
            si_of[c] = 1.0
            refused.append((c, 'bake raised %s' % exc))
            return False
        sd_of[c] = float(S.lum(a).std())
        si_of[c] = float(np.all(np.abs(a - 127.5) < 1e-9, axis=2).mean())
        if sd_of[c] < FLAT_SD:
            refused.append((c, 'P4 offline rung bake SD %.3f < %.2f' % (sd_of[c], FLAT_SD)))
            return False
        if si_of[c] > STANDIN_MAX:
            refused.append((c, 'P5 flat 127.5 stand-in %.1f%% > %.1f%%'
                            % (100 * si_of[c], 100 * STANDIN_MAX)))
            return False
        return True

    # the lattice
    cand = [c for c in p3 if c not in SELECTION]
    if not cand:
        raise RuntimeError('REFUSED: no chunk outside the selection survives P1-P3')
    xs = [c[0] for c in cand]
    ys = [c[1] for c in cand]
    bb = (min(xs), max(xs), min(ys), max(ys))
    L.append('P1-P3 outside the selection: %d chunks, bbox cx %d..%d cy %d..%d'
             % (len(cand), bb[0], bb[1], bb[2], bb[3]))
    L.append('P4 is applied lazily, per lattice pick, and every refusal is named.')
    val = []
    for gy in range(NY):
        for gx in range(NX):
            if len(val) >= NVAL:
                break
            tx = bb[0] + (bb[1] - bb[0]) * (gx + 0.5) / NX
            ty = bb[2] + (bb[3] - bb[2]) * (gy + 0.5) / NY
            order = sorted(cand, key=lambda c: ((c[0] - tx) ** 2 + (c[1] - ty) ** 2, c[0], c[1]))
            for c in order:
                if c in val:
                    continue
                if not p4_ok(c):
                    continue
                val.append(c)
                L.append('   lattice (%7.1f,%7.1f) -> (%d,%d)  offline SD %.2f  stand-in %.1f%%'
                         % (tx, ty, c[0], c[1], sd_of[c], 100 * si_of[c]))
                break
    for (c, why) in refused:
        L.append('   REFUSED (%d,%d): %s' % (c[0], c[1], why))
    pool = [dict(cx=c[0], cy=c[1], sd=sd_of[c], standin=si_of[c]) for c in sorted(sd_of)
            if sd_of[c] >= FLAT_SD and si_of[c] <= STANDIN_MAX]
    val = val[:NVAL]
    L.append('')
    L.append('SELECTION  (TILING3`s, frozen): %s' % ' '.join('(%d,%d)' % c for c in SELECTION))
    L.append('VALIDATION (this lane`s, frozen here): %s' % ' '.join('(%d,%d)' % c for c in val))
    assert not (set(val) & set(SELECTION)), 'the two sets must be disjoint'
    L.append('disjoint: yes (%d shared)' % len(set(val) & set(SELECTION)))

    txt = '\n'.join(L) + '\n'
    print(txt)
    os.makedirs(os.path.join(HERE, 'logs'), exist_ok=True)
    with open(os.path.join(HERE, 'logs', 'p0_pool.txt'), 'w', newline='\n') as f:
        f.write(txt)
    json.dump(dict(pool=pool, selection=[list(c) for c in SELECTION],
                   validation=[list(c) for c in val],
                   refused=[[list(c), w] for c, w in refused],
                   floorSd=FLOOR_SD, flatSd=FLAT_SD, standinMax=STANDIN_MAX,
                   bbox=list(bb)),
              open(os.path.join(HERE, 'pool.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
