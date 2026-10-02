#!/usr/bin/env python
"""Is every placed reference the game shows also drawn by the cell view, and where the game puts it?

Lane MISS1, 2026-10-02. The independent half of tests/spells/cell_refs.sh.

It walks the plugin ITSELF (no NifSkope code): the cell's references, their base
records, the enable-parent chain, the models on disk, and the cell's combined
meshes in the game's own archive. From that it says which references the game
shows when the cell first loads and where each one's box is, and compares with
what the viewer wrote (WW_CELL_DUMP = what was drawn, WW_CELL_REFDUMP = one line
per reference with the viewer's reason).

Rows, per cell:
  census  the plugin's reference count equals the viewer's reference list
  drawn   every reference the game shows is in the draw dump
  hidden  nothing is in the draw dump that the game hides
  placed  each drawn reference's world box equals the box rebuilt here
  anchor  the game's own combined meshes agree with the placement rule used here

The rules, and where each comes from:
  * a reference starts enabled when it has no enable parent and is not flagged
    initially disabled; with an enable parent it is enabled when the parent's
    start state differs from the "opposite" bit (the parent's own state follows
    the same rule, up its chain).
  * a base flagged as a marker (record flag 0x00800000), or whose model has no
    triangles outside EditorMarker* nodes, is not shown.
  * a placed model's ROOT node transform is not applied: the root takes the
    reference's transform. The "anchor" row proves it per cell from the combined
    meshes: each combined instance stores the reference transform and a world
    bound, and the bound only fits with the root dropped.

A reference this checker cannot judge is counted and NAMED, never passed silently.

usage: cell_refs_drawn_check.py <Fallout4.esm> <data root> <out dir> <cell> [<cell> ...]
       [--bar <units>] [--no-anchor]
       <out dir> holds <cell>.dump and <cell>.refdump from the viewer.
"""
import contextlib
import glob
import io
import os
import struct
import sys
import zlib

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tools', 'rigging_prototype'))
import nifparse                      # noqa: E402  the standalone NIF block splitter
import cell_open_check as coc        # noqa: E402  euler_matrix + read_hierarchy only

BS = chr(92)
SHAPES = ('BSTriShape', 'BSSubIndexTriShape', 'BSMeshLODTriShape', 'BSDynamicTriShape')
REF_TYPES = (b'REFR', b'ACHR', b'PGRE', b'PMIS', b'PHZD', b'PARW', b'PBAR', b'PBEA', b'PCON', b'PFLA')
SKIP_TOP = (b'DIAL', b'QUST', b'NAVI', b'INFO', b'SCEN', b'PACK')
FLAG_DELETED = 0x20
FLAG_DISABLED = 0x800
FLAG_MARKER = 0x00800000
MARKER_TYPES = ('ACTI', 'DOOR', 'FURN', 'STAT')      # the record types whose bit 23 means "is a marker"
U32 = struct.Struct('<I')


def fields(data):
    off, end, big, out = 0, len(data), 0, []
    while off + 6 <= end:
        t = data[off:off + 4]
        sz = struct.unpack_from('<H', data, off + 4)[0]
        off += 6
        if t == b'XXXX':
            big = U32.unpack_from(data, off)[0]
            off += sz
            continue
        if big:
            sz, big = big, 0
        out.append((t, data[off:off + sz]))
        off += sz
    return out


def zstr(p):
    return p.split(b'\0')[0].decode('cp1252', 'replace')


def norm_model(m):
    m = m.replace('/', BS).lower().strip()
    if m.startswith('meshes' + BS):
        m = m[7:]
    return m


