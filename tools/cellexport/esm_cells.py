"""Fallout 4 plugin reader for one rectangle of exterior cells (lane CELL1, 2026-09-26).

Reads straight out of the .esm, no NifSkope, no game:
  * every base record carrying a MODL (its type, EDID, near model, the STAT
    MNAM LOD slots, a base-level MODS material swap);
  * SCOL parts with their placement lists (pos, rot, scale per part);
  * MSWP material swaps (original -> replacement material paths);
  * every REFR under the worldspace's children group, binned to a cell BY ITS
    POSITION (so persistent references land in the right cell);
  * each cell's LAND VHGT height grid (33 x 33, 128 units a step).

The placement convention is the generator's own (src/lodgen.cpp, "Bethesda's
stored euler angles are applied NEGATED relative to Matrix::fromEuler"):
world R = fromEuler(-x, -y, -z), a SCOL part at rp + R * (part.pos * scale),
rotation R * Rpart, scale scale * part.scale.

Usage (prints a census, writes a pickle cache):
  python esm_cells.py <Fallout4.esm> <worldEdid> x0 y0 x1 y1 <cache.pkl>
"""
import math
import os
import pickle
import struct
import sys
import zlib

import numpy as np

COMPRESSED = 0x00040000
DELETED = 0x20
INITIALLY_DISABLED = 0x800

# base record types whose MODL the viewer and the generator draw
MODEL_TYPES = {b'STAT', b'SCOL', b'MSTT', b'FURN', b'CONT', b'DOOR', b'ACTI', b'TREE',
               b'FLOR', b'LIGH', b'MISC', b'TERM', b'BNDS', b'PKIN', b'IDLM', b'MOVS',
               b'STAG', b'TACT', b'WEAP', b'ARMO', b'BOOK', b'ALCH', b'AMMO', b'KEYM',
               b'NOTE', b'INGR', b'CMPO', b'PROJ'}


def _fields(data, off, end):
    big = 0
    while off + 6 <= end:
        t = data[off:off + 4]
        sz = struct.unpack_from('<H', data, off + 4)[0]
        off += 6
        if t == b'XXXX':
            big = struct.unpack_from('<I', data, off)[0]
            off += sz
            continue
        if big:
            sz, big = big, 0
        yield t, data[off:off + sz]
        off += sz


def _cstr(p):
    return p.split(b'\0')[0].decode('cp1252', 'replace')


def euler_matrix(x, y, z):
    """Matrix::fromEuler (src/data/niftypes.cpp), row-major, v' = M v."""
    sx, cx = math.sin(x), math.cos(x)
    sy, cy = math.sin(y), math.cos(y)
    sz, cz = math.sin(z), math.cos(z)
    return np.array([
        [cy * cz, -cy * sz, sy],
        [sx * sy * cz + sz * cx, cx * cz - sx * sy * sz, -sx * cy],
        [sx * sz - cx * sy * cz, cx * sy * sz + sx * cz, cx * cy]], dtype=np.float64)


def placement_matrix(rot):
    return euler_matrix(-rot[0], -rot[1], -rot[2])


