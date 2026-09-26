"""FLAT1 (2026-09-26): the INDEPENDENT reading behind the flat-object rule.

Builds on ROADS1's reader (scratchpad/roads1_20260926/work/roadgeo.py + nlc.py, imported from this worktree) and
adds what the flat rule needs and ROADS1 did not read:
  * every cell's LAND heights (VHGT, the winning version), row 0 south, column 0 west, 128 units a sample;
  * every cell's water (CELL DATA bit 1 + XCLW, else the worldspace default water height from WRLD DNAM).

Nothing here links NifSkope or reads a NifSkope output.
"""
import math
import os
import struct
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROADS1 = os.path.join(HERE, '..', '..', 'roads1_20260926', 'work')
sys.path.insert(0, os.path.join(HERE, '..', '..', '..', 'tests', 'spells'))
sys.path.insert(0, ROADS1)
import nlc  # noqa: E402

_BaseWorld = nlc.World
NO_WATER = (0xFF7FFFFF, 0x7F7FFFFF, 0x4F7FFFC9)


class LandWorld(_BaseWorld):
    """nlc.World that also keeps each exterior cell's LAND record and water facts."""

    def __init__(self, plugins, ws_global):
        self.lands = {}      # global cell form -> (buf, off, size, flags)  (last plugin wins)
        self.cellw = {}      # global cell form -> dict(hasWater, xclw or None)
        _BaseWorld.__init__(self, plugins, ws_global)

    def _children(self, pi, buf, masters, off, end, cell, top):
        while off + 24 <= end:
            t = buf[off:off + 4]
            size = struct.unpack_from('<I', buf, off + 4)[0]
            if t == b'GRUP':
                gtype = struct.unpack_from('<i', buf, off + 12)[0]
                lab = struct.unpack_from('<I', buf, off + 8)[0]
                if gtype == 6:
                    c = self._gmap(pi, masters, lab)
                    self._children(pi, buf, masters, off + 24, off + size, c, False)
                elif gtype in (4, 5):
                    self._children(pi, buf, masters, off + 24, off + size, None, False)
                else:
                    self._children(pi, buf, masters, off + 24, off + size, cell, False)
                off += size
                continue
            flags, form = struct.unpack_from('<II', buf, off + 8)
            g = self._gmap(pi, masters, form)
            if t == b'CELL':
                d, o, s = nlc.rec_payload(buf, off + 24, size, flags)
                ent = self.cells.setdefault(g, {'x': None, 'y': None, 'persistent': False})
                if top:
                    ent['persistent'] = True
                w = {'hasWater': False, 'xclw': None}
                if d is not None:
                    for ft, pl in nlc.rec_fields(d, o, s):
                        if ft == b'XCLC' and len(pl) >= 8:
                            ent['x'], ent['y'] = struct.unpack_from('<ii', pl, 0)
                        elif ft == b'DATA' and len(pl) >= 2:
                            w['hasWater'] = bool(struct.unpack_from('<H', pl, 0)[0] & 2)
                        elif ft == b'XCLW' and len(pl) >= 4:
                            raw = struct.unpack_from('<I', pl, 0)[0]
                            if raw not in NO_WATER:
                                w['xclw'] = struct.unpack_from('<f', pl, 0)[0]
                self.cellw[g] = w
            elif t == b'LAND':
                if cell is not None:
                    self.lands[cell] = (buf, off + 24, size, flags)
            elif t == b'REFR':
                d, o, s = nlc.rec_payload(buf, off + 24, size, flags)
                r = self.refs.get(g)
                if r is None:
                    r = self.refs[g] = {'cell': cell}
                r.update({'flags': flags, 'base': 0, 'pos': (0.0, 0.0, 0.0), 'rot': (0.0, 0.0, 0.0),
                          'scale': 1.0, 'xmsp': 0, 'plugin': pi})
                r.setdefault('origin', pi)
                if d is not None:
                    for ft, pl in nlc.rec_fields(d, o, s):
                        if ft == b'XMSP' and len(pl) >= 4:
                            r['xmsp'] = self._gmap(pi, masters, struct.unpack_from('<I', pl, 0)[0])
                        elif ft == b'NAME' and len(pl) >= 4:
                            r['base'] = self._gmap(pi, masters, struct.unpack_from('<I', pl, 0)[0])
                        elif ft == b'DATA' and len(pl) >= 24:
                            v = struct.unpack_from('<6f', pl, 0)
                            r['pos'], r['rot'] = v[:3], v[3:]
                        elif ft == b'XSCL' and len(pl) >= 4:
                            r['scale'] = struct.unpack_from('<f', pl, 0)[0]
            off += 24 + size


