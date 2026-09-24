"""Section 1e: is the road's diffuse asked for at the mip its footprint implies,
and does the texture HAVE that mip?

Our rasteriser asks for  mip = 0.5*log2( texture-pixel area / bake-texel area ),
clamped to  getMaxMipLevel() = (DDS mipMapCount - 1).  One bake texel is 32
world units; a road UV repeat measures ~256 world units (stripes.py), so one
bake texel spans 32/256 = 1/8 of the texture -- for a 2048px texture that is
256 pixels across, i.e. mip 8.  A DDS that ships fewer mips than that cannot
answer, and the clamp leaves the texture sampled too sharply, which is exactly
the banding the picture shows.

Prints, per unique road diffuse: size, shipped mips, the mip our code asks for
from that shape's own UV density, and whether the clamp bites.
"""

import json
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = 'E:/Tools/Fallout 4/DataUnpacked/Data'
UPT = 32.0
SEP = chr(92)


def mat_rel(matname):
    """lodgenRoadMaterialPath's rule: key on the LAST 'materials/' in the path,
    because Bethesda's road materials name absolute build paths."""
    p = matname.replace(SEP, '/').lower()
    k = p.rfind('materials/')
    return p[k:] if k >= 0 else 'materials/' + p


def first_dds(b):
    out = []
    for m in re.finditer(rb'[-\w\/. ]{5,240}\.dds', b, re.IGNORECASE):
        s, e = m.span()
        # length-prefixed?  the u32 before the run must equal its length
        for start in range(s, s + 8):
            if start < 4:
                continue
            n = struct.unpack_from('<I', b, start - 4)[0]
            if n in (e - start, e - start + 1):
                out.append((start, b[start:e].decode('latin-1')))
                break
    out.sort()
    return out[0][1] if out else None


def dds_info(rel):
    p = os.path.join(DATA, rel.replace(SEP, '/'))
    if not os.path.isfile(p):
        p2 = os.path.join(DATA, 'textures', rel.replace(SEP, '/'))
        if not os.path.isfile(p2):
            return None
        p = p2
    with open(p, 'rb') as f:
        h = f.read(148)
    if h[:4] != b'DDS ':
        return None
    ht, wd, _, _, mips = struct.unpack_from('<IIIII', h, 12)
    fourcc = h[84:88].decode('latin-1')
    return dict(w=wd, h=ht, mips=mips, fourcc=fourcc,
                size=os.path.getsize(p), path=p)


def main():
    mf = json.load(open(os.path.join(HERE, 'meshflags.json')))
    per = json.load(open(os.path.join(HERE, 'uvperiod.json')))
    wp = {}
    for r in per:
        wp.setdefault((r['model'].lower(), r['shape']), r['world_per_repeat'])

    matcache = {}
    rows = {}
    for r in mf:
        mn = r.get('matname')
        if not mn:
            continue
        rel = mat_rel(mn)
        if rel not in matcache:
            p = os.path.join(DATA, rel)
            tex = None
            if os.path.isfile(p):
                tex = first_dds(open(p, 'rb').read())
            matcache[rel] = tex
        tex = matcache[rel]
        if not tex:
            continue
        key = tex.lower()
        w = wp.get((r['model'].lower(), r['shape']))
        rows.setdefault(key, dict(tex=tex, shapes=0, wps=[]))
        rows[key]['shapes'] += 1
        if w:
            rows[key]['wps'].append(w)

    print('%-52s %10s %5s %6s %8s %8s %6s'
          % ('road diffuse', 'size', 'mips', 'shapes', 'world/rep',
             'mip want', 'clamp'))
    import math
    tot = clamped = 0
    worst = []
    for key, r in sorted(rows.items(), key=lambda kv: -kv[1]['shapes']):
        info = dds_info(r['tex'])
        if info is None:
            print('%-52s  MISSING (%d shapes)' % (r['tex'][-52:], r['shapes']))
            continue
        wps = r['wps']
        wmed = sorted(wps)[len(wps) // 2] if wps else float('nan')
        # texture pixels across one bake texel
        pix = info['w'] * UPT / wmed if wps else float('nan')
        want = math.log2(max(pix, 1.0)) if wps else float('nan')
        cap = info['mips'] - 1
        bite = 'YES' if want > cap + 1e-6 else '.'
        tot += r['shapes']
        if want > cap:
            clamped += r['shapes']
            worst.append((want - cap, r['tex'], r['shapes']))
        print('%-52s %5dx%-4d %5d %6d %8.1f %8.2f %6s'
              % (os.path.basename(r['tex'])[:52], info['w'], info['h'],
                 info['mips'], r['shapes'], wmed, want, bite))
    print('')
    print('shapes whose requested mip is CLAMPED SHORT: %d of %d (%.1f%%)'
          % (clamped, tot, 100.0 * clamped / max(tot, 1)))
    worst.sort(reverse=True)
    for d, t, n in worst[:6]:
        print('   short by %.2f mips (%.1fx too sharp): %s  [%d shapes]'
              % (d, 2 ** d, os.path.basename(t), n))


if __name__ == '__main__':
    main()
