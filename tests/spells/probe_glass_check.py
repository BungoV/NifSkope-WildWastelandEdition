#!/usr/bin/env python3
"""Which shapes of a real cell the probe bake takes for glass, and what they do to the light
(lane BAKE4, 2026-10-02).

  probe_glass_check.py <run dir> [--data <loose Data>] [--game <game Data>] [--esm <plugin> --cell <editor id>]

The run dir holds what one probe_glass.sh pass wrote: glass.tsv (the cell view's census of every
blended or effect shape it met while it built the bake's soup: its facts, whether it fed the
shape as glass, how many triangles, their mean transmittance), soup.psp (panes in its GLS1 tail),
bake/ (the .tbk v4 files) and souprefs.tsv (every reference the soup took: form, role, type).

Nothing here asks NifSkope what a material says. The checker reads each material file's bytes
itself (the loose folder first, then the game's material archive, both with its own readers)
and decides with the rule written here:

  a pane = a shape with a material file that says: blending on, with factors 6/7 (source alpha
           over), not a decal, environment mapped, (effect materials) no soft depth fade, and an
           opacity above 0

Measured on the game's 6899 materials, 2026-10-02: every window and car glass is over +
environment mapped; every mist, beam and glow card is soft and not environment mapped; additive
glass (6/0: the cryo pod windows) adds light and takes none away. A blended effect shape with no
material file is never a pane: Vault111Cryo has 363 such shapes, and the 33 whose own flags read
like glass are water drip splashes, lamp covers and klaxon shells.

  A census    every row's pane verdict is the checker's own; a pane's material must be readable
  B soup      the soup's pane triangles = the census's; every pane's T within [1 - opacity, 1]
              (opacity = the file's own, squared for an effect material: its shader applies it twice)
  C light     per link, the pane product along the segment from the probe to the surfel's stored
              position (the checker's own triangle test) against the link's stored tint:
                seen   of the link weight whose segment crosses a pane, the share the bake tinted
                mass   bake attenuation / bundle attenuation, summed over all links; a link's
                       bundle is nine segments: to the stored position and to the centers of the
                       eight octants of the surfel's cell (a link gathers every ray that lands in
                       its cell, so its tint is a mean over the cell, not the center's)
                clear  of the link weight whose segment is clear, the share the bake left clear
Gates, set before the first run and never moved: seen >= 0.80, mass in [0.75, 1.33], clear >= 0.97.
C is not run (and fails) when A failed: a wrong census is not worth an hour of segments.
Two corrections of the checker's own walk, both after a real cell failed and both measured:
* Vault111Cryo failed at seen 0.656, mass 0.748 while the segment ran along the link's stored
  direction to the surfel's distance. That end point is not the surfel: 100 links "crossed" a pane
  the bake stored clear, and for all 100 the segment to the surfel's own position is clear too.
* NorthEndMeanPastries (seven sneeze guards in a row, 12 triangles each) failed at mass 0.586 with
  the center segment alone as the denominator: a center that threads two or three small panes
  reads T 0.18 where the cell's mean is 0.46. With the bundle the same files read 0.860 (and
  Vault111Cryo 0.893, was 1.053). The center alone is still printed.

  T types     (with --esm and --cell) THE BAKE SEES THE FIXED WORLD ONLY. Every reference in
              souprefs.tsv is looked up in the plugin by the gates' own reader; its base record's
              type must be one of STAT MSTT TREE FURN CONT ACTI TERM FLOR LIGH (role 1, triangles)
              or DOOR (role 2, a box), or a static collection (SCOL) whose every part is one of
              the first list. Pick-up items, actors and everything else: zero.
One verdict line per stage, then `glass PASS` or `glass FAIL`; exit 0 on PASS.
"""
import os
import struct
import sys
import zlib

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from probe_bake import (ALB_MAGIC, GLS_MAGIC, SOUP_MAGIC, files_in, floordiv, glass_through, structure,  # noqa: E402
                        surfel_of)