nlc.World = LandWorld
import roadgeo as rg  # noqa: E402

rg.NIFXML = os.path.abspath(os.path.join(HERE, '..', '..', '..', 'build', 'nif.xml'))


def vhgt(fields):
    for ft, pl in fields:
        if ft == b'VHGT' and len(pl) >= 4 + 33 * 33:
            off = struct.unpack_from('<f', pl, 0)[0]
            d = np.frombuffer(pl, dtype=np.int8, count=33 * 33, offset=4).reshape(33, 33).astype(np.float64)
            # column 0 accumulates down the rows, then each row accumulates across
            col0 = np.cumsum(d[:, 0])
            rows = np.cumsum(np.concatenate([col0[:, None], d[:, 1:]], axis=1), axis=1)
            return (rows + off) * 8.0
    return None


class Terrain:
    """Heights by cell, bilinear inside the 33x33 grid; water per cell."""

    def __init__(self, R):
        W = R.W
        self.h = {}
        self.water = {}
        ws = W.base(W.ws)
        self.defLand, self.defWater = 0.0, 0.0
        if ws:
            for ft, pl in ws[2]:
                if ft == b'DNAM' and len(pl) >= 8:
                    self.defLand, self.defWater = struct.unpack_from('<2f', pl, 0)
        for cf, c in W.cells.items():
            if c['x'] is None or c['persistent'] and cf not in W.lands:
                continue
            key = (c['x'], c['y'])
            wv = W.cellw.get(cf)
            if wv is not None:
                self.water[key] = (wv['hasWater'], wv['xclw'] if wv['xclw'] is not None else self.defWater)
            if cf in W.lands:
                buf, off, size, flags = W.lands[cf]
                d, o, s = nlc.rec_payload(buf, off, size, flags)
                if d is None:
                    continue
                g = vhgt(list(nlc.rec_fields(d, o, s)))
                if g is not None:
                    self.h[key] = g

    def at(self, x, y):
        """Vectorised bilinear height; NaN where the cell has no LAND."""
        x = np.asarray(x, np.float64)
        y = np.asarray(y, np.float64)
        cx = np.floor(x / 4096.0).astype(np.int64)
        cy = np.floor(y / 4096.0).astype(np.int64)
        out = np.full(x.shape, np.nan)
        for key in set(zip(cx.ravel().tolist(), cy.ravel().tolist())):
            g = self.h.get(key)
            if g is None:
                continue
            m = (cx == key[0]) & (cy == key[1])
            fx = (x[m] - key[0] * 4096.0) / 128.0
            fy = (y[m] - key[1] * 4096.0) / 128.0
            i0 = np.clip(np.floor(fx).astype(np.int64), 0, 31)
            j0 = np.clip(np.floor(fy).astype(np.int64), 0, 31)
            tx = fx - i0
            ty = fy - j0
            out[m] = (g[j0, i0] * (1 - tx) * (1 - ty) + g[j0, i0 + 1] * tx * (1 - ty)
                      + g[j0 + 1, i0] * (1 - tx) * ty + g[j0 + 1, i0 + 1] * tx * ty)
        return out

    def water_at(self, x, y):
        key = (int(math.floor(x / 4096.0)), int(math.floor(y / 4096.0)))
        w = self.water.get(key)
        return w[1] if (w and w[0]) else None


def world_shapes(R, d):
    """The placement's drawn shapes in world space: list of (shape dict, world pos (N,3))."""
    model = R.model(d['info']['modl']) if d['info'] and d['info']['modl'] else None
    if not model:
        return model
    out = []
    rot = np.asarray(d['rot'])
    pos = np.asarray(d['pos'])
    for s in model:
        out.append((s, pos + (s['pos'] * d['scale']) @ rot.T))
    return out
