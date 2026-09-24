"""Compare a `WW_CELL_DUMP` against an INDEPENDENT walk of the same plugin.

The dump was written by src/cellview.cpp through src/esmdata.cpp. This file
parses the ESM itself (tests/spells/cell_census.py, a different reader that
shares no code with either) and the NIFs itself, and checks three things:

  (1) THE SET, both ways round. A reference the walk expects and the dump does
      not have is MISSING; one the dump has and the walk does not is INVENTED.
      They are different failures and they are named differently. A missing
      reference is forgiven only when its model is not in the loose data tree,
      which the checker verifies rather than assumes.

  (2) THE POSITION of every non-collection reference, to 0.05 units, against the
      REFR's own DATA field. A collection's parts are placed relative to the
      reference, so for SCOL the PART COUNT is checked instead.

  (3) THE WORLD BOX, recomputed here from the model's own vertex bounds under
      the engine's euler convention -- Matrix::fromEuler(-x,-y,-z), the
      convention src/lodgen.cpp ~3468 measured against vanilla chunks. This is
      the only one of the three that depends on the ROTATION and the SCALE and
      not merely on the parse.

THE RED CONTROL. `--red` recomputes (3) under the WRONG convention (+x,+y,+z).
The checker must then find the boxes MOVED and NAME references. A run where
--red reports nothing is a run where the box comparison is not looking at
anything, and it says so in those words.

  python cell_open_check.py --esm E --data D --world W --cell X Y --n N --dump F
"""

import argparse
import array
import math
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, 'tools', 'rigging_prototype'))

import cell_census as cc
import cell_tri_budget as ctb

# Record header flags (FO4)
FLAG_DELETED = 0x00000020
FLAG_INITIALLY_DISABLED = 0x00000800

# BSTriShape vertex descriptor: the low nibble x 4 is the stride, and bit 54
# says the position is float32 rather than float16.
#
# Reading a half-float vertex as a float32 does not give a slightly wrong
# number, it gives 1e14 -- which is how this checker first "found" that all 74
# placements in a wilderness cell had moved. Nothing about the shape of that
# failure said "the reader is wrong"; the MAGNITUDE did. A disagreement of
# 3,986,846,973,645 units is not a rotation convention.
VF_FULL_PRECISION = 1 << 54

# Every base record type that can be placed as a REFR and carries a MODL.
# src/esmdata.cpp:495 reads MODL out of ANY base record, so the viewer is not
# limited to the ten types the brief named, and downtown 5,-11 holds nine pool
# balls (MISC) and a terminal (TERM) that the CK shows and this checker used to
# call INVENTED.
DRAWABLE_TYPES = cc.MODEL_TYPES | {
    b'MISC', b'TERM', b'ALCH', b'AMMO', b'ARMO', b'BOOK', b'WEAP', b'KEYM',
    b'NOTE', b'INGR', b'TACT', b'IDLM', b'GRAS', b'PROJ', b'HAZD', b'BNDS',
    b'ARTO', b'SCRL', b'SLGM', b'CMPO', b'NPC_',
}


def is_marker_model(model):
    """src/cellview.cpp isMarkerModel(), element for element.

    Markers are hidden by default (the brief), so the plugin listing one is not
    evidence that the scene dropped it.

    The `startswith('marker')` clause is the one lane CELLVIEW4B added on both
    sides: a marker at the meshes ROOT has no backslash before its name, so
    every other test here needs one and `markerxheading.nif`,
    `markercocheading.nif` and the `markers\\...` subtree were classed as
    ordinary statics. Do not trust this comment that the two rules still agree
    -- the accounting row below measures it, and that row is why a stale copy
    of this function can no longer pass quietly.
    """
    m = model.lower().replace('/', '\\')
    return ('\\marker' in m or 'marker_' in m
            or m.endswith('markerx.nif') or m.startswith('marker')
            or '\\editor\\' in m)