SEEN_MIN, MASS_LO, MASS_HI, CLEAR_MIN = 0.80, 0.75, 1.33, 0.97
OCTANTS = [(x, y, z) for x in (0.25, 0.75) for y in (0.25, 0.75) for z in (0.25, 0.75)]
SOUP_TYPES = ('STAT', 'MSTT', 'TREE', 'FURN', 'CONT', 'ACTI', 'TERM', 'FLOR', 'LIGH')
PLACED = (b'REFR', b'ACHR', b'PGRE', b'PMIS', b'PHZD', b'PARW', b'PBAR', b'PBEA', b'PCON', b'PFLA')
COLS = ('ref', 'model', 'block', 'name', 'material', 'kind', 'matread', 'blend', 'src', 'dst', 'decal', 'env',
        'soft', 'alpha', 'pane', 'tris', 'tr', 'tg', 'tb')


def archive_files(path):
    """name -> bytes of a general archive (header, 36-byte records, name table)."""
    b = open(path, 'rb').read()
    if b[:4] != b'BTDX' or b[8:12] != b'GNRL':
        raise ValueError('not a general archive')
    ver, = struct.unpack_from('<I', b, 4)
    n, nt = struct.unpack_from('<IQ', b, 12)
    o = 24 + (8 if ver in (2, 3) else 0) + (4 if ver == 3 else 0)
    out = {}
    for i in range(n):
        _, _, _, _, off, pk, un, _ = struct.unpack_from('<I4sIIQIII', b, o + 36 * i)
        ln, = struct.unpack_from('<H', b, nt)
        name = b[nt + 2:nt + 2 + ln].decode('latin1').lower().replace(chr(92), '/')
        nt += 2 + ln
        d = b[off:off + (pk or un)]
        out[name] = zlib.decompress(d) if pk else d
    return out


def material(b):
    """The blend facts of a BGSM / BGEM, read field by field."""
    if b[:4] not in (b'BGEM', b'BGSM'):
        raise ValueError('not a material')
    o = [4]

    def u32():
        v = struct.unpack_from('<I', b, o[0])[0]
        o[0] += 4
        return v

    def f32():
        v = struct.unpack_from('<f', b, o[0])[0]
        o[0] += 4
        return v

    def u8():
        v = b[o[0]]
        o[0] += 1
        return v

    def text():
        n = u32()
        o[0] += n

    m = dict(effect=b[:4] == b'BGEM', soft=0, env=0)
    ver = u32()
    u32()                                  # tile flags
    for _ in range(4):
        f32()                              # uv offset, scale
    m['alpha'] = f32()
    m['blend'], m['src'], m['dst'] = u8(), u32(), u32()
    u8(), u8()                             # test ref, test
    u8(), u8(), u8(), u8()                 # z write, z test, reflections, wetness reflections
    m['decal'] = u8()
    u8(), u8(), u8(), u8(), u8()           # two sided, decal no fade, non occluder, refraction, its falloff
    f32()                                  # refraction power
    if ver < 10:
        m['env'] = u8()
        f32()
    else:
        u8()
    u8()                                   # grayscale to palette color
    if ver >= 6:
        u8()                               # mask writes
    if m['effect']:
        for _ in range(5):
            text()
        if ver >= 10:
            for _ in range(3):
                text()
            m['env'] = u8()
            f32()
        u8(), u8(), u8(), u8(), u8()       # blood, effect lighting, falloff, falloff color, palette alpha
        m['soft'] = u8()
    elif ver >= 10:
        raise ValueError('a lighting material of version %d' % ver)
    return m


def is_pane(m, nif_decal=False):
    return bool(m['blend'] and m['src'] == 6 and m['dst'] == 7 and not m['decal'] and not nif_decal and m['env']
                and not m['soft'] and m['alpha'] > 0.0)


def material_key(name):
    p = name.lower().replace(chr(92), '/')
    i = p.rfind('materials/')
    return p[i:] if i >= 0 else 'materials/' + p


