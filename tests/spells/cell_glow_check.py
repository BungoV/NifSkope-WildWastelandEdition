#!/usr/bin/env python3
"""The camera-facing glow cards (lane GLOW1, 2026-10-01; tests/spells/cell_glow.sh).

  cell_glow_check.py <Fallout4.esm> <data root> <interior EDID> <shot dir> <look-at x,y,z> [stages, default KNC]

The shot dir holds flat.png (WW_CELL_GLOW_RED=1: every billboard welded flat, as before), on.png (the run under
test), on.notes (its census) and cam.txt (WW_CELL_CAM_DUMP: "cam=x,y,z"). Everything else is this file's OWN read
of the plugin and the meshes (nothing shared with src/lodgen.cpp or src/cellview.cpp):
  every REFR of the cell, not deleted, whose base names a MODL; the loose mesh under <data root>/Meshes; every
  BSTriShape below an NiBillboardNode (EditorMarker subtrees skipped); the card = the shape's bounding sphere,
  carried into the billboard node's frame. The game (and the viewer) drops the node's rotation and the
  reference's: the card's frame is the camera's, so it projects to an exact circle,
  center = the projected pivot + the in-frame offset, radius = focal x r / depth, focal = (H/2) / tan(35 deg).
Stages:
  K  the census says as many shapes turned to the camera as this walk finds (less the refs that start
     disabled -- own flag, or the enable parent's state followed up its chain -- printed apart)
  N  nothing else moves: outside the predicted circles (x1.15 + 4 px), |on - flat| <= 3/255 on >= 99.5%
  C  the cards show: >= 3 cards (>= 500 pixels on screen) each brighten their own circle over flat by a mean
     >= 0.5/255, with >= 30% of its pixels changed. The material is a faint haze by design (fAlpha 0.2, which
     the effect shader applies twice, times a base alpha of at most 115/255: under 2% opaque), so the circles
     gain a level or two where many cards overlap, not tens; the render's own run-to-run noise sits near a
     mean of 0.1/255.
A stage the shot does not name is measured and printed as "n/a", and does not count: beside a pod the same
cards add under 0.5/255 (measured +0.03 to +0.41 at 220 to 430 units), so a near camera cannot carry C, and
the far camera that carries C has no pixel outside the circles for N.
Prints one line per stage, then "glow PASS" or "glow FAIL".
"""
import math
import os
import re
import struct
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cell_lit_check import euler, record, walk  # noqa: E402  (the plugin walk only)


# ---- a minimal FO4 NIF reader (20.2.0.7, BS version 130): header, node tree, transforms, shape bounds
def nif_read(path):
    b = open(path, 'rb').read()
    off = b.index(b'\n') + 1
    ver, = struct.unpack_from('<I', b, off)
    off += 4 + 1
    user, nblocks = struct.unpack_from('<II', b, off)
    off += 8
    bsver, = struct.unpack_from('<I', b, off)
    off += 4

    def sstr():
        nonlocal off
        n = b[off]
        off += 1 + n
    sstr()                      # author
    if bsver > 130:
        off += 4
    sstr()                      # process script
    sstr()                      # export script
    if bsver >= 103:
        sstr()                  # max filepath
    ntypes, = struct.unpack_from('<H', b, off)
    off += 2
    types = []
    for _ in range(ntypes):
        n, = struct.unpack_from('<I', b, off)
        types.append(b[off + 4:off + 4 + n].decode('latin1'))
        off += 4 + n
    tidx = struct.unpack_from('<%dH' % nblocks, b, off)
    off += 2 * nblocks
    sizes = struct.unpack_from('<%dI' % nblocks, b, off)
    off += 4 * nblocks
    nstr, _maxlen = struct.unpack_from('<II', b, off)
    off += 8
    strings = []
    for _ in range(nstr):
        n, = struct.unpack_from('<I', b, off)
        strings.append(b[off + 4:off + 4 + n].decode('latin1'))
        off += 4 + n
    ngroups, = struct.unpack_from('<I', b, off)
    off += 4 + 4 * ngroups
    blocks = []
    for i in range(nblocks):
        t = types[tidx[i] & 0x7FFF]
        blocks.append((t, off, sizes[i]))
        off += sizes[i]
    return b, blocks, strings, bsver


def av_object(b, off, strings):
    """NiObjectNET + NiAVObject (BS version 130): name, transform, the offset after it."""
    name_i, = struct.unpack_from('<i', b, off)
    off += 4
    nextra, = struct.unpack_from('<I', b, off)
    off += 4 + 4 * nextra + 4          # extra data refs, controller
    off += 4                           # flags (uint32)
    t = np.array(struct.unpack_from('<3f', b, off))
    off += 12
    r = np.array(struct.unpack_from('<9f', b, off)).reshape(3, 3)
    off += 36
    s, = struct.unpack_from('<f', b, off)
    off += 4 + 4                       # scale, collision
    name = strings[name_i] if 0 <= name_i < len(strings) else ''
    return name, (r, t, s), off