def euler_matrix(rx, ry, rz, negate=True):
    """Matrix::fromEuler(x,y,z), fed -x,-y,-z when negate (the engine's way)."""
    if negate:
        rx, ry, rz = -rx, -ry, -rz
    cx, sx = math.cos(rx), math.sin(rx)
    cy, sy = math.cos(ry), math.sin(ry)
    cz, sz = math.cos(rz), math.sin(rz)
    # NifSkope Matrix::fromEuler, written out so this file does not depend on it
    return [
        [cy * cz, -cy * sz, sy],
        [sx * sy * cz + sz * cx, cx * cz - sx * sy * sz, -sx * cy],
        [sx * sz - cx * sy * cz, cx * sy * sz + sx * cz, cx * cy],
    ]


# NiAVObject-derived node blocks: NiAVObject fields, then Num Children and the
# children array. Everything else that happens to end in "Node" is not one.
NODE_TYPES = {
    'NiNode', 'BSFadeNode', 'BSOrderedNode', 'BSMultiBoundNode',
    'NiBillboardNode', 'BSLeafAnimNode', 'BSValueNode', 'BSTreeNode',
    'NiSwitchNode', 'BSBlastNode', 'BSDamageStage', 'BSDebrisNode',
    'NiSortAdjustNode', 'BSRangeNode', 'BSMasterParticleSystem',
}


def read_hierarchy(data, blocks):
    """(transform per node block, parent of each block) for one NIF.

    A NIF's vertices are in the SHAPE's space, and the shape hangs under a
    chain of NiNodes each of which may rotate, move and scale it. The loader
    the viewer uses composes that chain; a checker that reads only the shape's
    own transform is off by the chain, which on this corpus is a 16-to-67 unit
    disagreement -- the same size and shape of error a wrong rotation
    convention produces. That is the whole reason this function exists: without
    it the red control cannot be told from a bug.

    Every NiNode subclass in the FO4 corpus (BSFadeNode, BSOrderedNode,
    BSMultiBoundNode, NiBillboardNode, BSLeafAnimNode, BSValueNode) adds its
    own fields AFTER the children array, so reading NiAVObject + numChildren +
    children is valid for all of them.

    The type is matched against a LIST, not against a name ending in "Node".
    BSFurnitureMarkerNode ends in Node and is an NiExtraData: parsed as a node
    it read a furniture heading of pi as a scale of -0.0, claimed the ROOT as
    its child, and collapsed every vertex of FederalistChairOffice01 onto one
    point -- one placement in downtown 5,-11 "off by 74.2 units".
    """
    xform = {}
    parent = {}
    for i, tname, start, size in blocks:
        if tname not in NODE_TYPES:
            continue
        try:
            o = start + 4                            # name
            ne = struct.unpack_from('<I', data, o)[0]
            o += 4 + 4 * ne                          # extra data list
            o += 8                                   # controller, flags
            t = list(struct.unpack_from('<fff', data, o))
            r = list(struct.unpack_from('<9f', data, o + 12))
            s = struct.unpack_from('<f', data, o + 48)[0]
            o += 52 + 4                              # transform, collision
            nc = struct.unpack_from('<i', data, o)[0]
            o += 4
            if nc < 0 or nc > 100000:
                continue
            kids = struct.unpack_from('<%di' % nc, data, o)
        except Exception:
            continue
        xform[i] = (t, r, s)
        for k in kids:
            # A child always follows its parent in a Bethesda NIF; anything
            # else is a misparse, and a misparse must not be believed.
            if k > i and k not in parent:
                parent[k] = i
    return xform, parent


def chain_of(block, xform, parent):
    """The transforms from a block's parent outwards, innermost first."""
    out = []
    seen = set()
    i = block
    while i in parent and i not in seen:
        seen.add(i)
        i = parent[i]
        if i in xform:
            out.append(xform[i])
    return out


def apply_chain(p3, chain):
    v = list(p3)
    for t, r, s in chain:
        v = [t[k] + s * (r[k * 3] * v[0] + r[k * 3 + 1] * v[1]
                         + r[k * 3 + 2] * v[2]) for k in range(3)]
    return v