def read_soup_glass(path):
    with open(path, 'rb') as f:
        magic, ntri, ndoor = struct.unpack('<III', f.read(12))
        if magic != SOUP_MAGIC:
            raise ValueError('not a soup')
        f.seek(ntri * 36 + ndoor * 28, 1)
        tail = f.read(8)
        if len(tail) == 8 and struct.unpack('<I', tail[:4])[0] == ALB_MAGIC:
            f.seek(struct.unpack('<I', tail[4:])[0] * 3, 1)
            tail = f.read(8)
        if len(tail) < 8 or struct.unpack('<I', tail[:4])[0] != GLS_MAGIC:
            return np.zeros((0, 3, 3)), np.zeros((0, 3))
        n = struct.unpack('<I', tail[4:])[0]
        tris = np.frombuffer(f.read(n * 36), dtype='<f4').reshape(n, 3, 3).astype(np.float64)
        T = np.frombuffer(f.read(n * 3), dtype='u1').reshape(n, 3).astype(np.float64) / 255.0
    return tris, T


def soup_types(run, esm, cell):
    """Stage T: (ok, verdict line)."""
    from cell_lit_check import fields, record, walk   # the gates' own plugin reader (python, no NifSkope)
    rows = []
    try:
        for line in open(os.path.join(run, 'souprefs.tsv'), encoding='utf-8', errors='replace'):
            f = line.rstrip('\r\n').split('\t')
            if len(f) == 3:
                rows.append((int(f[0], 16), int(f[1]), f[2]))
    except (OSError, ValueError) as e:
        return False, 'T FAIL: no reference list (%s)' % e
    buf = open(esm, 'rb').read()
    want = {r[0] for r in rows}
    kind, at, ref_at, cell_form, placed = {}, {}, {}, None, []
    for t, form, off, stack in walk(buf):
        if t in PLACED:
            if form in want:
                ref_at[form] = (t, off)
            if cell_form is not None and any(g[1] == cell_form and g[2] in (6, 8, 9) for g in stack):
                placed.append((t, off))
            continue
        if t == b'CELL' and cell_form is None and all(g[2] != 1 for g in stack):
            if record(buf, off)[1].get(b'EDID', b'').split(b'\0')[0].decode('cp1252', 'replace') == cell:
                cell_form = form
        kind[form] = t.decode('latin1')
        if t == b'SCOL':
            at[form] = off

    def base_of(t, off):
        """(base type, the types of its parts when it is a static collection)."""
        if t != b'REFR':
            return t.decode('latin1'), None
        f = record(buf, off)[1]
        if b'NAME' not in f:
            return 'no base', None
        b = struct.unpack_from('<I', f[b'NAME'])[0]
        bt = kind.get(b, 'unknown')
        if bt != 'SCOL':
            return bt, None
        size, flags = struct.unpack_from('<II', buf, at[b] + 4)
        data = buf[at[b] + 24:at[b] + 24 + size]
        if flags & 0x00040000:
            data = zlib.decompress(data[4:])
        return bt, {kind.get(struct.unpack_from('<I', p)[0], 'unknown') for ft, p in fields(data) if ft == b'ONAM'}

    def fmt(d):
        return ' '.join('%s %d' % kv for kv in sorted(d.items())) or 'none'

    # one row per placed model: a static collection gives one per part, under the collection's reference
    in_soup, outside, said_off, done = {}, [], 0, {}
    for form, role, said in rows:
        if form not in done:
            done[form] = base_of(*ref_at[form]) if form in ref_at else ('not in the plugin', None)
            in_soup[done[form][0]] = in_soup.get(done[form][0], 0) + 1
        bt, parts = done[form]
        if parts is not None:   # a collection is fixed world when every part is; a row names the part's type
            good = role == 1 and bool(parts) and all(p in SOUP_TYPES for p in parts)
            said_off += said not in parts
        else:
            good = (role == 1 and bt in SOUP_TYPES) or (role == 2 and bt == 'DOOR')
            said_off += bt != said
        if not good:
            outside.append('%08X %s' % (form, bt))
    cell_types = {}
    for t, off in placed:
        bt = base_of(t, off)[0]
        cell_types[bt] = cell_types.get(bt, 0) + 1
    ok = bool(rows) and not outside and not said_off and cell_form is not None
    return ok, ('T %s: %d references in the soup (%d placed models), by the plugin\'s own base records: %s; outside '
                'the fixed-world list: %d%s%s; the cell places %d: %s' % (
                    'PASS' if ok else 'FAIL', len(done), len(rows), fmt(in_soup), len(set(outside)),
                    (' (%s)' % ', '.join(sorted(set(outside))[:4])) if outside else '',
                    ('; %d rows name another type than the plugin' % said_off) if said_off else '',
                    len(placed), fmt(cell_types) if cell_form is not None else 'CELL NOT FOUND'))