NODES = ('NiNode', 'BSFadeNode', 'BSLeafAnimNode', 'BSOrderedNode', 'BSMultiBoundNode', 'BSValueNode',
         'NiBillboardNode', 'NiSwitchNode', 'NiLODNode', 'BSBlastNode', 'BSDamageStage', 'BSMasterParticleSystem',
         'BSTreeNode', 'NiSortAdjustNode', 'BSRangeNode', 'BSDebrisNode')


def compose(a, c):
    """(R, t, s) of a then c: x -> a(c(x)), with x' = R x s + t."""
    ra, ta, sa = a
    rc, tc, sc = c
    return ra @ rc, ra @ tc * sa + ta, sa * sc


def apply(xf, p):
    r, t, s = xf
    return r @ p * s + t


def billboard_cards(path):
    """[(pivot in model space, model scale of the node, disc center in the node frame, radius in that frame)]."""
    b, blocks, strings, _ = nif_read(path)
    info, parent = {}, {}
    for i, (t, off, size) in enumerate(blocks):
        if t in NODES or t == 'BSTriShape' or t == 'BSSubIndexTriShape' or t == 'BSMeshLODTriShape':
            name, xf, o = av_object(b, off, strings)
            rec = dict(type=t, name=name, xf=xf)
            if t in NODES:
                n, = struct.unpack_from('<I', b, o)
                for k in struct.unpack_from('<%di' % n, b, o + 4):
                    if k >= 0:
                        parent[k] = i
            else:
                rec['sphere'] = struct.unpack_from('<4f', b, o)
                nv, = struct.unpack_from('<H', b, o + 4 * 4 + 12 + 8 + 4)
                rec['nverts'] = nv
            info[i] = rec
    out = []
    for i, rec in info.items():
        if 'sphere' not in rec or rec.get('nverts', 0) == 0:
            continue
        chain, k, bb, marker = [i], parent.get(i), None, rec['name'].lower().startswith('editormarker')
        while k is not None:
            if info.get(k, {}).get('name', '').lower().startswith('editormarker'):
                marker = True
            if bb is None and info.get(k, {}).get('type') == 'NiBillboardNode':
                bb = k
            chain.append(k)
            k = parent.get(k)
        if marker or bb is None:
            continue
        # model transform of the billboard node, and the shape's transform inside it
        node_xf = (np.eye(3), np.zeros(3), 1.0)
        below = (np.eye(3), np.zeros(3), 1.0)
        seen_bb = False
        for k in reversed(chain):
            if not seen_bb:
                node_xf = compose(node_xf, info[k]['xf'])
                seen_bb = k == bb
            else:
                below = compose(below, info[k]['xf'])
        cx, cy, cz, rad = rec['sphere']
        center = apply(below, np.array([cx, cy, cz]))
        out.append((node_xf[1], node_xf[2], center, rad * below[2]))
    return out


def cards_of(esm, data, cell_edid):
    buf = open(esm, 'rb').read()
    cell_form, models, refs = None, {}, []
    start = {}      # lane MISS1: ref form -> (initially disabled, enable parent, opposite), in the order of refs

    def hidden(form, depth=0):
        # the game's start state: the enable parent's own state (its flag, or its parent's in turn), inverted
        # when "opposite"; a parent outside the cell counts as enabled, as in the cell view
        off, parent, opposite = start[form]
        if parent and depth < 16:
            off = (hidden(parent, depth + 1) if parent in start else False) == (not opposite)
        return off

    for t, form, off, stack in walk(buf):
        if t == b'CELL' and cell_form is None and all(g[2] != 1 for g in stack):
            _, f = record(buf, off)
            if f.get(b'EDID', b'').split(b'\0')[0].decode('cp1252', 'replace') == cell_edid:
                cell_form = form
        elif t == b'REFR':
            if cell_form is not None and any(g[1] == cell_form and g[2] in (6, 8, 9) for g in stack):
                refs.append(record(buf, off))
                x = refs[-1][1].get(b'XESP', b'')
                x = struct.unpack_from('<II', x) if len(x) >= 8 else (0, 0)
                start[form] = (bool(refs[-1][0] & 0x800), x[0], bool(x[1] & 1))
        elif t in (b'STAT', b'MSTT', b'ACTI', b'FURN', b'DOOR', b'MISC', b'CONT', b'FLOR', b'TERM', b'IDLM'):
            _, f = record(buf, off)
            m = f.get(b'MODL', b'').split(b'\0')[0].decode('cp1252', 'replace')
            if m:
                models[form] = m
    cache, cards, shapes = {}, [], 0
    for (flags, f), form in zip(refs, start):
        if flags & 0x20 or b'NAME' not in f or b'DATA' not in f:
            continue
        m = models.get(struct.unpack_from('<I', f[b'NAME'])[0])
        if not m:
            continue
        if m not in cache:
            p = os.path.join(data, 'Meshes', m.replace('\\', '/'))
            try:
                cache[m] = billboard_cards(p) if os.path.isfile(p) else []
            except (struct.error, IndexError, ValueError):
                cache[m] = []
        if not cache[m]:
            continue
        pos = np.array(struct.unpack_from('<3f', f[b'DATA'], 0))
        rot = struct.unpack_from('<3f', f[b'DATA'], 12)
        scale = struct.unpack_from('<f', f[b'XSCL'])[0] if b'XSCL' in f else 1.0
        R = euler(-rot[0], -rot[1], -rot[2])
        # a reference that starts disabled: the cell view does not draw these (counted apart, see K)
        opp = hidden(form)
        for pivot, nscale, center, rad in cache[m]:
            shapes += 1
            cards.append(dict(pivot=pos + R @ pivot * scale, ws=scale * nscale, center=center, rad=rad,
                              model=m, opposite=opp))
    return cards, shapes