def model_points(data_root, model, cache):
    """A model's shape vertices in MODEL space, flat xyz, or None.

    The POINTS and not their box. A box is what this checker used to keep, and
    rotating the eight corners of a model-space AABB gives the AABB OF THE
    ROTATED AABB, which is a strict superset of the AABB of the rotated
    vertices -- exact when the rotation is zero and larger by tens of units the
    moment it is not. That produced "73 of 74 world boxes disagree, and only the
    rotated ones", read for an hour as a wrong euler convention, when
    Matrix::fromEuler (src/data/niftypes.cpp:215) is element-for-element the
    same formula as euler_matrix above. The viewer takes min/max over its
    transformed VERTICES (src/cellview.cpp ~853), so the check has to as well.
    """
    key = model.lower()
    if key in cache:
        return cache[key]
    p = os.path.join(data_root, 'meshes', model.replace('\\', os.sep))
    out = None
    if os.path.isfile(p):
        try:
            import contextlib
            import io
            with contextlib.redirect_stdout(io.StringIO()):
                import nifparse
                data, hdr, strings, blocks = nifparse.parse(p)
            pts = array.array('f')
            xform, parent = read_hierarchy(data, blocks)

            def block_name(b):
                st = blocks[b][2] if b < len(blocks) and blocks[b][0] == b \
                    else next((s for j, t, s, z in blocks if j == b), None)
                if st is None:
                    return ''
                ix = struct.unpack_from('<i', data, st)[0]
                return strings[ix] if 0 <= ix < len(strings) else ''

            for i, tname, start, size in blocks:
                if tname not in ctb.SHAPES:
                    continue
                # EDITOR MARKERS ARE NOT THE OBJECT, and the loader the viewer
                # uses drops them (src/lodgen.cpp 2128-2143: the game hides
                # every node named EditorMarker*). Keeping them here made four
                # GlowingSeaMirageEffect01 placements "disagree by 85.5 units"
                # -- the size of a marker, not of a rotation.
                blk, marker = i, False
                for _ in range(64):
                    if block_name(blk).lower().startswith('editormarker'):
                        marker = True
                        break
                    if blk not in parent:
                        break
                    blk = parent[blk]
                if marker:
                    continue
                chain = chain_of(i, xform, parent)
                o = start + 4                       # name
                ne = struct.unpack_from('<I', data, o)[0]
                o += 4 + 4 * ne                     # extra data list
                o += 8                              # controller, flags
                # The shape's OWN transform. Skipping it cost this checker a
                # false 16-to-170 unit disagreement on 73 of 74 wilderness
                # placements: a NIF's vertices are in the shape's local space,
                # and the loader the viewer uses composes this before handing
                # the geometry over. A tens-of-units error is exactly what a
                # dropped local transform looks like, and exactly what a wrong
                # rotation convention looks like too -- which is why the red
                # control has to be run on a GREEN checker to mean anything.
                st = list(struct.unpack_from('<fff', data, o))
                sr = list(struct.unpack_from('<9f', data, o + 12))
                ss = struct.unpack_from('<f', data, o + 48)[0]
                o += 12 + 36 + 4                    # translation, rotation, scale
                o += 4                              # collision
                o += 16                             # bounding sphere
                o += 4                              # skin
                o += 8                              # shader, alpha
                vdesc = struct.unpack_from('<Q', data, o)[0]
                o += 8
                nv = struct.unpack_from('<H', data, o + 4)[0]
                o += 10                             # numTris, numVerts, dataSize
                stride = (vdesc & 0xF) * 4
                if stride < 6:
                    continue
                fmt = '<fff' if (vdesc & VF_FULL_PRECISION) else '<eee'
                for v in range(nv):
                    p3 = struct.unpack_from(fmt, data, o + v * stride)
                    q = [st[k] + ss * (sr[k * 3 + 0] * p3[0]
                                       + sr[k * 3 + 1] * p3[1]
                                       + sr[k * 3 + 2] * p3[2])
                         for k in range(3)]
                    q = apply_chain(q, chain)
                    pts.extend(q)
            if pts:
                out = pts
        except Exception as e:
            sys.stderr.write('  bounds unreadable %s: %s\n' % (model, e))
    cache[key] = out
    return out


