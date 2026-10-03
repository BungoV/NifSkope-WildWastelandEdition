#!/usr/bin/env python3
"""THE LIT EFFECT CARDS: an independent check (lane FXLIT1, 2026-10-02; stage L of cell_fx.sh).

The game lights an effect shape whose material sets the effect-lighting flag: its linear color is multiplied by
    mix( 1, directional color + sum over the placed model's (up to) four lights of
            color x fade x pow( 1 - saturate( d / radius )^2, 2.2 ) x the spot cone, lighting influence ).
The viewer writes, for those shapes alone and opaque, the placed model (probe 70), the world position (71 / 72)
and that multiplier / 4 in 16 bits (73 / 74). This script computes the multiplier ITSELF, from the plugin and the
models on disk, with none of NifSkope's code: its own walk of the cell's references, its own reading of each
light record and of the placement's fade offset, each model's stored bounds and material, and the game's rule
for which four lights a model takes (written again here from the notes, not from the viewer's source).
A material swap (the reference's XMSP, else its base's MODS: an MSWP of original -> replacement material rows)
replaces the material a shape names before its lighting flag and influence are read: the Vault's dusty mist
placements swap a 0.95 material for a 1.0 one, and a checker without the swap disagrees on every one of them.

Bars, per view: the share of pixels whose multiplier agrees (the tolerance holds the spread of the expectation
over +-0.5 unit of position per axis), and the viewer's total over the expected total.

USAGE  cell_fxlit_check.py <esm> <data> <cell> <run dir> [rule|strong] [agree floor] [total band]
       cell_fxlit_check.py --unlit <run dir>    # stage U: before.png, on.png, hide.png, p70.png, dump.txt
The run dir holds p70.png .. p74.png, p70.notes and dump.txt. Prints "L PASS|FAIL|SKIP ..." last.
"""

import math
import os
import re
import struct
import sys
import zlib

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cell_lit_check import euler, fields, record, walk  # noqa: E402  (the plugin walk only)
from cell_glow_check import NODES, apply, av_object, compose, nif_read  # noqa: E402  (the NIF header and nodes)

PLACED = (b'STAT', b'MSTT', b'ACTI', b'FURN', b'DOOR', b'MISC', b'CONT', b'FLOR', b'TERM', b'IDLM', b'LIGH')
SHAPES = ('BSTriShape', 'BSSubIndexTriShape', 'BSMeshLODTriShape', 'BSDynamicTriShape')


def bgem(path):
    """(effect lighting flag, lighting influence) of an effect material file, or None."""
    try:
        b = open(path, 'rb').read()
    except OSError:
        return None
    if b[:4] != b'BGEM':
        return None
    ver, = struct.unpack_from('<I', b, 4)
    o = 8 + 4 + 16 + 4 + 9 + 1      # tile flags, UV offset + scale, alpha, blend mode (1 + 4 + 4), test ref
    o += 10                         # test, z write, z test, SSR, wet SSR, decal, two sided, no fade, non occluder, refraction
    o += 1 + 4                      # refraction falloff, power
    o += 5 if ver < 10 else 1       # environment mapping + mask scale | depth bias
    o += 1                          # grayscale to palette color
    if ver >= 6:
        o += 1                      # mask writes
    for _ in range(8 if ver >= 11 else 5):
        n, = struct.unpack_from('<I', b, o)
        o += 4 + n
    if ver >= 10:
        o += 5
    lit = b[o + 1] != 0             # blood, EFFECT LIGHTING, falloff, falloff color, palette alpha, soft
    o += 6 + 16 + 16                # those six, base color + scale, the four falloff values
    influence, = struct.unpack_from('<f', b, o)
    return lit, influence


def merge(a, c):
    """the smallest sphere holding spheres a and c ((center, radius) or None)."""
    if a is None:
        return c
    d = np.linalg.norm(c[0] - a[0])
    if d + c[1] <= a[1]:
        return a
    if d + a[1] <= c[1]:
        return c
    r = (d + a[1] + c[1]) / 2
    return a[0] + (c[0] - a[0]) * ((r - a[1]) / d), r