class Plugin:
    def __init__(self, path, world_edid):
        with open(path, 'rb') as fh:
            self.buf = fh.read()
        self.world_edid = world_edid
        self.bases = {}      # form -> dict(type, edid, model, mnam[4], mods)
        self.scols = {}      # form -> [(partBase, [(pos3, rot3, scale)])]
        self.mswp = {}       # form -> [(orig, repl)]
        self.world = None
        self.refs = []       # dict(form, base, pos, rot, scale, flags, xmsp)
        self.land = {}       # (cx, cy) -> 33x33 float heights (game units)
        self._walk()

    # ------------------------------------------------------------------ walk
    def _record_data(self, off, size, flags):
        if flags & COMPRESSED:
            raw = self.buf[off + 24:off + 24 + size]
            data = zlib.decompress(raw[4:])
            return data, 0, len(data)
        return self.buf, off + 24, off + 24 + size

    def _walk(self):
        buf = self.buf
        tes4 = struct.unpack_from('<I', buf, 4)[0]
        off = 24 + tes4
        stack = []           # (end, label, gtype)
        target = self.world_edid.encode('cp1252').lower()
        cur_cell_xy = None
        n = len(buf)
        while off + 24 <= n:
            while stack and off >= stack[-1][0]:
                stack.pop()
            t = buf[off:off + 4]
            size = struct.unpack_from('<I', buf, off + 4)[0]
            if t == b'GRUP':
                label = struct.unpack_from('<I', buf, off + 8)[0]
                gtype = struct.unpack_from('<i', buf, off + 12)[0]
                stack.append((off + size, label, gtype))
                off += 24
                continue
            flags = struct.unpack_from('<I', buf, off + 8)[0]
            form = struct.unpack_from('<I', buf, off + 12)[0]
            rec_end = off + 24 + size
            in_world = self.world is not None and any(g == 1 and l == self.world for _, l, g in stack)
            if t in MODEL_TYPES:
                self._base(t, off, size, flags, form)
            elif t == b'MSWP':
                self._mswp(off, size, flags, form)
            elif t == b'WRLD' and self.world is None:
                d, a, b = self._record_data(off, size, flags)
                for ft, p in _fields(d, a, b):
                    if ft == b'EDID' and _cstr(p).encode('cp1252').lower() == target:
                        self.world = form
                        break
            elif in_world and t == b'CELL':
                cur_cell_xy = None
                d, a, b = self._record_data(off, size, flags)
                for ft, p in _fields(d, a, b):
                    if ft == b'XCLC' and len(p) >= 8:
                        cur_cell_xy = struct.unpack_from('<ii', p, 0)
            elif in_world and t == b'REFR':
                self._refr(off, size, flags, form)
            elif in_world and t == b'LAND' and cur_cell_xy is not None:
                if not (flags & DELETED):
                    self._land(off, size, flags, cur_cell_xy)
            off = rec_end

    def _base(self, t, off, size, flags, form):
        d, a, b = self._record_data(off, size, flags)
        rec = dict(type=t.decode(), edid='', model='', mnam=['', '', '', ''], mods=0)
        parts = []
        cur = None
        for ft, p in _fields(d, a, b):
            if ft == b'EDID':
                rec['edid'] = _cstr(p)
            elif ft == b'MODL' and not rec['model']:
                s = _cstr(p)
                if s.lower().endswith('.nif'):
                    rec['model'] = s
            elif ft == b'MNAM' and t == b'STAT' and len(p) == 1040:
                rec['mnam'] = [_cstr(p[k * 260:(k + 1) * 260]) for k in range(4)]
            elif ft == b'MODS' and len(p) == 4:
                rec['mods'] = struct.unpack_from('<I', p, 0)[0]
            elif t == b'SCOL' and ft == b'ONAM' and len(p) >= 4:
                cur = struct.unpack_from('<I', p, 0)[0]
            elif t == b'SCOL' and ft == b'DATA' and cur is not None:
                pl = []
                for k in range(len(p) // 28):
                    v = struct.unpack_from('<7f', p, k * 28)
                    pl.append((v[0:3], v[3:6], v[6]))
                parts.append((cur, pl))
                cur = None
        self.bases[form] = rec
        if t == b'SCOL':
            self.scols[form] = parts

    def _mswp(self, off, size, flags, form):
        d, a, b = self._record_data(off, size, flags)
        pairs = []
        orig = None
        for ft, p in _fields(d, a, b):
            if ft == b'BNAM':
                orig = _cstr(p)
            elif ft == b'SNAM' and orig is not None:
                pairs.append((orig, _cstr(p)))
                orig = None
        self.mswp[form] = pairs

    def _refr(self, off, size, flags, form):
        d, a, b = self._record_data(off, size, flags)
        r = dict(form=form, base=0, pos=(0.0, 0.0, 0.0), rot=(0.0, 0.0, 0.0), scale=1.0,
                 flags=flags, xmsp=0)
        for ft, p in _fields(d, a, b):
            if ft == b'NAME' and len(p) >= 4:
                r['base'] = struct.unpack_from('<I', p, 0)[0]
            elif ft == b'DATA' and len(p) >= 24:
                v = struct.unpack_from('<6f', p, 0)
                r['pos'], r['rot'] = v[0:3], v[3:6]
            elif ft == b'XSCL' and len(p) >= 4:
                r['scale'] = struct.unpack_from('<f', p, 0)[0]
            elif ft == b'XMSP' and len(p) >= 4:
                r['xmsp'] = struct.unpack_from('<I', p, 0)[0]
        self.refs.append(r)

    def _land(self, off, size, flags, xy):
        d, a, b = self._record_data(off, size, flags)
        for ft, p in _fields(d, a, b):
            if ft == b'VHGT' and len(p) >= 4 + 33 * 33:
                base = struct.unpack_from('<f', p, 0)[0]
                dl = np.frombuffer(p, dtype=np.int8, count=33 * 33, offset=4).astype(np.float64).reshape(33, 33)
                col0 = base + np.cumsum(dl[:, 0])
                rows = np.cumsum(np.concatenate([col0[:, None], dl[:, 1:]], axis=1), axis=1)
                self.land[tuple(xy)] = rows * 8.0


# -------------------------------------------------------------- placements

SKIP_PREFIXES = ('markers\\', 'effects\\', 'meshes\\markers\\', 'meshes\\effects\\')


def expand(plugin, x0, y0, x1, y1):
    """Every drawable placement in the rectangle, SCOLs expanded.

    Returns (placements, census). A placement is
    dict(ref, part, base, model, mnam, M (3x3, scale folded in), T, swap)."""
    out = []
    census = dict(refs=0, deleted=0, disabled=0, nobase=0, nomodel=0, skipped_prefix=0,
                  scol_refs=0, scol_parts=0, plain=0)
    for r in plugin.refs:
        cx = math.floor(r['pos'][0] / 4096.0)
        cy = math.floor(r['pos'][1] / 4096.0)
        if not (x0 <= cx <= x1 and y0 <= cy <= y1):
            continue
        census['refs'] += 1
        if r['flags'] & DELETED:
            census['deleted'] += 1
            continue
        if r['flags'] & INITIALLY_DISABLED:
            census['disabled'] += 1
            continue
        b = plugin.bases.get(r['base'])
        if b is None:
            census['nobase'] += 1
            continue
        R = placement_matrix(r['rot'])
        rp = np.array(r['pos'], dtype=np.float64)
        s = r['scale']
        swap = r['xmsp'] or b['mods']
        if b['type'] == 'SCOL':
            census['scol_refs'] += 1
            k = 0
            for pbase, pls in plugin.scols.get(r['base'], ()):
                pb = plugin.bases.get(pbase)
                for (pp, pr, ps) in pls:
                    k += 1
                    if pb is None or not pb['model']:
                        census['nomodel'] += 1
                        continue
                    if pb['model'].lower().startswith(SKIP_PREFIXES):
                        census['skipped_prefix'] += 1
                        continue
                    P = placement_matrix(pr)
                    out.append(dict(ref=r['form'], part=k - 1, base=pbase, model=pb['model'],
                                    mnam=pb['mnam'], M=(R @ P) * (s * ps),
                                    T=rp + R @ (np.array(pp) * s),
                                    swap=r['xmsp'] or pb['mods']))
                    census['scol_parts'] += 1
            continue
        if not b['model']:
            census['nomodel'] += 1
            continue
        if b['model'].lower().startswith(SKIP_PREFIXES):
            census['skipped_prefix'] += 1
            continue
        census['plain'] += 1
        out.append(dict(ref=r['form'], part=-1, base=r['base'], model=b['model'], mnam=b['mnam'],
                        M=R * s, T=rp, swap=swap))
    return out, census


def load_cached(esm, world, cache):
    if cache and os.path.isfile(cache) and os.path.getmtime(cache) > os.path.getmtime(esm):
        with open(cache, 'rb') as fh:
            state = pickle.load(fh)
        if state.get('world_edid', '').lower() == world.lower():
            p = Plugin.__new__(Plugin)
            p.__dict__.update(state)
            return p
    p = Plugin(esm, world)
    p.buf = None
    if cache:
        with open(cache, 'wb') as fh:
            pickle.dump(dict(p.__dict__), fh, protocol=4)
    return p


def main(argv):
    esm, world = argv[1], argv[2]
    x0, y0, x1, y1 = (int(v) for v in argv[3:7])
    cache = argv[7] if len(argv) > 7 else None
    p = load_cached(esm, world, cache)
    print('world %s form %08X: bases %d, scols %d, mswp %d, refs in world %d, land cells %d'
          % (world, p.world or 0, len(p.bases), len(p.scols), len(p.mswp), len(p.refs), len(p.land)))
    pl, c = expand(p, x0, y0, x1, y1)
    print('rect %d,%d..%d,%d: %s -> %d placements, %d distinct models'
          % (x0, y0, x1, y1, c, len(pl), len({q['model'].lower() for q in pl})))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
