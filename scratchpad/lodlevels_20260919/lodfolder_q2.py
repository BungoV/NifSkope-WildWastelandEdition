#!/usr/bin/env python3
"""LODFOLDER Q2 (read-only): are the four MNAM slots different meshes, and are
the later ones actually simpler?  Triangle counts read from the real files.
Split by category (trees/landscape, architecture, vehicles, other)."""
import os
import pickle
import statistics
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TMP = os.environ.get('LODFOLDER_TMP') or HERE
ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
sys.path.insert(0, ROOT + '/tests/spells')
import gltf_nifread as G                    # noqa: E402

MESHROOT = 'E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/'
BS = chr(92)

_tri = {}
STAT = dict(ok=0, missing=0, badparse=0)


def norm(p):
    return p.replace(BS, '/').lower().strip().lstrip('/')


def tris(path):
    n = norm(path)
    if n in _tri:
        return _tri[n]
    full = MESHROOT + n
    v = None
    if not os.path.exists(full):
        STAT['missing'] += 1
    else:
        try:
            nif = G.Nif(full)
            v = sum(int(sh.get('numTris') or 0) for sh in nif.shapes.values())
            STAT['ok'] += 1
        except Exception:
            STAT['badparse'] += 1
            v = None
    _tri[n] = v
    return v


def category(b):
    """classify from the model paths (near MODL + every filled LOD slot)."""
    p = norm(b['modl'] or '') + ' ' + ' '.join(norm(s) for s in b['slots'] if s)
    if ('landscape/' in p or 'trees/' in p or 'setdressing/foliage' in p
            or 'lod/landscape' in p or 'lod/trees' in p):
        return 'trees/landscape'
    if 'vehicles/' in p:
        return 'vehicles'
    if 'architecture/' in p or 'buildings/' in p or 'kit/' in p or 'kit\\' in p:
        return 'architecture/buildings'
    return 'other'


CATS = ['trees/landscape', 'architecture/buildings', 'vehicles', 'other']


def row(plug, form, edid, slots):
    cells = []
    for s in slots:
        if not s:
            cells.append('-')
            continue
        t = tris(s)
        cells.append('%s<br>(%s tris)' % (os.path.basename(s.replace(BS, '/')),
                                          '%d' % t if t is not None else 'not unpacked'))
    return '| %s | %08X | %s | %s |' % (edid or '(none)', form, plug, ' | '.join(cells))


def ratioTable(W, groups):
    W('| set | ratio | n | median | mean | p10 | p90 | exactly 1.000 | < 0.99 |')
    W('|---|---|---|---|---|---|---|---|---|')
    for label, recs in groups:
        rs = {1: [], 2: [], 3: []}
        for plug, form, edid, typ, slots in recs:
            if not slots[0]:
                continue
            t0 = tris(slots[0])
            if not t0:
                continue
            for k in (1, 2, 3):
                if not slots[k]:
                    continue
                tk = tris(slots[k])
                if tk is None:
                    continue
                rs[k].append(tk / float(t0))
        for k in (1, 2, 3):
            r = sorted(rs[k])
            if not r:
                W('| %s | slot%d/slot0 | 0 | - | - | - | - | - | - |' % (label, k))
                continue
            eq = sum(1 for x in r if abs(x - 1.0) < 1e-9)
            lt = sum(1 for x in r if x < 0.99)
            W('| %s | slot%d/slot0 | %d | %.3f | %.3f | %.3f | %.3f | %d (%.0f%%) | %d (%.0f%%) |'
              % (label, k, len(r), statistics.median(r), sum(r) / len(r),
                 r[int(0.10 * (len(r) - 1))], r[int(0.90 * (len(r) - 1))],
                 eq, 100.0 * eq / len(r), lt, 100.0 * lt / len(r)))