def model_bounds(data_root, model, cache):
    """(lo, hi) of a model's shape vertices in model space, or None.

    Kept for the one-off diagnostics that print a model-space box next to the
    viewer's; the gate itself never uses it, because a box cannot be rotated.
    """
    pts = model_points(data_root, model, cache)
    if not pts:
        return None
    lo = [min(pts[k::3]) for k in range(3)]
    hi = [max(pts[k::3]) for k in range(3)]
    return (lo, hi)


def read_dump(path):
    rows = {}
    for line in open(path):
        if line.startswith('#') or not line.strip():
            continue
        f = line.rstrip('\n').split(' ')
        rows[(int(f[0], 16), int(f[3]))] = {
            'base': int(f[1], 16), 'type': f[2], 'part': int(f[3]),
            'pos': [float(f[6]), float(f[7]), float(f[8])],
            'rot': [float(f[9]), float(f[10]), float(f[11])],
            'scale': float(f[12]), 'tris': int(f[13]),
            'bmin': [float(f[14]), float(f[15]), float(f[16])],
            'bmax': [float(f[17]), float(f[18]), float(f[19])],
            'model': ' '.join(f[20:]),
        }
    return rows


def scol_part_rotations(esm):
    """SCOL form -> [(part base, (rx,ry,rz)) ...] in PLACEMENT order.

    A collection's part is rotated TWICE: once by the reference (rm) and once
    by the part's own placement inside the collection (pm), and the scene uses
    rm*pm (src/cellview.cpp:622). The dump can only carry one euler triple and
    carries the REFERENCE's, so the second half has to be read back out of the
    plugin here or every SCOL part's box is checked against the wrong rotation
    -- which is what "112 of 240 boxes disagree, all of them SCOL parts" was.
    """
    out = {}
    buf = esm.buf

    def cb(rec, path):
        if rec is None or rec.type != b'SCOL':
            return
        if rec.flags & 0x00040000:
            try:
                data = cc.decompress(buf, rec)
            except Exception:
                return
            it = cc.fields(data, cc.Rec(rec.type, len(data), rec.flags, rec.form, 0))
        else:
            it = cc.fields(buf, rec)
        rows = []
        cur = None
        for t, p in it:
            if t == b'ONAM' and len(p) >= 4:
                cur = struct.unpack_from('<I', p, 0)[0]
            elif t == b'DATA' and cur is not None:
                # 7 floats per placement: pos xyz, rot xyz, scale
                for o in range(0, len(p) - 27, 28):
                    rx, ry, rz = struct.unpack_from('<fff', p, o + 12)
                    rows.append((cur, (rx, ry, rz)))
                cur = None
        out[rec.form] = rows

    esm.walk(24 + struct.unpack_from('<I', buf, 4)[0], len(buf), cb)
    return out


def persistent_cell_of(esm, wform):
    """The worldspace's PERSISTENT cell, the one whose parent GRUP is the

    world-children group itself rather than a block or a subblock
    (src/esmdata.cpp:141-145). Its references have no grid of their own and the
    viewer files them by POSITION (src/cellview.cpp:674-687), so a checker that
    only looks at the block's own cells calls every one of them INVENTED -- as
    this one did for Vault_ChairFolding02 in Sanctuary.
    """
    buf = esm.buf
    found = [None]

    def cb(rec, path):
        if rec is None or rec.type != b'CELL' or not path:
            return
        label, gtype, _ = path[-1]
        if gtype == 1 and struct.unpack_from('<I', label, 0)[0] == wform:
            found[0] = rec.form

    esm.walk(24 + struct.unpack_from('<I', buf, 4)[0], len(buf), cb)
    return found[0]


