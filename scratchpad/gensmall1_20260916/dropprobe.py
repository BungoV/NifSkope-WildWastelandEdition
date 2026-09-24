#!/usr/bin/env python
"""The asymmetric-drop proof, chunk (-32,0) dim 32 (plan section 5 row 13).

`ww-spec-gate-audit`: the pre-registered number is *"2,628 of 42,560 placements
(6.17%) have no geometry in the file at all"*, written by lane SPECS on
2026-09-06 (`scratchpad/specs_20260906/spec_fo4cs_native.md:277`).  This script
PRINTS THE TABLE, not the count:

  * every shape in the `.BTO`, with its vertex count, so the saturated buckets
    are visible rather than asserted;
  * the placement indices the file actually carries, against the manifest's own
    42,560 rows;
  * where the first miss is, and what the survival rate is AFTER it, which is
    the claim that the loss is order-dependent rather than importance-dependent;
  * the dropped placements by TYPE and by base, ten rows, because a drop that
    is all one kind of object is a different defect from one that is not.

The reader is the glTF gates' independent NIF reader; only the vertex-colour
walk is new here.  The identity index is R + G*256 (`--identity`, which since
2026-09-12 is OPT-IN -- a bake without it carries no index at all and this
measurement silently reads zero, which is named in the report).
"""
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tests', 'spells'))
import gltf_nifread as G


class Probe(G.Nif):
    def _read_all(self):
        self.buckets = []
        self.present = set()
        d = self.data
        for i in range(self.numBlocks):
            if self.type[i] not in G.SHAPE_TYPES:
                continue
            name, t, r, s, o = self._avobject(self.start[i])
            o += 16                                   # Bounding Sphere
            o += 4                                    # Skin
            o += 4                                    # Shader Property
            o += 4                                    # Alpha Property
            desc = struct.unpack_from('<Q', d, o)[0]; o += 8
            ntri = self._u32(o); o += 4
            nv = struct.unpack_from('<H', d, o)[0]; o += 2
            dsize = self._u32(o); o += 4
            stride = (desc & 0xF) * 4
            va = (desc >> 44) & 0xFFF
            want = nv * stride + ntri * 6
            if dsize != want:
                raise G.NifError('shape %d %r: data size %d != %d' % (i, name, dsize, want))
            off = 0
            if va & G.VA_VERTEX:
                off += 16 if (va & G.VA_FULLPREC) else 8
            if va & G.VA_UV:
                off += 4
            if va & G.VA_UV2:
                off += 4
            if va & G.VA_NORMALS:
                off += 4
            if va & G.VA_TANGENTS:
                off += 4
            n = 0
            if va & G.VA_COLORS:
                base = o + off
                rb = d[base:base + nv * stride:stride]
                gb = d[base + 1:base + 1 + nv * stride:stride]
                ids = set(rb[k] + gb[k] * 256 for k in range(len(rb)))
                self.present |= ids
                n = len(ids)
            self.buckets.append((i, name, nv, ntri, stride, va, n))


def main(bto, manifest):
    p = Probe(bto)
    rows = []
    with open(manifest, 'r', encoding='utf-8', errors='replace') as f:
        head = f.readline()
        for line in f:
            if not line or not line[0].isdigit():
                continue
            t = line.split()
            rows.append((int(t[0]), t[1], t[2], t[10] if len(t) > 10 else '-'))
    total = len(rows)
    idx = set(r[0] for r in rows)

    print('# the asymmetric-drop proof, %s' % os.path.basename(bto))
    print('%s' % head.rstrip())
    print()
    print('## the shapes, biggest first (the bucket cap is 65,535 vertices)')
    print('%-5s %-38s %9s %9s %7s %6s' % ('block', 'name', 'vertices', 'triangles', 'stride', 'ids'))
    for b in sorted(p.buckets, key=lambda x: -x[2])[:12]:
        print('%-5d %-38s %9d %9d %7d %6d' % (b[0], b[1][:38], b[2], b[3], b[4], b[6]))
    sat = [b for b in p.buckets if b[2] >= 65000]
    print('shapes: %d   at 65,000 vertices or more: %d   vertices in file: %d'
          % (len(p.buckets), len(sat), sum(b[2] for b in p.buckets)))
    print()

    have = p.present & idx
    miss = sorted(idx - p.present)
    print('## the placements')
    print('manifest rows          %d' % total)
    print('indices with geometry  %d' % len(have))
    print('indices with NONE      %d  (%.2f%%)' % (len(miss), 100.0 * len(miss) / total if total else 0.0))
    stray = p.present - idx
    print('indices in the file that the manifest does not list: %d' % len(stray))
    print()

    if miss:
        first = miss[0]
        after = total - first
        missAfter = sum(1 for m in miss if m >= first)
        print('## is the loss ORDER-dependent?')
        print('first index with no geometry      %d' % first)
        print('indices at or after it            %d' % after)
        print('of those, with no geometry        %d  (%.1f%%)' % (missAfter, 100.0 * missAfter / after))
        print('of those, WITH geometry           %d  (%.1f%%)' % (after - missAfter, 100.0 * (after - missAfter) / after))
        print()
        byType, byBase = {}, {}
        for i, base, typ, part in rows:
            if i in p.present:
                continue
            byType[typ] = byType.get(typ, 0) + 1
            byBase[base] = byBase.get(base, 0) + 1
        print('## what was dropped, by record type')
        for k, v in sorted(byType.items(), key=lambda kv: -kv[1]):
            print('  %-8s %6d' % (k, v))
        print('## the ten bases that lost the most placements')
        for k, v in sorted(byBase.items(), key=lambda kv: -kv[1])[:10]:
            print('  %-10s %6d' % (k, v))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