def main():
    allbases = pickle.load(open(os.path.join(TMP, 'lodfolder_bases.pkl'), 'rb'))

    single, repeat, multi = [], [], []
    cat = {}
    for (plug, form), b in allbases.items():
        filled = [(k, s) for k, s in enumerate(b['slots']) if s]
        if not filled:
            continue
        uniq = set(norm(s) for _, s in filled)
        rec = (plug, form, b['edid'], b['type'], list(b['slots']))
        c = category(b)
        cat[(plug, form)] = c
        if len(filled) == 1:
            single.append(rec)
            kind = 'single'
        elif len(uniq) == 1:
            repeat.append(rec)
            kind = 'repeat'
        else:
            multi.append(rec)
            kind = 'multi'
        cat.setdefault('_tally', {})
        cat['_tally'][(c, kind)] = cat['_tally'].get((c, kind), 0) + 1

    tally = cat['_tally']
    out = []
    W = out.append
    W('### Q2 -- do the four MNAM slots hold DIFFERENT meshes?')
    W('')
    W('| pattern | bases |')
    W('|---|---|')
    W('| only ONE filled slot | **%d** |' % len(single))
    W('| >=2 filled slots, the SAME mesh repeated in every filled slot | **%d** |' % len(repeat))
    W('| >=2 filled slots, at least two DIFFERENT meshes | **%d** |' % len(multi))
    W('| total LOD-bearing bases | %d |' % (len(single) + len(repeat) + len(multi)))
    W('')
    W('#### by category (classified from the model path)')
    W('')
    W('| category | multi-mesh across slots | same mesh repeated | single filled slot | total |')
    W('|---|---|---|---|---|')
    for c in CATS:
        m = tally.get((c, 'multi'), 0)
        r = tally.get((c, 'repeat'), 0)
        s = tally.get((c, 'single'), 0)
        W('| %s | **%d** | %d | %d | %d |' % (c, m, r, s, m + r + s))
    W('| **all** | **%d** | %d | %d | %d |'
      % (len(multi), len(repeat), len(single), len(multi) + len(repeat) + len(single)))
    W('')

    # ------------------------------------------------ named examples
    def pick(c, n):
        got = []
        for rec in multi:
            if len(got) >= n:
                break
            if cat.get((rec[0], rec[1])) != c:
                continue
            if tris(rec[4][0]) is None:       # need real triangle counts
                continue
            got.append(rec)
        return got

    multi.sort(key=lambda r: (-len([s for s in r[4] if s]), r[2]))
    hdr = ('| base editor ID | formID | plugin | slot 0 | slot 1 | slot 2 | slot 3 |',
           '|---|---|---|---|---|---|---|')

    W('#### five TREE / landscape multi-level examples (triangles read from the .nif)')
    W('')
    W(hdr[0]); W(hdr[1])
    for plug, form, edid, typ, slots in pick('trees/landscape', 5):
        W(row(plug, form, edid, slots))
    W('')
    W('#### five ARCHITECTURE / building multi-level examples')
    W('')
    W(hdr[0]); W(hdr[1])
    for plug, form, edid, typ, slots in pick('architecture/buildings', 5):
        W(row(plug, form, edid, slots))
    W('')
    W('#### ten mixed multi-level examples (most filled slots first)')
    W('')
    W(hdr[0]); W(hdr[1])
    for plug, form, edid, typ, slots in multi[:10]:
        W(row(plug, form, edid, slots))
    W('')

    # ------------------------------------------------ ratio tables
    W('#### triangle ratio distribution slot N : slot 0')
    W('')
    W('The set that answers the question is the FIRST block -- the %d bases whose slots'
      % len(multi))
    W('actually name different meshes.  The last block adds the %d repeat bases to show'
      % len(repeat))
    W('how much of the whole population is just the same file in every slot.')
    W('')
    treesM = [r for r in multi if cat.get((r[0], r[1])) == 'trees/landscape']
    archM = [r for r in multi if cat.get((r[0], r[1])) == 'architecture/buildings']
    othM = [r for r in multi if cat.get((r[0], r[1])) not in
            ('trees/landscape', 'architecture/buildings')]
    ratioTable(W, [('MULTI-mesh, all', multi),
                   ('MULTI, trees/landscape', treesM),
                   ('MULTI, architecture', archM),
                   ('MULTI, vehicles+other', othM),
                   ('MULTI + REPEAT (whole population)', multi + repeat)])
    W('')

    base0 = [tris(r[4][0]) for r in multi if r[4][0] and tris(r[4][0])]
    if base0:
        W('slot-0 triangle counts over the multi-mesh bases: n=%d  median %d  mean %d  max %d'
          % (len(base0), int(statistics.median(base0)),
             int(sum(base0) / len(base0)), max(base0)))
        W('')

    # ------------------------------------------------ near vs slot0
    W('#### near model (MODL) vs its own LOD slot 0')
    W('')
    nearr = []
    identical = 0
    for (plug, form), b in allbases.items():
        s0 = b['slots'][0]
        if not s0 or not b['modl']:
            continue
        if norm(b['modl']) == norm(s0):
            identical += 1
            continue
        tn = tris(b['modl'])
        t0 = tris(s0)
        if not tn or t0 is None:
            continue
        nearr.append(t0 / float(tn))
    if nearr:
        r = sorted(nearr)
        W('n=%d  median slot0/near = **%.3f**  mean %.3f  p10 %.3f  p90 %.3f'
          % (len(r), statistics.median(r), sum(r) / len(r),
             r[int(0.10 * (len(r) - 1))], r[int(0.90 * (len(r) - 1))]))
        W('')
        W('So the very first LOD slot is already ~%.0fx simpler than the near model: the drop'
          % (1.0 / statistics.median(r)))
        W('from full geometry to LOD happens in ONE step, not through an intermediate.')
        W('')
        W('(%d further bases have slot 0 == their own MODL path, ratio 1.000 by definition.)'
          % identical)
    W('')
    W('NIF read tally: %d parsed, %d named a file not present in the unpacked corpus, '
      '%d failed to parse (counted, not silently dropped).'
      % (STAT['ok'], STAT['missing'], STAT['badparse']))
    W('')
    W('CAVEAT on the missing %d: E:%sTools%sFallout 4%sDataUnpacked%sData holds the BASE GAME'
      % (STAT['missing'], BS, BS, BS, BS))
    W('BA2s only -- there is no Meshes%sDLC03 or Meshes%sDLC04 there.  Every DLC LOD mesh'
      % (BS, BS))
    W('therefore reads as "not unpacked"; the DLC records are still counted in every')
    W('slot-pattern and category table above, only their triangle counts are absent.')
    W('')

    txt = '\n'.join(out)
    open(os.path.join(TMP, 'q2.md'), 'w').write(txt)
    print(txt)


if __name__ == '__main__':
    main()