def mat_key(name):
    """a material path as the swap rows compare it: lower case, forward slashes, no leading materials/."""
    k = name.replace('\\', '/').strip().lower()
    return k[len('materials/'):] if k.startswith('materials/') else k


def model_facts(path, data, swap=()):
    """(bound sphere of the whole model in model space, [lighting influence of each lit effect shape]);
    swap = the placement's material swap rows ((original, replacement), ...)."""
    b, blocks, strings, _ = nif_read(path)
    info, parent = {}, {}
    for i, (t, off, _size) in enumerate(blocks):
        if t in NODES or t in SHAPES:
            name, xf, o = av_object(b, off, strings)
            rec = dict(name=name, xf=xf)
            if t in NODES:
                n, = struct.unpack_from('<I', b, o)
                for k in struct.unpack_from('<%di' % n, b, o + 4):
                    if k >= 0:
                        parent[k] = i
            else:
                rec['sphere'] = struct.unpack_from('<4f', b, o)
                rec['shader'], = struct.unpack_from('<i', b, o + 16 + 4)
                rec['nverts'], = struct.unpack_from('<H', b, o + 16 + 12 + 8 + 4)
            info[i] = rec
    bound, lit = None, []
    for i, rec in info.items():
        if 'sphere' not in rec or rec['nverts'] == 0:
            continue
        xf, k, marker = rec['xf'], parent.get(i), rec['name'].lower().startswith('editormarker')
        while k is not None:
            marker |= info[k]['name'].lower().startswith('editormarker')
            if k in parent:     # the root's own transform is the placement's in the game, never the file's
                xf = compose(info[k]['xf'], xf)
            k = parent.get(k)
        if marker:
            continue
        cx, cy, cz, rad = rec['sphere']
        bound = merge(bound, (apply(xf, np.array([cx, cy, cz])), rad * xf[2]))
        s = rec['shader']
        if not (0 <= s < len(blocks)) or blocks[s][0] != 'BSEffectShaderProperty':
            continue
        o = blocks[s][1]
        name_i, nextra = struct.unpack_from('<iI', b, o)
        o += 8 + 4 * nextra + 4
        sf2, = struct.unpack_from('<I', b, o + 4)
        o += 8 + 16
        n, = struct.unpack_from('<I', b, o)
        o += 4 + n
        fact = (bool(sf2 & 0x40000000), b[o + 1] / 255.0)
        name = strings[name_i] if 0 <= name_i < len(strings) else ''
        for orig, repl in swap:
            if repl and mat_key(orig) == mat_key(name):
                name = repl
                break
        if name.lower().endswith('.bgem'):
            rel = name.replace('\\', '/')
            if not rel.lower().startswith('materials/'):
                rel = 'Materials/' + rel
            fact = bgem(os.path.join(data, rel)) or fact
        if fact[0] and fact[1] > 0:
            lit.append(fact[1])
    return bound, lit


