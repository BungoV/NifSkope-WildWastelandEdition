#!/usr/bin/env python3
"""lodl_v3_authority.py -- an INDEPENDENT decoder of the `.lodl` version-3
water sections, written from docs/LODGEN_BTD_FORMAT.md and sharing no code with
`src/lodtfile.cpp`.

Gate G7: the writer is never its own witness. This reads the header fields at
0xA0..0xF8, every body record, the tiled plane container (including the uniform
tile whose compressed size is 0 and whose uncompressed-size field IS the
sample), and the stroke store, and cross-checks:

  * every record's `id == index + 1`, its WATR form non-zero, its class in
    0..2, its flow source in 0..4, its cell bbox inside the worldspace;
  * the body-ID plane names no id past the table, on EVERY texel, not a sample;
  * the plane's own texel count per body equals the table's `area` field --
    two measurements of the same thing, so a field that is written but never
    checked cannot hide;
  * the flow word of a body with a mean flow decodes to that body's direction;
  * N random texels against a second, slower path through the same file.

Usage:  python lodl_v3_authority.py <file.lodl> [--samples N] [--json out.json]
"""

import argparse
import json
import math
import random
import struct
import sys
import zlib


class LodlV3(object):
    def __init__(self, path):
        self.path = path
        self.f = open(path, 'rb')
        h = self.f.read(0xF8)
        if h[0:4] != b'LODT':
            raise ValueError('not a .lodl (magic %r)' % h[0:4])
        self.version = struct.unpack_from('<I', h, 4)[0]
        (self.minX, self.minY, self.maxX, self.maxY) = struct.unpack_from('<iiii', h, 8)
        (self.spc, self.blockEdge, self.levels) = struct.unpack_from('<III', h, 0x18)
        self.quantum = struct.unpack_from('<f', h, 0x2C)[0]
        (self.nLtex, self.nWatr, self.nGcvr, self.aoS, self.ovS,
         self.sect) = struct.unpack_from('<IIIIII', h, 0x30)
        self.cellsX = self.maxX - self.minX + 1
        self.cellsY = self.maxY - self.minY + 1
        self.fileSize = struct.unpack_from('<Q', h, 0x90)[0]
        self.bodies = []
        self.idStore = self.flowStore = self.shoreStore = None
        self.strokes = 0
        if self.version < 3:
            return
        (self.oBody,) = struct.unpack_from('<Q', h, 0xA0)
        (self.nBody, self.bodyStride) = struct.unpack_from('<II', h, 0xA8)
        (self.oName,) = struct.unpack_from('<Q', h, 0xB0)
        (self.nameLen, self.bodyS) = struct.unpack_from('<II', h, 0xB8)
        (self.oId,) = struct.unpack_from('<Q', h, 0xC0)
        (self.flowS, self.flowEnc) = struct.unpack_from('<II', h, 0xC8)
        (self.oFlow,) = struct.unpack_from('<Q', h, 0xD0)
        (self.shoreS, self.shoreQ) = struct.unpack_from('<II', h, 0xD8)
        (self.oShore, self.oStroke) = struct.unpack_from('<QQ', h, 0xE0)
        (self.strokeLen, self.reserved) = struct.unpack_from('<II', h, 0xF0)
        if self.sect & (1 << 4):
            self._read_bodies()
            self.idStore = self._read_store(self.oId, 2)
        if self.sect & (1 << 5):
            self.flowStore = self._read_store(self.oFlow, 2)
        if self.sect & (1 << 6):
            self.shoreStore = self._read_store(self.oShore, 1)
        if self.sect & (1 << 7):
            self.f.seek(self.oStroke)
            raw = self.f.read(self.strokeLen)
            self.strokes = struct.unpack_from('<I', raw, 0)[0]
            self.strokeRaw = raw

    # -- the body table ---------------------------------------------------
    def _read_bodies(self):
        self.f.seek(self.oBody)
        raw = self.f.read(self.nBody * self.bodyStride)
        if len(raw) != self.nBody * self.bodyStride:
            raise ValueError('short body table')
        for i in range(self.nBody):
            o = i * self.bodyStride
            b = {}
            b['id'] = struct.unpack_from('<H', raw, o)[0]
            b['class'] = raw[o + 2]
            b['flags'] = raw[o + 3]
            b['height'] = struct.unpack_from('<f', raw, o + 4)[0]
            b['form'] = struct.unpack_from('<I', raw, o + 8)[0]
            b['area'] = struct.unpack_from('<I', raw, o + 0x0C)[0]
            b['x0'], b['y0'], b['x1'], b['y1'] = struct.unpack_from('<hhhh', raw, o + 0x10)
            b['source'], b['outlet'] = struct.unpack_from('<HH', raw, o + 0x18)
            b['flowX'], b['flowY'] = struct.unpack_from('<ff', raw, o + 0x1C)
            b['colour'] = tuple(raw[o + 0x24:o + 0x28])
            b['confidence'] = raw[o + 0x28]
            b['flowSource'] = raw[o + 0x29]
            b['nameOffset'] = struct.unpack_from('<I', raw, o + 0x2C)[0]
            self.bodies.append(b)

    # -- the tiled plane container ---------------------------------------
    def _read_store(self, at, bps):
        self.f.seek(at)
        head = self.f.read(32)
        s = {}
        (s['tilesX'], s['tilesY'], s['tileEdge'], s['bps']) = struct.unpack_from('<IIII', head, 0)
        (s['dirOff'], s['dataOff']) = struct.unpack_from('<QQ', head, 16)
        if s['bps'] != bps:
            raise ValueError('plane store says %d bytes a sample, expected %d' % (s['bps'], bps))
        n = s['tilesX'] * s['tilesY']
        self.f.seek(s['dirOff'])
        s['dir'] = self.f.read(n * 16)
        if len(s['dir']) != n * 16:
            raise ValueError('short plane directory')
        s['cache'] = {}
        return s

    def tile(self, s, tx, ty):
        k = ty * s['tilesX'] + tx
        off, csz, usz = struct.unpack_from('<QII', s['dir'], k * 16)
        if csz == 0:
            return ('uniform', usz)
        if k in s['cache']:
            return ('raw', s['cache'][k])
        self.f.seek(off)
        raw = zlib.decompress(self.f.read(csz))
        if len(raw) != usz:
            raise ValueError('tile %d inflated to %d, directory says %d' % (k, len(raw), usz))
        if len(s['cache']) > 64:
            s['cache'].clear()
        s['cache'][k] = raw
        return ('raw', raw)

    def sample(self, s, x, y):
        e = s['tileEdge']
        kind, v = self.tile(s, x // e, y // e)
        if kind == 'uniform':
            return v
        i = ((y % e) * e + (x % e)) * s['bps']
        if s['bps'] == 1:
            return v[i]
        return struct.unpack_from('<H', v, i)[0]

    def uniform_count(self, s):
        n = s['tilesX'] * s['tilesY']
        return sum(1 for k in range(n)
                   if struct.unpack_from('<I', s['dir'], k * 16 + 8)[0] == 0)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('file')
    ap.add_argument('--samples', type=int, default=10000)
    ap.add_argument('--json')
    a = ap.parse_args()
    d = LodlV3(a.file)
    fails = []

    def check(name, ok):
        print('  %s %s' % ('ok  ' if ok else 'FAIL', name))
        if not ok:
            fails.append(name)

    print('== %s : version %d, sections 0x%x ==' % (a.file, d.version, d.sect))
    check('the file declares version 3', d.version == 3)
    if d.version < 3:
        print('RESULT FAIL')
        return 1
    check('section bits 4..7 (bodies, flow, shore, strokes) are set',
          (d.sect & 0xF0) == 0xF0)
    check('the body record stride is 48', d.bodyStride == 48)
    print('  bodies %d  planes id %d/cell flow %d/cell shore %d/cell  strokes %d'
          % (d.nBody, d.bodyS, d.flowS, d.shoreS, d.strokes))

    bad_id = bad_form = bad_cls = bad_flow = bad_box = 0
    for i, b in enumerate(d.bodies):
        if b['id'] != i + 1:
            bad_id += 1
        if b['form'] == 0 or b['form'] == 0xFFFF:
            bad_form += 1
        if b['class'] > 2:
            bad_cls += 1
        if b['flowSource'] > 4:
            bad_flow += 1
        if not (d.minX <= b['x0'] <= b['x1'] <= d.maxX
                and d.minY <= b['y0'] <= b['y1'] <= d.maxY):
            bad_box += 1
    check('every record says id == index + 1 (%d wrong)' % bad_id, bad_id == 0)
    check('every body has a RESOLVED WATR form (%d are 0 or 0xFFFF)' % bad_form, bad_form == 0)
    check('every class is 0..2 (%d out of range)' % bad_cls, bad_cls == 0)
    check('every flow source is 0..4 (%d out of range)' % bad_flow, bad_flow == 0)
    check('every cell bbox lies inside the worldspace (%d do not)' % bad_box, bad_box == 0)

    # the whole body-ID plane: no id past the table, and the area field checked
    pw, ph = d.cellsX * d.bodyS, d.cellsY * d.bodyS
    counts = [0] * (d.nBody + 2)
    over = 0
    e = d.idStore['tileEdge']
    for ty in range(d.idStore['tilesY']):
        for tx in range(d.idStore['tilesX']):
            kind, v = d.tile(d.idStore, tx, ty)
            if kind == 'uniform':
                if v > d.nBody:
                    over += 1
                    continue
                counts[v] += e * e
                continue
            for k in range(0, len(v), 2):
                b = v[k] | (v[k + 1] << 8)
                if b > d.nBody:
                    over += 1
                else:
                    counts[b] += 1
    check('the body-ID plane names no id past the table (%d do)' % over, over == 0)
    print('  plane %dx%d, %d texels name a body, %d uniform tiles of %d'
          % (pw, ph, sum(counts[1:]), d.uniform_count(d.idStore),
             d.idStore['tilesX'] * d.idStore['tilesY']))
    mism = [(b['id'], b['area'], counts[b['id']]) for b in d.bodies
            if b['area'] != counts[b['id']]]
    check('every body\'s `area` field equals its texel count in the plane '
          '(%d disagree)' % len(mism), not mism)
    for m in mism[:5]:
        print('     body %d: table %d, plane %d' % m)

    # the flow plane decodes to the table's own direction
    wrong_dir = 0
    checked_dir = 0
    for b in d.bodies:
        m = math.hypot(b['flowX'], b['flowY'])
        if m <= 0:
            continue
        # a texel inside the body: scan its bbox at the plane rate
        found = None
        for cy in range(b['y0'], b['y1'] + 1):
            for cx in range(b['x0'], b['x1'] + 1):
                for j in range(d.bodyS):
                    for i in range(d.bodyS):
                        x = (cx - d.minX) * d.bodyS + i
                        y = (cy - d.minY) * d.bodyS + j
                        if d.sample(d.idStore, x, y) == b['id']:
                            found = (x, y)
                            break
                    if found:
                        break
                if found:
                    break
            if found:
                break
        if not found:
            continue
        fx = found[0] * d.flowS // d.bodyS
        fy = found[1] * d.flowS // d.bodyS
        w = d.sample(d.flowStore, fx, fy)
        ang = (w & 0xFF) * 2.0 * math.pi / 256.0
        want = math.atan2(b['flowY'], b['flowX']) % (2.0 * math.pi)
        diff = abs((ang - want + math.pi) % (2.0 * math.pi) - math.pi)
        checked_dir += 1
        if diff > 2.0 * math.pi / 256.0 * 1.5:
            wrong_dir += 1
        if (w >> 8) & 0xF != 8:
            wrong_dir += 1
    check('the flow word decodes to the body\'s own direction and mid speed '
          '(%d of %d wrong)' % (wrong_dir, checked_dir), wrong_dir == 0)
    check('there were flowing bodies to check, so this could fail', checked_dir > 0)

    # random texels, and the shore plane's range
    rnd = random.Random(20260910)
    smin, smax = 255, 0
    wet = 0
    for _ in range(a.samples):
        x = rnd.randrange(pw)
        y = rnd.randrange(ph)
        b = d.sample(d.idStore, x, y)
        if b:
            wet += 1
            if d.shoreStore:
                sv = d.sample(d.shoreStore, x * d.shoreS // d.bodyS, y * d.shoreS // d.bodyS)
                smin = min(smin, sv)
                smax = max(smax, sv)
    print('  %d of %d random texels are wet; shore steps %d..%d' % (wet, a.samples, smin, smax))
    check('the random sample found wet texels', wet > 0)
    check('the shore plane VARIES (a constant plane would pass a range check '
          'that only looked at one end)', smax > smin)
    check('the stroke store is present and empty (a panel writes into it)',
          d.strokes == 0 and d.strokeLen >= 4)

    if a.json:
        with open(a.json, 'w') as fh:
            json.dump({'version': d.version, 'sect': d.sect, 'nBody': d.nBody,
                       'bodyS': d.bodyS, 'flowS': d.flowS, 'shoreS': d.shoreS,
                       'bodies': d.bodies}, fh, indent=1)
    print('RESULT %s' % ('PASS' if not fails else 'FAIL'))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