def main():
    esm, data, cell, run, at = sys.argv[1:6]
    stages = (sys.argv[6] if len(sys.argv) > 6 and sys.argv[6] else 'KNC').upper()
    word = lambda st, good: ('PASS' if good else 'FAIL') if st in stages else 'n/a '
    flat = np.asarray(Image.open(f'{run}/flat.png').convert('RGB')).astype(np.int32)
    on = np.asarray(Image.open(f'{run}/on.png').convert('RGB')).astype(np.int32)
    H, W = on.shape[:2]
    cam = np.array([float(v) for v in re.search(r'cam=([-\d.]+),([-\d.]+),([-\d.]+)',
                                                 open(f'{run}/cam.txt').read()).groups()])
    look = np.array([float(v) for v in at.split(',')])
    fwd = look - cam
    fwd /= np.linalg.norm(fwd)
    right = np.cross(fwd, [0.0, 0.0, 1.0])
    right /= np.linalg.norm(right)
    up = np.cross(right, fwd)
    focal = (H / 2.0) / math.tan(math.radians(35.0))

    cards, shapes = cards_of(esm, data, cell)
    ok = True
    notes = open(f'{run}/on.notes', errors='replace').read()
    m = re.search(r'billboards: (\d+) shapes turned to the camera, (\d+) welded flat', notes)
    turned = int(m.group(1)) if m else -1
    opp = sum(c['opposite'] for c in cards)
    k_ok = turned == shapes - opp and shapes > opp
    print(f"K {'PASS' if k_ok else 'FAIL'}  census turned {turned} (welded flat {m.group(2) if m else '?'}), "
          f"this walk finds {shapes} billboard shapes on {len({c['model'] for c in cards})} models, "
          f"{opp} of them on refs that start disabled (not drawn by the cell view)")
    ok &= k_ok
    cards = [c for c in cards if not c['opposite']]

    yy, xx = np.mgrid[0:H, 0:W]
    near_mask = np.zeros((H, W), bool)
    circles = []
    for c in cards:
        v = c['pivot'] - cam
        px, py, depth = v @ right, v @ up, v @ fwd
        # the card's frame is the camera's: x right, y up, z toward the eye
        cx = px + c['center'][0] * c['ws']
        cy = py + c['center'][1] * c['ws']
        d = depth - c['center'][2] * c['ws']
        if d < 15.0:
            continue
        sx, sy = W / 2.0 + focal * cx / d, H / 2.0 - focal * cy / d
        sr = focal * c['rad'] * c['ws'] / d
        if sx + sr < 0 or sx - sr >= W or sy + sr < 0 or sy - sr >= H:
            continue
        circles.append((sx, sy, sr))
        near_mask |= (xx - sx) ** 2 + (yy - sy) ** 2 <= (sr * 1.15 + 4) ** 2
    print(f"cards on screen {len(circles)} of {len(cards)}")

    diff = np.abs(on - flat).max(axis=2)
    outside = ~near_mask
    n_out = int(outside.sum())
    if n_out < 1000:
        print(f"N skip  only {n_out} pixels outside the predicted cards")
    else:
        still = float((diff[outside] <= 3).mean())
        n_ok = still >= 0.995
        print(f"N {word('N', n_ok)}  {100.0 * still:.3f}% of {n_out} pixels outside the cards equal flat (>= 99.5%)")
        ok &= n_ok or 'N' not in stages
    if 'N' in stages and n_out < 1000:
        print("N FAIL  this shot is named for N and has no pixels to judge")
        ok = False

    signed = (on - flat).sum(axis=2) / 3.0
    lit, best, judged = 0, 0.0, 0
    for sx, sy, sr in circles:
        disc = (xx - sx) ** 2 + (yy - sy) ** 2 <= sr * sr
        if disc.sum() < 500:
            continue
        judged += 1
        mean, moved = float(signed[disc].mean()), float((diff[disc] > 0).mean())
        best = max(best, mean)
        if mean >= 0.5 and moved >= 0.30:
            lit += 1
    c_ok = lit >= 3
    print(f"C {word('C', c_ok)}  {lit} of {judged} cards brighten their circle by a mean >= 0.5/255 with "
          f">= 30% of it changed (>= 3; the brightest mean {best:+.2f}/255)")
    ok &= c_ok or 'C' not in stages
    print('glow PASS' if ok else 'glow FAIL')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