def cell_of(esm, cell_edid):
    """(lights in record order, {reference: placement}) of an interior cell, from our own walk of the plugin."""
    buf = open(esm, 'rb').read()
    cell_form, ligh, models, refs, start = None, {}, {}, [], {}
    mods, mswp = {}, {}     # a base's own material swap; each swap's (original, replacement) rows
    for t, form, off, stack in walk(buf):
        if t == b'MSWP':
            size, rflags = struct.unpack_from('<II', buf, off + 4)
            raw = buf[off + 24:off + 24 + size]
            if rflags & 0x00040000:
                raw = zlib.decompress(raw[4:])
            rows = []
            for tag, p in fields(raw):
                s = p.split(b'\0')[0].decode('cp1252', 'replace')
                if tag == b'BNAM':
                    rows.append([s, ''])
                elif tag == b'SNAM' and rows:
                    rows[-1][1] = s
            mswp[form] = tuple((a, b) for a, b in rows)
            continue
        if t == b'CELL' and cell_form is None and all(g[2] != 1 for g in stack):
            _, f = record(buf, off)
            if f.get(b'EDID', b'').split(b'\0')[0].decode('cp1252', 'replace') == cell_edid:
                cell_form = form
        elif t == b'REFR':
            if cell_form is not None and any(g[1] == cell_form and g[2] in (6, 8, 9) for g in stack):
                flags, f = record(buf, off)
                refs.append((form, flags, f))
                x = f.get(b'XESP', b'')
                x = struct.unpack_from('<II', x) if len(x) >= 8 else (0, 0)
                start[form] = (bool(flags & 0x800), x[0], bool(x[1] & 1))
        elif t in PLACED:
            _, f = record(buf, off)
            m = f.get(b'MODL', b'').split(b'\0')[0].decode('cp1252', 'replace')
            if m:
                models[form] = m
            if len(f.get(b'MODS', b'')) >= 4:
                mods[form] = struct.unpack_from('<I', f[b'MODS'])[0]
            if t == b'LIGH' and b'DATA' in f:
                ligh[form] = f

    def hidden(form, depth=0):
        off, par, opposite = start[form]
        if par and depth < 16:
            off = (hidden(par, depth + 1) if par in start else False) == (not opposite)
        return off

    lights, placed = [], {}
    for form, flags, f in refs:
        if flags & 0x20 or b'NAME' not in f or b'DATA' not in f:
            continue
        base = struct.unpack_from('<I', f[b'NAME'])[0]
        pos = np.array(struct.unpack_from('<3f', f[b'DATA'], 0))
        rot = struct.unpack_from('<3f', f[b'DATA'], 12)
        R = euler(-rot[0], -rot[1], -rot[2])
        if base in models:
            sw = struct.unpack_from('<I', f[b'XMSP'])[0] if len(f.get(b'XMSP', b'')) >= 4 else mods.get(base, 0)
            placed[form] = dict(pos=pos, R=R, scale=struct.unpack_from('<f', f[b'XSCL'])[0] if b'XSCL' in f else 1.0,
                                model=models[base], hidden=hidden(form), swap=mswp.get(sw, ()))
        b = ligh.get(base)
        if b is None or flags & 0x800:
            continue
        d = b[b'DATA']
        radius, = struct.unpack_from('<I', d, 4)
        lf, = struct.unpack_from('<I', d, 12)
        if lf & 0x20 or lf & 0x100000:      # off; Ambient Only
            continue
        exponent, fov = struct.unpack_from('<2f', d, 16) if len(d) >= 28 else (1.0, 90.0)
        fade = struct.unpack_from('<f', b[b'FNAM'])[0] if b'FNAM' in b else 1.0
        xlig = struct.unpack_from('<%df' % (len(f[b'XLIG']) // 4), f[b'XLIG']) if b'XLIG' in f else ()
        r = radius + (struct.unpack_from('<f', f[b'XRDS'])[0] if b'XRDS' in f else 0.0)
        offset = xlig[1] if len(xlig) >= 2 else 0.0
        col = (np.array(list(d[8:11]), float) / 255.0) ** 2.2
        if r <= 0 or (col * (fade + offset)).max() <= 0:
            continue
        spot = bool(lf & 0x4400)
        lights.append(dict(pos=pos, r=r, c=col * (fade + offset), c0=col * fade, spot=spot, aim=R @ np.array([1.0, 0.0, 0.0]),
                           cos=math.cos(math.radians(fov + (xlig[0] if xlig else 0.0)) / 2) if spot else -2.0,
                           cone=exponent))
    return lights, placed


def pick(lights, center, radius, strong):
    """THE GAME'S RULE for a placed model's lights (its first four are what an effect shape of it is lit by):
    a light is a candidate when it reaches the model's bounding sphere (distance to the center - the sphere's
    radius < the light's radius). A model whose sphere is under 150 units sorts its candidates by
    (distance - sphere radius) / light radius, LARGEST first; a bigger one keeps the lights' own order."""
    cand = []
    for i, L in enumerate(lights):
        d = np.linalg.norm(L['pos'] - center)
        if not d - radius < L['r']:
            continue
        if strong:
            key = L['c'].max() * (1 - min(d / L['r'], 1.0) ** 2) ** 2.2
        else:
            key = (d - radius) / L['r'] if radius < 150 else 0.0
        cand.append((-key, len(cand), i))
    return [i for _, _, i in sorted(cand)[:4]]


def light_at(L, P, power=2.2, color='c'):
    v = L['pos'] - P
    d = np.linalg.norm(v, axis=1)
    a = (1 - np.clip(d / L['r'], 0, 1) ** 2) ** power
    if L['spot']:
        c = np.clip(-(v / np.maximum(d, 1e-3)[:, None]) @ L['aim'], 0, 1)
        base = np.clip(1 - (1 - c) / max(1 - (L['cos'] + 0.001), 1e-4), 0, 1)
        a = a * np.minimum(base ** max(L['cone'], 1e-3), 1)
    return a[:, None] * L[color][None, :]


def multiplier(lights, which, influence, base, P, power=2.2, color='c'):
    E = np.tile(base, (len(P), 1))
    for i in which:
        E = E + light_at(lights[i], P, power, color)
    return np.minimum(1 - influence + influence * E, 4.0)


def directional(esm, cell_edid):
    """the interior's directional color (linear), black when its fade is 0 or it has none."""
    buf = open(esm, 'rb').read()
    for t, _form, off, stack in walk(buf):
        if t == b'CELL' and all(g[2] != 1 for g in stack):
            _, f = record(buf, off)
            if f.get(b'EDID', b'').split(b'\0')[0].decode('cp1252', 'replace') == cell_edid:
                x = f.get(b'XCLL', b'')
                if len(x) < 36:
                    return np.zeros(3), 'no XCLL'
                fade, = struct.unpack_from('<f', x, 28)
                return (np.array(list(x[4:7]), float) / 255.0) ** 2.2 * fade, 'XCLL'
    return np.zeros(3), 'no cell'


def ids_of(run):
    """(model serial per pixel, -1 where no lit effect drew; the number of models the viewer lit)."""
    a = np.round(np.asarray(Image.open(os.path.join(run, 'p70.png')).convert('RGB'), float)).astype(int)
    n = 0
    for line in open(os.path.join(run, 'dump.txt'), encoding='utf-8', errors='replace'):
        n += line.startswith('model ')
    idn = a[..., 0] + a[..., 1] * 256 + a[..., 2] * 65536 - 1
    idn[idn >= n] = -1
    return idn, n


def unlit(run):
    """stage U: outside the lit effects (their pixels, grown by 2) the frame equals the exe before the lane's."""
    A, B, H = (np.asarray(Image.open(os.path.join(run, t + '.png')).convert('RGB'), int) for t in ('before', 'on', 'hide'))
    idn, _ = ids_of(run)
    if A.shape != B.shape or A.shape[:2] != idn.shape:
        print('U FAIL the frames differ in size')
        return 1
    lit = idn >= 0
    grown = lit.copy()
    for dy in range(-2, 3):
        for dx in range(-2, 3):
            grown |= np.roll(lit, (dy, dx), (0, 1))
    out = ~grown
    moved = (np.abs(A - B).max(axis=2) > 0) & out
    effects = (np.abs(B - H).max(axis=2) > 0) & out
    inside = (np.abs(A - B).max(axis=2) > 0) & lit
    n, e = int(moved.sum()), int(effects.sum())
    print('U lit effects cover %d of %d pixels (%d of them moved against the exe before); outside them %d pixels show an'
          ' effect shape without the flag' % (int(lit.sum()), lit.size, int(inside.sum()), e))
    if e < 300:
        print('U SKIP %d pixels of unlit effects outside the lit ones: under 300, this view proves nothing' % e)
        return 2
    print('U %s unlit effects: %d of %d pixels outside the lit effects differ from the exe before (largest step %d of 255)'
          % ('PASS' if n == 0 else 'FAIL', n, int(out.sum()), int(np.abs(A - B).max(axis=2)[out].max()) if out.any() else 0))
    return 0 if n == 0 else 1


def main():
    if sys.argv[1] == '--unlit':
        return unlit(sys.argv[2])
    esm, data, cell, run = sys.argv[1:5]
    strong = len(sys.argv) > 5 and sys.argv[5] == 'strong'
    floor = float(sys.argv[6]) if len(sys.argv) > 6 else 0.95
    band = float(sys.argv[7]) if len(sys.argv) > 7 else 0.05
    head = open(os.path.join(run, 'dump.txt'), encoding='utf-8', errors='replace').readline()
    said = re.search(r'pick=(\w+)', head)
    if not said or (said.group(1) == 'strong') != strong:
        print('L FAIL the viewer picked "%s", this gate expects "%s"' % (said.group(1) if said else '?', 'strong' if strong else 'game'))
        return 1
    img = {p: np.asarray(Image.open(os.path.join(run, 'p%d.png' % p)).convert('RGB'), float) for p in range(70, 75)}
    notes = open(os.path.join(run, 'p70.notes'), encoding='utf-8', errors='replace').read()
    m = re.search(r'cell lighting: .*center=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)', notes)
    if not m:
        print('L FAIL no "cell lighting ... center=" line in the notes (the cell view published nothing)')
        return 1
    center = np.array([float(v) for v in m.groups()])
    dump = {}
    for line in open(os.path.join(run, 'dump.txt'), encoding='utf-8', errors='replace'):
        g = re.match(r'model (\d+) ref ([0-9a-f]+) center (\S+) (\S+) (\S+) radius (\S+) lights(.*?) model (.*)$', line.strip())
        if g:
            dump[int(g.group(1))] = dict(ref=int(g.group(2), 16), center=np.array([float(g.group(k)) for k in (3, 4, 5)]),
                                         radius=float(g.group(6)), model=g.group(8),
                                         lights=[np.array([float(v) for v in t.split(':')[1].split(',')])
                                                 for t in g.group(7).split()])
    lights, placed = cell_of(esm, cell)
    base, where = directional(esm, cell)
    print('L %s: %d lights and %d placed models from the plugin; directional %.3f %.3f %.3f (%s); the viewer lit %d models'
          % (cell, len(lights), len(placed), base[0], base[1], base[2], where, len(dump)))

    # ---- our own list of lit models, and the viewer's against it
    cache, ours = {}, {}
    for form, p in placed.items():
        key = (p['model'], p['swap'])
        if key not in cache:
            path = os.path.join(data, 'Meshes', p['model'].replace('\\', '/'))
            try:
                cache[key] = model_facts(path, data, p['swap']) if os.path.isfile(path) else (None, [])
            except (struct.error, IndexError, ValueError):
                cache[key] = (None, [])
        bound, lit = cache[key]
        if bound is None or not lit or p['hidden']:
            continue
        c = p['pos'] + p['R'] @ bound[0] * p['scale']
        r = bound[1] * p['scale']
        ours[form] = dict(center=c, radius=r, lit=lit, which=pick(lights, c, r, strong), model=p['model'])
    seen = {d['ref'] for d in dump.values()}
    miss = sorted(set(ours) - seen)
    extra = sorted(seen - set(ours))
    dc = dr = 0.0
    same_pick = 0
    for d in dump.values():
        o = ours.get(d['ref'])
        if o is None:
            continue
        dc = max(dc, float(np.linalg.norm(o['center'] - d['center'])))
        dr = max(dr, abs(o['radius'] - d['radius']))
        mine = [lights[i]['pos'] for i in o['which']]
        same_pick += len(mine) == len(d['lights']) and all(np.linalg.norm(a - b) < 0.5 for a, b in zip(mine, d['lights']))
    print('L models: ours %d, the viewer\'s %d, ours it did not light %d, its that are not ours %d; bounds differ by at most'
          ' %.1f (center) %.1f (radius); the four lights agree, in order, on %d of %d'
          % (len(ours), len(dump), len(miss), len(extra), dc, dr, same_pick, len(seen & set(ours))))
    for form in miss[:6]:
        print('L   not lit by the viewer: %08x %s' % (form, ours[form]['model']))
    for form in extra[:6]:
        print('L   not ours: %08x' % form)

    # ---- the pixels
    idn = (np.round(img[70][..., 0]) + np.round(img[70][..., 1]) * 256 + np.round(img[70][..., 2]) * 65536).astype(int) - 1
    ok = (idn >= 0) & (idn < max(len(dump), 1))
    for dy in (-1, 0, 1):               # an edge of a card blends two answers: the 3 x 3 around a pixel is one model
        for dx in (-1, 0, 1):
            ok &= np.roll(idn, (dy, dx), (0, 1)) == idn
    ok[0, :] = ok[-1, :] = ok[:, 0] = ok[:, -1] = False
    P = np.round(img[71]) * 256 + np.round(img[72]) + 0.5 - 32768.0 + center
    M = (np.round(img[73]) * 256 + np.round(img[74])) * 4.0 / 65535.0
    total = int(ok.sum())
    used = agree = stray = unknown = mixed = 0
    sum_v = sum_e = 0.0
    mutants = {'white': 0, 'nofade': 0, 'all': 0, 'nopower': 0}
    msum = dict.fromkeys(mutants, 0.0)
    worst = []
    corners = np.array([[x, y, z] for x in (-0.5, 0.5) for y in (-0.5, 0.5) for z in (-0.5, 0.5)] + [[0, 0, 0]])
    everything = list(range(len(lights)))
    for serial in np.unique(idn[ok]):
        sel = ok & (idn == serial)
        n = int(sel.sum())
        o = ours.get(dump[serial]['ref']) if serial in dump else None
        if o is None:
            unknown += n
            continue
        if max(o['lit']) - min(o['lit']) > 1e-4:
            mixed += n
            continue
        p, v = P[sel], M[sel]
        inside = np.linalg.norm(p - o['center'], axis=1) <= o['radius'] + 2.0
        stray += int((~inside).sum())
        p, v = p[inside], v[inside]
        if not len(p):
            continue
        k = o['lit'][0]
        e = np.stack([multiplier(lights, o['which'], k, base, p + c) for c in corners])
        lo, hi, mid = e.min(axis=0), e.max(axis=0), e[-1]
        tol = 0.004 + 0.01 * hi
        good = np.all((v >= lo - tol) & (v <= hi + tol), axis=1)
        used += len(p)
        agree += int(good.sum())
        sum_v += float(v.sum())
        sum_e += float(mid.sum())
        if (~good).any():
            j = int(np.argmax(np.abs(v - mid).max(axis=1)))
            worst.append((float(np.abs(v[j] - mid[j]).max()), dump[serial]['ref'], p[j], v[j], mid[j]))
        # what each red control would write here, against the same bars (predicted, no viewer)
        for name in mutants:
            if name == 'white':
                w = np.ones_like(mid)
            elif name == 'nofade':
                w = multiplier(lights, o['which'], k, base, p, color='c0')
            elif name == 'all':
                w = multiplier(lights, everything, k, base, p)
            else:
                w = multiplier(lights, o['which'], k, base, p, power=1.0)
            mutants[name] += int(np.all((w >= lo - tol) & (w <= hi + tol), axis=1).sum())
            msum[name] += float(w.sum())
    print('L pixels: %d of one model; %d checked, %d outside their model\'s bound, %d of a model not ours, %d of a model'
          ' with two influences' % (total, used, stray, unknown, mixed))
    if used < 300:
        print('L SKIP %d lit effect pixels checked: under 300, this view proves nothing' % used)
        return 2
    share, ratio = agree / used, sum_v / max(sum_e, 1e-9)
    print('L mean multiplier: the viewer\'s %.4f, expected %.4f' % (sum_v / used / 3, sum_e / used / 3))
    for name in ('white', 'nofade', 'all', 'nopower'):
        print('L   predicted red %-7s agree %.1f%% total %.3f' % (name, 100.0 * mutants[name] / used, msum[name] / max(sum_e, 1e-9)))
    for w in sorted(worst, key=lambda t: -t[0])[:3]:
        print('L   worst: ref %08x at %.0f %.0f %.0f wrote %.3f %.3f %.3f, expected %.3f %.3f %.3f'
              % ((w[1],) + tuple(w[2]) + tuple(w[3]) + tuple(w[4])))
    good = share >= floor and abs(ratio - 1) <= band and not miss and stray <= 0.02 * max(total, 1)
    print('L %s lit effects: %.1f%% of %d pixels agree (floor %.0f%%), the viewer\'s total over expected %.3f (within %.0f%%)'
          % ('PASS' if good else 'FAIL', 100.0 * share, used, 100.0 * floor, ratio, 100.0 * band))
    return 0 if good else 1


if __name__ == '__main__':
    sys.exit(main())