class Plugin:
    """One walk of the plugin: record headers only, fields read on demand."""

    def __init__(self, path, wanted):
        self.buf = buf = open(path, 'rb').read()
        self.base = {}      # form -> offset, every record outside the cell groups
        self.refs = {}      # form -> offset, every placed reference
        self.cells = {}     # editor id -> (form, offset)
        self.kids = {}      # cell form -> [reference offsets]
        self._want = set(w.lower() for w in wanted)
        self._en = {}
        self._bi = {}
        off = 24 + U32.unpack_from(buf, 4)[0]
        end = len(buf)
        while off + 24 <= end:
            size = U32.unpack_from(buf, off + 4)[0]
            label = buf[off + 8:off + 12]
            if label in (b'CELL', b'WRLD'):
                self._cells(off + 24, off + size, label == b'CELL', None)
            elif label not in SKIP_TOP:
                self._flat(off + 24, off + size)
            off += size

    def _flat(self, off, end):
        buf, base, up = self.buf, self.base, U32.unpack_from
        while off + 24 <= end:
            size = up(buf, off + 4)[0]
            if buf[off:off + 4] == b'GRUP':
                self._flat(off + 24, off + size)
                off += size
            else:
                base[up(buf, off + 12)[0]] = off
                off += 24 + size

    def _cells(self, off, end, interior, cur):
        buf, up = self.buf, U32.unpack_from
        while off + 24 <= end:
            t = buf[off:off + 4]
            size = up(buf, off + 4)[0]
            if t == b'GRUP':
                gtype = up(buf, off + 12)[0]
                ncur = cur
                if gtype in (6, 8, 9):
                    lab = up(buf, off + 8)[0]
                    ncur = lab if lab in self.kids else None
                self._cells(off + 24, off + size, interior, ncur)
                off += size
                continue
            form = up(buf, off + 12)[0]
            if t == b'CELL':
                if interior:
                    ed = ''
                    for ft, p in self.fields(off):
                        if ft == b'EDID':
                            ed = zstr(p)
                            break
                    if ed.lower() in self._want:
                        self.cells[ed.lower()] = (form, off)
                        self.kids[form] = []
            elif t in REF_TYPES:
                self.refs[form] = off
                if cur is not None:
                    self.kids[cur].append(off)
            off += 24 + size

    def fields(self, off):
        buf = self.buf
        size, flags = struct.unpack_from('<II', buf, off + 4)
        raw = buf[off + 24:off + 24 + size]
        if flags & 0x00040000:
            raw = zlib.decompress(raw[4:])
        return fields(raw)

    def header(self, off):
        """(type, flags, form)"""
        buf = self.buf
        return buf[off:off + 4].decode('latin1'), U32.unpack_from(buf, off + 8)[0], U32.unpack_from(buf, off + 12)[0]

    def xesp(self, off):
        for t, p in self.fields(off):
            if t == b'XESP' and len(p) >= 8:
                return struct.unpack_from('<II', p)
        return None

    def enabled(self, form, depth=0):
        """The game's start state of a reference, or None when the form is not a placed reference."""
        if form in self._en:
            return self._en[form]
        off = self.refs.get(form)
        if off is None:
            return None
        res = not (self.header(off)[1] & FLAG_DISABLED)
        x = self.xesp(off)
        if x is not None and depth < 16:
            pe = self.enabled(x[0], depth + 1)
            if pe is not None:
                res = pe != bool(x[1] & 1)
        self._en[form] = res
        return res

    def baseinfo(self, form):
        if form in self._bi:
            return self._bi[form]
        off = self.base.get(form)
        out = None
        if off is not None:
            t, flags, _ = self.header(off)
            edid, model, female, parts = '', '', '', []
            for ft, p in self.fields(off):
                if ft == b'EDID':
                    edid = zstr(p)
                elif t == 'ARMO':
                    # a piece of clothing lies in the world as MOD2 (MOD4 the female one); its MODL is a form id
                    if ft == b'MOD2' and not model:
                        model = zstr(p)
                    elif ft == b'MOD4' and not female:
                        female = zstr(p)
                elif ft == b'MODL' and not model:
                    model = zstr(p)
                elif ft == b'ONAM' and t == 'SCOL' and len(p) >= 4:
                    parts.append(U32.unpack_from(p)[0])
            out = dict(type=t, flags=flags, edid=edid, model=norm_model(model or female), parts=parts)
        self._bi[form] = out
        return out


def xf_apply(pts, xf):
    t, r, s = xf
    return pts @ np.array(r, dtype=np.float64).reshape(3, 3).T * s + np.array(t, dtype=np.float64)


def xf_identity(xf):
    t, r, s = xf
    return (max(abs(v) for v in t) < 1e-4 and abs(s - 1) < 1e-5
            and all(abs(r[k] - (1 if k % 4 == 0 else 0)) < 1e-5 for k in range(9)))


