# Lane SLAB1 -- WHICH CHANNEL MOVED in the chunk-level mask DDS.
#
# `Commonwealth.4.4.-12_data.DDS` is the STOCK composite's own mask texture --
# the object term's SECOND call site -- and it changes when the switch is on.
# A byte count ("453,824 of 699,176 bytes differ") says nothing about what
# moved, because BC1 stores endpoints, not channels. This decodes mip 0 of both
# bakes and compares the three channels separately.
#
# WHICH CHANNEL IS THE AO HERE: this file is NOT the VT mask sheet. lodgen.cpp
# packs it as `0xFF000000 | ( ao8 << 16 ) | ( wet8 << 8 ) | sho8`, so in the
# chunk composite the sky AO is **R**, wetness is G and shore is B -- where the
# role-5 `.lodt` mask sheet carries the AO in B. Only R may move.
#
# NEVER present a file byte as a pixel (brief rule): every number below is a
# DECODED channel value, 0..255, of a named texel.
import os, struct, sys, importlib.util

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
_spec = importlib.util.spec_from_file_location(
    'vtc', os.path.join(REPO, 'tests', 'spells', 'lodgen_vt_check.py'))
vtc = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(vtc)


def mip0(path):
    b = open(path, 'rb').read()
    assert b[:4] == b'DDS ', path
    h, w = struct.unpack_from('<2i', b, 12)     # dwHeight, dwWidth
    fourcc = b[84:88]
    assert fourcc == b'DXT1', fourcc
    off = 128
    return w, h, vtc.decode_bc1(b, off, w, h)


def main():
    a = os.path.join(HERE, 'before', 'vt', 'tex', 'Commonwealth.4.4.-12_data.DDS')
    c = os.path.join(HERE, 'after', 'vt', 'tex', 'Commonwealth.4.4.-12_data.DDS')
    w, h, A = mip0(a)
    w2, h2, C = mip0(c)
    assert (w, h) == (w2, h2)
    print('chunk mask DDS %dx%d DXT1, mip 0 decoded from both bakes' % (w, h))
    dr = dg = db = 0
    sr = sg = sb = 0
    mb = 0
    mbat = None
    for y in range(h):
        ra, rc = A[y], C[y]
        for x in range(w):
            p, q = ra[x], rc[x]
            if p[0] != q[0]:
                dr += 1; sr += q[0] - p[0]
            if p[1] != q[1]:
                dg += 1; sg += q[1] - p[1]
            if p[2] != q[2]:
                db += 1; sb += q[2] - p[2]
                if abs(q[2] - p[2]) > abs(mb):
                    mb = q[2] - p[2]; mbat = (x, y)
    n = w * h
    print('  R differs in %d of %d texels   (sum of differences %+d)' % (dr, n, sr))
    print('  G differs in %d of %d texels   (sum of differences %+d)' % (dg, n, sg))
    print('  B differs in %d of %d texels   (sum of differences %+d)' % (db, n, sb))
    if db:
        print('  B mean move over the texels that moved %+.3f; largest %+d at texel (%d,%d)'
              % (sb / float(db), mb, mbat[0], mbat[1]))
    bm_a = sum(sum(p[2] for p in row) for row in A) / float(n)
    bm_c = sum(sum(p[2] for p in row) for row in C) / float(n)
    print('  WHOLE-TEXTURE B mean  before %.3f  after %.3f  move %+.3f' % (bm_a, bm_c, bm_c - bm_a))
    rm_a = sum(sum(p[0] for p in row) for row in A) / float(n)
    rm_c = sum(sum(p[0] for p in row) for row in C) / float(n)
    gm_a = sum(sum(p[1] for p in row) for row in A) / float(n)
    gm_c = sum(sum(p[1] for p in row) for row in C) / float(n)
    print('  WHOLE-TEXTURE R mean  before %.3f  after %.3f' % (rm_a, rm_c))
    print('  WHOLE-TEXTURE G mean  before %.3f  after %.3f' % (gm_a, gm_c))


if __name__ == '__main__':
    main()