def expected(esm, bases, scols, cellxy, persistent, cx, cy, n, pcell=None):
    """What the plugin says should be in the block: refForm -> record."""
    half = n // 2
    xy2form = {xy: f for f, xy in cellxy.items()}
    forms = set()
    for y in range(cy - half, cy + half + 1):
        for x in range(cx - half, cx + half + 1):
            cf = xy2form.get((x, y))
            if cf is not None:
                forms.add(cf)
    # the block's world extent, for the persistent cell's own references
    box = (float(cx - half) * 4096.0, float(cy - half) * 4096.0,
           float(cx + half + 1) * 4096.0, float(cy + half + 1) * 4096.0)
    want = {}
    for cellform, rec in persistent:
        from_pcell = pcell is not None and cellform == pcell
        if cellform not in forms and not from_pcell:
            continue
        if rec.flags & (FLAG_DELETED | FLAG_INITIALLY_DISABLED):
            continue            # the viewer hides both by default
        base = None
        pos = None
        for t, p in cc.refr_fields(esm, rec):
            if t == b'NAME' and len(p) >= 4:
                base = struct.unpack_from('<I', p, 0)[0]
            elif t == b'DATA' and len(p) >= 24:
                pos = list(struct.unpack_from('<fff', p, 0))
        if base is None or pos is None:
            continue
        if from_pcell and not (box[0] <= pos[0] < box[2]
                               and box[1] <= pos[1] < box[3]):
            continue
        b = bases.get(base)
        if b is None:
            continue
        btype, modl, edid = b
        if btype == b'SCOL':
            parts = 0
            models = []
            for pb, pn in scols.get(base, ()):
                pbb = bases.get(pb)
                if pbb and pbb[1]:
                    parts += pn
                    models.extend([pbb[1]] * pn)
            if parts:
                want[rec.form] = {'type': 'SCOL', 'base': base, 'pos': pos,
                                  'parts': parts, 'models': models, 'edid': edid}
        elif modl:
            want[rec.form] = {'type': btype.decode(), 'base': base, 'pos': pos,
                              'parts': 1, 'models': [modl], 'edid': edid}
    return want, len(forms)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--esm', required=True)
    ap.add_argument('--data', required=True)
    ap.add_argument('--world', required=True)
    ap.add_argument('--cell', nargs=2, type=int, required=True)
    ap.add_argument('--n', type=int, default=1)
    ap.add_argument('--dump', required=True)
    # the viewer's own census, for the accounting row (lane CELLVIEW4B)
    ap.add_argument('--notes')
    ap.add_argument('--red', action='store_true')
    a = ap.parse_args()

    dump = read_dump(a.dump)
    byref = {}
    for (ref, part), row in dump.items():
        byref.setdefault(ref, []).append(row)

    esm = cc.Esm(a.esm)
    bases, scols = cc.index_bases(esm, DRAWABLE_TYPES)
    wform, cellxy, persistent = cc.cells_of_world(esm, a.world)
    pcell = persistent_cell_of(esm, wform)
    want, cells = expected(esm, bases, scols, cellxy, persistent,
                           a.cell[0], a.cell[1], a.n, pcell)
    print('dump: %d placements over %d references' % (len(dump), len(byref)))
    print('independent walk: world %08X, %d of %d cells present, %d drawable '
          'references' % (wform, cells, a.n * a.n, len(want)))

    cache = {}
    problems = []
    moved = []
    partrot = scol_part_rotations(esm)

    def scol_rot(ref, part, model):
        """The part's own euler inside the collection, or None.

        Matched by MODEL NAME at the part index, not by index alone: esmdata
        and this walk both number parts in file order, and the check says so
        out loud instead of trusting it.
        """
        w = want.get(ref)
        if not w or w.get('type') != 'SCOL':
            return None
        rows = partrot.get(w['base'])
        if not rows:
            return None
        kept = [(pb, r) for pb, r in rows
                if bases.get(pb) and bases[pb][1]]
        for lst in (kept, rows):
            if 0 <= part < len(lst):
                pb, r = lst[part]
                bb = bases.get(pb)
                if bb and bb[1].lower() == model.lower():
                    return r
        return False            # a part index that does not line up

    # ---------------------------------------------------- (3) the world boxes
    scol_unmatched = 0
    for (ref, part), row in sorted(dump.items()):
        pts = model_points(a.data, row['model'], cache)
        if not pts:
            continue
        m = euler_matrix(row['rot'][0], row['rot'][1], row['rot'][2],
                         negate=not a.red)
        if part >= 0:
            pr = scol_rot(ref, part, row['model'])
            if pr is False:
                scol_unmatched += 1
                continue
            if pr is not None:
                pm = euler_matrix(pr[0], pr[1], pr[2], negate=not a.red)
                m = [[sum(m[i][k] * pm[k][j] for k in range(3))
                      for j in range(3)] for i in range(3)]
        # wp = pos + rot * (v * scale), vertex by vertex, exactly as
        # src/cellview.cpp ~853 does it, and then the min/max of THAT.
        s = row['scale']
        r0, r1, r2 = m[0], m[1], m[2]
        a0, a1, a2 = r0[0] * s, r0[1] * s, r0[2] * s
        b0, b1, b2 = r1[0] * s, r1[1] * s, r1[2] * s
        c0, c1, c2 = r2[0] * s, r2[1] * s, r2[2] * s
        lo = [3e38] * 3
        hi = [-3e38] * 3
        for i in range(0, len(pts), 3):
            x = pts[i]; y = pts[i + 1]; z = pts[i + 2]
            wx = a0 * x + a1 * y + a2 * z
            wy = b0 * x + b1 * y + b2 * z
            wz = c0 * x + c1 * y + c2 * z
            if wx < lo[0]: lo[0] = wx
            if wx > hi[0]: hi[0] = wx
            if wy < lo[1]: lo[1] = wy
            if wy > hi[1]: hi[1] = wy
            if wz < lo[2]: lo[2] = wz
            if wz > hi[2]: hi[2] = wz
        for k in range(3):
            lo[k] += row['pos'][k]
            hi[k] += row['pos'][k]
        err = max(max(abs(lo[k] - row['bmin'][k]) for k in range(3)),
                  max(abs(hi[k] - row['bmax'][k]) for k in range(3)))
        if err > 2.0:
            moved.append((ref, part, err, row['model']))
    checked = sum(1 for k in dump if cache.get(dump[k]['model'].lower()))
    if scol_unmatched:
        print('  NOTE     %d collection parts could not be lined up with the '
              'plugin by model name; their boxes were NOT checked'
              % scol_unmatched)

    if a.red:
        if moved:
            print('RED CONTROL OK: %d of %d boxes moved under the wrong euler '
                  'convention; first five:' % (len(moved), checked))
            for ref, part, err, model in moved[:5]:
                print('  0x%08X part %d  off by %.1f units  %s'
                      % (ref, part, err, model))
            return 0
        print('RED CONTROL FAILED: the wrong euler convention moved NOTHING in '
              '%d boxes. The box comparison is not measuring the transform.'
              % checked)
        return 1

    if moved:
        problems.append('%d of %d world boxes disagree with the engine euler '
                        'convention' % (len(moved), checked))
        for ref, part, err, model in moved[:10]:
            print('  BOX      0x%08X part %d  off by %.1f units  %s'
                  % (ref, part, err, model))

    # ---------------------------------------------------- (1) and (2) the set
    invented = sorted(set(byref) - set(want))
    missing = sorted(set(want) - set(byref))
    for ref in invented[:10]:
        r = byref[ref][0]
        print('  INVENTED 0x%08X %s %s -- in the scene, not in the plugin'
              % (ref, r['type'], r['model']))
    if invented:
        problems.append('%d references in the scene that the plugin does not '
                        'put in this block' % len(invented))

    unexplained = []
    for ref in missing:
        w = want[ref]
        # A reference is legitimately absent when the scene had nothing to
        # draw for it. Three ways that happens, all VERIFIED here rather than
        # assumed: the model is not in the loose tree at all; it is a MARKER
        # and markers are hidden by default; or it parses but yields no
        # geometry, which is what every StaticCollectionPivotDummy and
        # InvisibleGeneric01 does -- their only shapes sit under an
        # EditorMarker node that the loader drops.
        loose = []
        for m in w['models']:
            if not os.path.isfile(os.path.join(a.data, 'meshes',
                                               m.replace('\\', os.sep))):
                continue
            if is_marker_model(m):
                continue
            if not model_points(a.data, m, cache):
                continue
            loose.append(m)
        if loose:
            unexplained.append((ref, w, loose[0]))
    for ref, w, m in unexplained[:10]:
        print('  MISSING  0x%08X %s %s -- the model IS in the data tree: %s'
              % (ref, w['type'], w['edid'] or '-', m))
    if unexplained:
        problems.append('%d references dropped although their model is present '
                        '(%d more were dropped with no loose model, which is '
                        'expected)'
                        % (len(unexplained), len(missing) - len(unexplained)))
    elif missing:
        print('  %d references absent, every one with no loose model (BA2-only)'
              ' -- not counted against the scene' % len(missing))

    # THE ACCOUNTING IDENTITY (lane CELLVIEW4B). The row above explains one
    # reference at a time and its last excuse -- "no loose model" -- is open
    # ended: a reference that vanished for a reason nobody has thought of also
    # has no loose model some of the time. The viewer publishes a census of
    # every reference it deliberately did not draw, by NAMED category:
    #
    #   hidden: disabled 0, markers 12, deleted 0, no base 0
    #
    # so the honest question is whether those named categories account for the
    # whole gap, exactly. They did on 2026-09-19 for downtown 5,-11: plugin
    # 1438 drawable, scene 1426, gap 12, census 12. A reference dropped for an
    # unnamed reason moves the two apart and this fails; widening or narrowing
    # the marker rule moves a reference between two NAMED categories and leaves
    # the sum alone, so this row cannot be satisfied by editing the rule.
    if getattr(a, 'notes', None) and os.path.isfile(a.notes):
        with open(a.notes, 'r', errors='replace') as fh:
            hid = re.search(r'hidden: disabled ([0-9]+), markers ([0-9]+), '
                            r'deleted ([0-9]+), no base ([0-9]+)', fh.read())
        if not hid:
            problems.append('the scene wrote no "hidden:" census line, so the '
                            'references it did not draw are unaccounted for')
        else:
            named = sum(int(g) for g in hid.groups())
            print('  accounting: %d references the plugin draws are not in the '
                  'scene; the census names %d (disabled %s, markers %s, '
                  'deleted %s, no base %s)'
                  % ((len(missing), named) + tuple(hid.groups())))
            if named != len(missing):
                problems.append('%d references are missing from the scene but '
                                'the census names only %d -- %d dropped for a '
                                'reason the viewer does not state'
                                % (len(missing), named, len(missing) - named))

    badpos = []
    badparts = []
    for ref in sorted(set(byref) & set(want)):
        w = want[ref]
        rows = sorted(byref[ref], key=lambda r: r['part'])
        if w['type'] == 'SCOL':
            # A collection's rows carry the PART's base and record type, not the
            # collection's -- that is the expansion working. More parts than the
            # plugin has is the failure worth naming; fewer is a missing model.
            if len(rows) > w['parts']:
                badparts.append((ref, len(rows), w['parts'], w['edid']))
            continue
        d = max(abs(rows[0]['pos'][k] - w['pos'][k]) for k in range(3))
        if d > 0.05:
            badpos.append((ref, d, w['edid']))
    for ref, d, edid in badpos[:10]:
        print('  POSITION 0x%08X %s -- off by %.3f units' % (ref, edid or '-', d))
    if badpos:
        problems.append('%d references at a position the plugin does not give '
                        'them' % len(badpos))
    for ref, got, exp, edid in badparts[:10]:
        print('  PARTS    0x%08X %s -- %d collection parts drawn, plugin has %d'
              % (ref, edid or '-', got, exp))
    if badparts:
        problems.append('%d collections with more parts than the plugin has'
                        % len(badparts))

    if problems:
        for p in problems:
            print('FAIL: %s' % p)
        return 1
    print('OK: %d placements, %d references, %d boxes recomputed and matched'
          % (len(dump), len(byref), checked))
    return 0


if __name__ == '__main__':
    sys.exit(main())