def model_info(data_root, model, cache):
    """None when the file is missing, else the model's visible geometry in model space, with the root
    node's transform dropped ('drop', what the game shows for a placed model) and kept ('keep')."""
    if model in cache:
        return cache[model]
    path = os.path.join(data_root, 'meshes', *model.split(BS))
    info = None
    if os.path.isfile(path):
        info = dict(drop=None, keep=None, tris=0, shapes=[], rootid=True, error='')
        try:
            with contextlib.redirect_stdout(io.StringIO()):
                data, hdr, strings, blocks = nifparse.parse(path)
            xform, parent = coc.read_hierarchy(data, blocks)
            start_of = {i: s for i, t, s, z in blocks}

            def name(b):
                ix = struct.unpack_from('<i', data, start_of[b])[0]
                return strings[ix] if 0 <= ix < len(strings) else ''

            drop, keep = [], []
            for i, tname, start, size in blocks:
                if tname not in SHAPES:
                    continue
                blk, marker = i, False
                for _ in range(64):
                    if name(blk).lower().startswith('editormarker'):
                        marker = True
                        break
                    if blk not in parent:
                        break
                    blk = parent[blk]
                if marker:
                    continue
                o = start + 4
                ne = U32.unpack_from(data, o)[0]
                o += 4 + 4 * ne + 8
                own = (list(struct.unpack_from('<3f', data, o)), list(struct.unpack_from('<9f', data, o + 12)),
                       struct.unpack_from('<f', data, o + 48)[0])
                o += 52 + 4
                centre = np.array(struct.unpack_from('<3f', data, o), dtype=np.float64)
                o += 16 + 4 + 8
                vdesc = struct.unpack_from('<Q', data, o)[0]
                o += 8
                nt, nv, dsz = struct.unpack_from('<IHI', data, o)
                o += 10
                stride = (vdesc & 0xF) * 4
                if nv == 0 or dsz == 0 or stride < 6:
                    continue
                full = bool(vdesc & coc.VF_FULL_PRECISION)
                pts = np.ndarray((nv, 3), dtype='<f4' if full else '<f2', buffer=data, offset=o,
                                 strides=(stride, 4 if full else 2)).astype(np.float64)
                # the chain from the shape outwards; the last link is the root (the block with no parent)
                chain = [(own, i not in parent)]
                at, seen = i, set()
                while at in parent and at not in seen:
                    seen.add(at)
                    at = parent[at]
                    if at in xform:
                        chain.append((xform[at], at not in parent))
                pd, pk, cd, ck = pts, pts, centre[None, :], centre[None, :]
                for xf, isroot in chain:
                    pk, ck = xf_apply(pk, xf), xf_apply(ck, xf)
                    if isroot:
                        if not xf_identity(xf):
                            info['rootid'] = False
                    else:
                        pd, cd = xf_apply(pd, xf), xf_apply(cd, xf)
                drop.append(pd)
                keep.append(pk)
                info['tris'] += nt
                info['shapes'].append((i, nv, cd[0], ck[0]))
            if drop:
                info['drop'] = np.vstack(drop)
                info['keep'] = np.vstack(keep)
        except Exception as e:                       # a model this reader cannot read is named, not guessed
            info = dict(drop=None, keep=None, tris=0, shapes=[], rootid=True, error=str(e))
    cache[model] = info
    return info


def world(pts, pos, R, scale):
    return pts @ R.T * scale + pos


def read_dump(path):
    rows = {}
    for line in open(path):
        if line.startswith('#') or not line.strip():
            continue
        f = line.rstrip('\n').split(' ')
        rows.setdefault(int(f[0], 16), []).append(dict(
            type=f[2], part=int(f[3]), tris=int(f[13]),
            lo=np.array([float(v) for v in f[14:17]]), hi=np.array([float(v) for v in f[17:20]])))
    return rows


def read_refdump(path):
    rows = {}
    for line in open(path):
        if line.startswith('#') or not line.strip():
            continue
        f = line.rstrip('\n').split(' ')
        rows[int(f[0], 16)] = f[5]
    return rows