def main(a):
    run = a[0]
    data = a[a.index('--data') + 1] if '--data' in a else 'E:/Tools/Fallout 4/DataUnpacked/Data'
    game = a[a.index('--game') + 1] if '--game' in a else 'X:/Programs/Steam/steamapps/common/Fallout 4/Data'
    bad = 0

    # ---- A: the census, re-decided from the material bytes
    rows = []
    try:
        for line in open(os.path.join(run, 'glass.tsv'), encoding='utf-8', errors='replace'):
            f = line.rstrip('\r\n').split('\t')
            if len(f) == len(COLS) and f[0] != 'ref':
                rows.append(dict(zip(COLS, f)))
    except OSError as e:
        print('A FAIL: no census (%s)' % e)
        print('glass FAIL')
        return 1
    arch = {}
    ap = os.path.join(game, 'Fallout4 - Materials.ba2')
    if os.path.isfile(ap):
        arch = archive_files(ap)
    seen, wrong, unread, facts_off = {}, [], [], []
    n_pane = n_nif = 0
    classes = {}
    for r in rows:
        nif_decal = False
        m = None
        if r['material']:
            key = material_key(r['material'])
            if key not in seen:
                b = None
                lp = os.path.join(data, key)
                if os.path.isfile(lp):
                    b = open(lp, 'rb').read()
                elif key in arch:
                    b = arch[key]
                try:
                    seen[key] = material(b) if b else None
                except Exception:
                    seen[key] = None
            m = seen[key]
        said = dict(blend=int(r['blend']), src=int(r['src']), dst=int(r['dst']), decal=int(r['decal']),
                    env=int(r['env']), soft=int(r['soft']), alpha=float(r['alpha']))
        if m is None:
            # no material file: never a pane, whatever the NIF's own flags say
            n_nif += 1
            want = False
            if int(r['pane']) and r['material']:
                unread.append(r['material'])
            r['own_alpha'] = said['alpha']
            cls = 'no material'
        else:
            # the NIF's own decal bit still counts: the census decal column is material OR NIF
            nif_decal = bool(said['decal']) and not m['decal']
            want = is_pane(m, nif_decal)
            r['own_alpha'] = m['alpha']
            if (bool(m['blend']), m['src'], m['dst'], bool(m['env']), bool(m['soft'])) != (
                    bool(said['blend']), said['src'], said['dst'], bool(said['env']), bool(said['soft'])) \
                    or abs(m['alpha'] - said['alpha']) > 1e-4:
                facts_off.append(os.path.basename(material_key(r['material'])))
            if not m['blend']:
                cls = 'not blended'
            elif (m['src'], m['dst']) != (6, 7):
                cls = 'blend %d/%d' % (m['src'], m['dst'])
            elif m['decal'] or nif_decal:
                cls = 'decal'
            elif m['soft']:
                cls = 'soft card'
            elif not m['env']:
                cls = 'over, no environment map'
            else:
                cls = 'pane'
        classes[cls] = classes.get(cls, 0) + 1
        r['want'] = want
        n_pane += int(want)
        if bool(int(r['pane'])) != want:
            wrong.append('%s %s (%s: fed %s, the material says %s)' % (
                r['ref'], os.path.basename(r['model']), os.path.basename(r['material']) or 'no material',
                r['pane'], int(want)))
    a_ok = bool(rows) and not wrong and not unread and not facts_off
    names = sorted({os.path.basename(material_key(r['material'])) for r in rows if r['want'] and r['material']})
    print('A %s: %d blended or effect shapes, %d panes by the checker\'s own read of %d material files '
          '(%d rows without one); classes %s; pane materials: %s%s%s%s' % (
              'PASS' if a_ok else 'FAIL', len(rows), n_pane, sum(1 for v in seen.values() if v), n_nif,
              ', '.join('%s %d' % kv for kv in sorted(classes.items(), key=lambda kv: -kv[1])),
              ', '.join(names[:8]) or 'none',
              ('; WRONG %d: %s' % (len(wrong), '; '.join(wrong[:4]))) if wrong else '',
              ('; a pane whose material the checker cannot read: %s' % ', '.join(sorted(set(unread))[:4])) if unread else '',
              ('; facts differ from the file: %s' % ', '.join(sorted(set(facts_off))[:4])) if facts_off else ''))
    bad += not a_ok

    # ---- B: the soup's panes
    try:
        gtris, gT = read_soup_glass(os.path.join(run, 'soup.psp'))
    except Exception as e:
        print('B FAIL: the soup does not read (%s)' % e)
        print('glass FAIL')
        return 1
    fed = sum(int(r['tris']) for r in rows if int(r['pane']))
    want_tris = sum(int(r['tris']) for r in rows if r['want'])
    t_off = []
    for r in rows:
        if not int(r['pane']):
            continue
        # an effect shader multiplies the material's opacity in twice (res/shaders/fo4_effectshader.frag)
        op = r['own_alpha'] ** 2 if r['kind'] == 'E' else r['own_alpha']
        lo = 1.0 - min(1.0, max(0.0, op)) - 1.5 / 255.0
        t = [float(r[c]) for c in ('tr', 'tg', 'tb')]
        if min(t) < lo or max(t) > 1.0 + 1e-6:
            t_off.append('%s T %.3f..%.3f under 1 - opacity %.3f' % (os.path.basename(r['material']), min(t), max(t), lo))
    area = 0.0
    if len(gtris):
        area = float(np.linalg.norm(np.cross(gtris[:, 1] - gtris[:, 0], gtris[:, 2] - gtris[:, 0]), axis=1).sum() / 2)
    b_ok = len(gtris) == fed and fed == want_tris and not t_off and (len(gtris) > 0 or n_pane == 0)
    print('B %s: the soup holds %d pane triangles (census fed %d, the checker\'s panes have %d), area %.0f, '
          'mean T %s%s' % ('PASS' if b_ok else 'FAIL', len(gtris), fed, want_tris, area,
                           ('%.3f %.3f %.3f' % tuple(gT.mean(0))) if len(gT) else 'n/a',
                           ('; ' + '; '.join(t_off[:3])) if t_off else ''))
    bad += not b_ok

    # ---- T: what the soup took (the fixed world only)
    t_line = None
    if '--esm' in a and '--cell' in a:
        t_ok, t_line = soup_types(run, a[a.index('--esm') + 1], a[a.index('--cell') + 1])
        bad += not t_ok

    def done():
        if t_line:
            print(t_line)
        print('glass PASS' if not bad else 'glass FAIL')
        return 1 if bad else 0

    # ---- C: the light through them
    if not a_ok:
        print('C FAIL: not run, the census is wrong')
        bad += 1
        return done()
    fails = []
    tbks = structure(files_in(os.path.join(run, 'bake')), fails)
    if fails or not tbks:
        print('C FAIL: the bake does not read (%s)' % '; '.join(fails[:3] or ['no .tbk']))
        bad += 1
        return done()
    gc = gtris.mean(1) if len(gtris) else np.zeros((0, 3))
    gr = np.linalg.norm(gtris - gc[:, None, :], axis=2).max(1) if len(gtris) else np.zeros(0)
    w_cross = w_cross_seen = w_clear = w_clear_ok = 0.0
    att_bake = att_seg = att_center = w_all = 0.0
    n_links = n_tinted = 0
    for _, t in tbks:
        for i, pr in enumerate(t['probes']):
            a0, b0 = int(pr['off']), int(pr['off'] + pr['cnt'])
            if b0 <= a0:
                continue
            o = pr['pos'].astype(np.float64)
            pk = tuple(floordiv(pr['pos'][c], t['cell']) for c in range(3))
            ends, w, tint = [], [], []
            for l, x in zip(t['links'][a0:b0], t['lext'][a0:b0]):
                sf = surfel_of(t, pk, l, x)
                if sf is None:
                    continue
                ends.append(sf['pos'].astype(np.float64))
                w.append(float(l['w']))
                tint.append(x['tint'].astype(np.float64) / 255.0)
            if not ends:
                continue
            w, tint, ends = np.array(w), np.array(tint), np.array(ends)
            cell = float(t['cell'])
            reach = np.linalg.norm(ends - o, axis=1).max() + 2.0 * cell
            near = np.linalg.norm(gc - o, axis=1) <= reach + gr   # the panes this probe's segments can reach
            nt, nT = gtris[near], gT[near]
            step = max(1, 1500000 // max(1, len(nt)))   # rays per pass: the test is rays x panes

            def through(to):
                seg = to - o
                tend = np.linalg.norm(seg, axis=1)
                dirs = seg / np.maximum(tend, 1e-9)[:, None]
                return np.concatenate([glass_through(nt, nT, o, dirs[j:j + step], tend[j:j + step])[0]
                                       for j in range(0, len(dirs), step)]).mean(1)

            tc, tb = through(ends), tint.mean(1)
            # the bundle: the stored position and the centers of the eight octants of the surfel's cell
            tm = tc.copy()
            if len(nt):
                base = np.floor(ends / cell) * cell
                for q in OCTANTS:
                    tm += through(base + np.array(q) * cell)
                tm /= 9.0
            cross = tc < 0.98
            w_cross += w[cross].sum()
            w_cross_seen += w[cross & (tb < 0.995)].sum()
            w_clear += w[~cross].sum()
            w_clear_ok += w[~cross & (tb >= 0.75)].sum()
            att_bake += float((w * (1.0 - tb)).sum())
            att_seg += float((w * (1.0 - tm)).sum())
            att_center += float((w * (1.0 - tc)).sum())
            w_all += w.sum()
            n_links += len(w)
            n_tinted += int((tb < 0.995).sum())
    if n_pane == 0:
        c_ok = n_tinted == 0
        print('C %s: no panes in this cell; %d of %d links tinted' % ('PASS' if c_ok else 'FAIL', n_tinted, n_links))
    else:
        seen_share = w_cross_seen / w_cross if w_cross > 0 else 0.0
        mass = att_bake / att_seg if att_seg > 0 else 0.0
        clear = w_clear_ok / w_clear if w_clear > 0 else 0.0
        c_ok = w_cross > 0 and seen_share >= SEEN_MIN and MASS_LO <= mass <= MASS_HI and clear >= CLEAR_MIN
        print('C %s: %d links, %d tinted; a pane lies on %.2f%% of the link weight by the checker\'s own segments; '
              'seen %.3f (>= %.2f), mass %.3f (%.2f..%.2f; light taken %.3f%% bake, %.3f%% the nine-segment bundles, '
              '%.3f%% the center segments alone), clear %.4f (>= %.2f)' % (
                  'PASS' if c_ok else 'FAIL', n_links, n_tinted, 100.0 * w_cross / max(w_all, 1e-9), seen_share,
                  SEEN_MIN, mass, MASS_LO, MASS_HI, 100.0 * att_bake / max(w_all, 1e-9),
                  100.0 * att_seg / max(w_all, 1e-9), 100.0 * att_center / max(w_all, 1e-9), clear, CLEAR_MIN))
    bad += not c_ok
    return done()


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(2)
    sys.exit(main(sys.argv[1:]))