def combined_instances(esm, cell_form, work):
    """The cell's combined meshes out of the game's archive: [(translation, bound centre, verts)] or None."""
    ba2 = os.path.join(os.path.dirname(esm), 'Fallout4 - MeshesExtra.ba2')
    if not os.path.isfile(ba2):
        return None
    os.makedirs(work, exist_ok=True)
    prefix = ('meshes' + BS + 'precombined' + BS + '%08x_' % cell_form).lower()
    with open(ba2, 'rb') as f:
        magic, ver, typ, n, nto = struct.unpack('<4sI4sIQ', f.read(24))
        if magic != b'BTDX' or typ != b'GNRL':
            return None
        recs = [struct.unpack('<I4sIIQIII', f.read(36)) for _ in range(n)]
        f.seek(nto)
        names = []
        for _ in range(n):
            ln = struct.unpack('<H', f.read(2))[0]
            names.append(f.read(ln).decode('cp1252').lower())
        for nm, rec in zip(names, recs):
            if nm.startswith(prefix) and nm.endswith('_oc.nif'):
                out = os.path.join(work, os.path.basename(nm.replace(BS, '/')))
                if not os.path.isfile(out):
                    f.seek(rec[4])
                    raw = zlib.decompress(f.read(rec[5])) if rec[5] else f.read(rec[6])
                    with open(out, 'wb') as o:
                        o.write(raw)
    ins = []
    for p in sorted(glob.glob(os.path.join(work, '%08x_*_oc.nif' % cell_form))):
        with contextlib.redirect_stdout(io.StringIO()):
            data, hdr, strings, blocks = nifparse.parse(p)
        for i, t, start, size in blocks:
            if t not in ('BSPackedCombinedSharedGeomDataExtra', 'BSPackedCombinedGeomDataExtra'):
                continue
            o = start + 4 + 8
            nvt, ntt, f1, f2, nd = struct.unpack_from('<5I', data, o)
            o += 20
            if t == 'BSPackedCombinedSharedGeomDataExtra':
                o += 8 * nd
            for k in range(nd):
                nv, lods, c0, o0, c1, o1, c2, o2, nc = struct.unpack_from('<9I', data, o)
                o += 36
                for c in range(nc):
                    tr = struct.unpack_from('<3f', data, o + 40)
                    bound = struct.unpack_from('<3f', data, o + 56)
                    ins.append((tr, np.array(bound, dtype=np.float64), nv))
                    o += 72
                o += 8
                if t == 'BSPackedCombinedGeomDataExtra':
                    vdesc = struct.unpack_from('<Q', data, o - 8)[0]
                    o += nv * ((vdesc & 0xF) * 4) + (c0 + c1 + c2) * 6
    return ins


def check_cell(esm, plug, data_root, out_dir, cell, bar, anchor, mcache):
    """Prints the cell's rows; returns (rows, failures)."""
    rows = fails = 0

    def row(tag, ok, text):
        nonlocal rows, fails
        rows += 1
        if not ok:
            fails += 1
        print('  %s  [%s] %s: %s' % ('PASS' if ok else 'FAIL', tag, cell, text))

    ent = plug.cells.get(cell.lower())
    dump_path = os.path.join(out_dir, cell + '.dump')
    ref_path = os.path.join(out_dir, cell + '.refdump')
    if ent is None or not os.path.isfile(dump_path) or not os.path.isfile(ref_path):
        row('census', False, 'no such interior in the plugin' if ent is None else 'the viewer wrote no dumps')
        return rows, fails
    cell_form, cell_off = ent
    dump = read_dump(dump_path)
    refdump = read_refdump(ref_path)

    # the cell's own list of references the game drew into combined meshes (proof that the game shows them)
    combined_refs = set()
    for ft, p in plug.fields(cell_off):
        if ft == b'XCRI' and len(p) >= 8:
            nmesh = U32.unpack_from(p)[0]
            o = 8 + 4 * nmesh
            while o + 8 <= len(p):
                combined_refs.add(U32.unpack_from(p, o)[0])
                o += 8

    refs = []
    for off in plug.kids[cell_form]:
        t, flags, form = plug.header(off)
        if t == 'REFR':
            refs.append((form, off, flags))
    mine = set(f for f, _, _ in refs)
    theirs = set(refdump)
    row('census', mine == theirs,
        'references in the plugin %d, in the viewer\'s list %d, only here %d, only there %d'
        % (len(mine), len(theirs), len(mine - theirs), len(theirs - mine)))

    expect = {}          # ref form -> (base info, pos, rot, scale)
    why_hidden = {}      # ref form -> reason the game does not show it
    unjudged = {}        # reason -> [ref form]
    parented = flipped = 0
    for form, off, flags in refs:
        fl = dict(plug.fields(off))
        if flags & FLAG_DELETED:
            why_hidden[form] = 'deleted'
            continue
        if b'NAME' not in fl or b'DATA' not in fl or len(fl[b'DATA']) < 24:
            why_hidden[form] = 'no base'
            continue
        base = U32.unpack_from(fl[b'NAME'])[0]
        b = plug.baseinfo(base)
        x = plug.xesp(off)
        if x is not None:
            parented += 1
            old = not (flags & FLAG_DISABLED)
            if x[1] & 1:
                old = not old
            if old != plug.enabled(form):
                flipped += 1
        if b is None:
            why_hidden[form] = 'no base'
            continue
        if not plug.enabled(form):
            why_hidden[form] = 'disabled at start'
            continue
        models = [b['model']] if b['model'] else []
        if b['type'] == 'SCOL':
            models = [pb['model'] for pb in (plug.baseinfo(p) for p in b['parts']) if pb and pb['model']]
        if not models:
            why_hidden[form] = 'no model on the base'
            continue
        if b['flags'] & FLAG_MARKER and b['type'] in MARKER_TYPES:
            why_hidden[form] = 'marker (base flag)'
            continue
        infos = [model_info(data_root, m, mcache) for m in models]
        if all(i is None for i in infos):
            unjudged.setdefault('model file not on disk', []).append(form)
            continue
        if any(i is not None and i['error'] for i in infos):
            unjudged.setdefault('model unreadable here', []).append(form)
            continue
        if not any(i is not None and i['drop'] is not None for i in infos):
            why_hidden[form] = 'no triangles outside editor markers'
            continue
        if b['model'].startswith('markers' + BS) and form not in combined_refs:
            # a model from the editor's marker folder that still has real triangles and no marker flag:
            # only the game's own combined list can say it is shown, and this one is not in it
            unjudged.setdefault('marker-folder model with triangles, not in the combined list', []).append(form)
            continue
        pos = np.array(struct.unpack_from('<3f', fl[b'DATA'], 0), dtype=np.float64)
        rot = struct.unpack_from('<3f', fl[b'DATA'], 12)
        scale = struct.unpack('<f', fl[b'XSCL'][:4])[0] if b'XSCL' in fl and len(fl[b'XSCL']) >= 4 else 1.0
        expect[form] = (b, pos, rot, scale)

    drawn = set(dump)
    skip = set(f for v in unjudged.values() for f in v)
    missing = sorted(set(expect) - drawn)
    extra = sorted(drawn - set(expect) - skip)

    def names(forms, reason):
        out = []
        for f in forms[:6]:
            b = expect[f][0] if f in expect else None
            out.append('%08x %s' % (f, (b['edid'] + ' (viewer: ' + refdump.get(f, '?') + ')') if b else reason(f)))
        return ('; '.join(out) + (' ...' if len(forms) > 6 else '')) if out else ''

    row('drawn', not missing and len(expect) > 0,
        'the game shows %d, drawn %d, NOT drawn %d%s'
        % (len(expect), len(set(expect) & drawn), len(missing),
           (' -- ' + names(missing, lambda f: '')) if missing else ''))
    row('hidden', not extra,
        'the game hides %d (enable parents on %d references, %d of them start in the other state than '
        'their own flag says), drawn anyway %d%s'
        % (len(why_hidden), parented, flipped, len(extra),
           (' -- ' + names(extra, lambda f: why_hidden.get(f, 'not a reference of this cell'))) if extra else ''))
    for reason, forms in sorted(unjudged.items()):
        bases = sorted(set((plug.baseinfo(U32.unpack_from(dict(plug.fields(plug.refs[f]))[b'NAME'])[0]) or {}).get('edid', '?')
                           for f in forms))
        print('  SKIP  [drawn] %s: %d not judged (%s): %s' % (cell, len(forms), reason, ', '.join(bases[:6])))

    # placement
    floor = 0.0
    worst = (0.0, None)
    n_plain = n_root = n_scol = 0
    signal = []
    bad = []
    rooted = []          # (form, base info, info, pos, R, scale) for the anchor row
    for form in sorted(set(expect) & drawn):
        b, pos, rot, scale = expect[form]
        if b['type'] == 'SCOL':
            n_scol += 1
            continue
        info = model_info(data_root, b['model'], mcache)
        R = np.array(coc.euler_matrix(*rot), dtype=np.float64)
        w = world(info['drop'], pos, R, scale)
        lo, hi = w.min(0), w.max(0)
        dlo = np.min([r['lo'] for r in dump[form]], axis=0)
        dhi = np.max([r['hi'] for r in dump[form]], axis=0)
        err = float(max(np.abs(lo - dlo).max(), np.abs(hi - dhi).max()))
        if info['rootid']:
            n_plain += 1
            floor = max(floor, err)
        else:
            n_root += 1
            wk = world(info['keep'], pos, R, scale)
            signal.append(float(max(np.abs(lo - wk.min(0)).max(), np.abs(hi - wk.max(0)).max())))
            rooted.append((form, b, info, pos, R, scale))
        if err > worst[0]:
            worst = (err, form)
        if err > bar:
            bad.append((err, form, b['edid']))
    bad.sort(reverse=True)
    text = ('%d boxes rebuilt, worst error %.3f (bar %.2f); models with a plain root %d, worst %.3f; '
            'models whose root carries a transform %d' % (n_plain + n_root, worst[0], bar, n_plain, floor, n_root))
    if signal:
        text += ', their boxes would move %.1f to %.1f with the root kept' % (min(signal), max(signal))
    else:
        text += ' (none here: this cell cannot tell the two root rules apart)'
    if n_scol:
        text += '; %d collections left to cell_open.sh' % n_scol
    if bad:
        text += ' -- over the bar %d: ' % len(bad) + '; '.join('%08x %s %.1f' % (f, e, v) for v, f, e in bad[:6])
    row('placed', not bad and n_plain + n_root > 0, text)

    # the anchor: the game's own combined meshes, read with no viewer in the loop
    if anchor:
        ins = combined_instances(esm, cell_form, os.path.join(out_dir, 'combined', cell))
        if ins is None:
            print('  SKIP  [anchor] %s: the game\'s MeshesExtra archive is not beside the plugin' % cell)
        else:
            bykey = {}
            for tr, bound, nv in ins:
                bykey.setdefault(tuple(int(round(v)) for v in tr), []).append((bound, nv))
            cnt = dict(drop=0, keep=0, both=0, neither=0)
            nrefs = 0
            for form, b, info, pos, R, scale in rooted:
                cands = bykey.get(tuple(int(round(v)) for v in pos))
                if not cands:
                    continue
                nrefs += 1
                for blk, nv, cd, ck in info['shapes']:
                    hit = {}
                    for key, c in (('drop', cd), ('keep', ck)):
                        wc = world(c[None, :], pos, R, scale)[0]
                        hit[key] = any(n == nv and np.abs(wc - bound).max() < 2.0 for bound, n in cands)
                    cnt['both' if hit['drop'] and hit['keep'] else 'drop' if hit['drop']
                        else 'keep' if hit['keep'] else 'neither'] += 1
            if nrefs == 0:
                print('  SKIP  [anchor] %s: %d combined instances, none of a model whose root carries a transform'
                      % (cell, len(ins)))
            else:
                row('anchor', cnt['drop'] > 0 and cnt['keep'] == 0,
                    '%d combined instances; %d references of root-carrying models found in them: shapes whose stored '
                    'bound fits only with the root dropped %d, only with it kept %d, either %d, neither %d'
                    % (len(ins), nrefs, cnt['drop'], cnt['keep'], cnt['both'], cnt['neither']))
    return rows, fails


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    bar = 0.05
    anchor = '--no-anchor' not in sys.argv
    if '--bar' in sys.argv:
        bar = float(sys.argv[sys.argv.index('--bar') + 1])
        args.remove(sys.argv[sys.argv.index('--bar') + 1])
    if len(args) < 4:
        print(__doc__)
        return 2
    esm, data_root, out_dir, cells = args[0], args[1], args[2], args[3:]
    plug = Plugin(esm, cells)
    print('  plugin walked: %d base records, %d placed references' % (len(plug.base), len(plug.refs)))
    rows = fails = 0
    mcache = {}
    for cell in cells:
        r, f = check_cell(esm, plug, data_root, out_dir, cell, bar, anchor, mcache)
        rows += r
        fails += f
    print('PASS  %d rows' % rows if fails == 0 else 'FAIL  %d of %d rows' % (fails, rows))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
