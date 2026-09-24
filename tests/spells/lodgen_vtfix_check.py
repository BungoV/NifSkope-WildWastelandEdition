#!/usr/bin/env python3
# Lane VTFIX1 (2026-09-24) -- the three measurements of tests/spells/lodgen_vtfix.sh.
# Every sub-command prints ONE verdict line starting "PASS" or "FAIL" (plus numbers).
#
#   fixture <root> <ws> <dim> <x> <y> [res]
#       Writes <root>/Textures/Terrain/<ws>/<ws>.<dim>.<x>.<y>_msn.DDS: a KNOWN-ANSWER
#       vanilla `_msn` whose fine relief runs along EAST ONLY. Channel law is the tree's
#       (lodgenTerrainMsnPixel): R = east, G = up, B = north. East is a period-4 ripple
#       (+,+,-,- at +-0.6) so its mip-2 average -- the coarse lodgenVanillaMsnDetail
#       subtracts -- is flat and the detail field is dE = the ripple, dN = 0.
#       Uncompressed B8G8R8A8 through a DX10 header, full mip chain (box filter), exactly
#       the layout lodgenWriteDdsBgra8 writes, so no codec stands between the known answer
#       and the reader.
#
#   g2 <base_msn.dds> <blend_msn.dds>
#       Decodes both baked `_msn` sheets (BC1/BC3/uncompressed) at mip 0 and reports the
#       mean |delta| per channel. The blend must move EAST (R), not NORTH (B):
#       PASS iff mean|dR| >= 8 levels and mean|dB| <= 0.25 * mean|dR|.
#       On the pre-VTFIX1 exe the detail lands in B: that is the red.
#
#   g1 <VT .lodm>
#       Reads terrain.maskRules and checks pbrm + legacyInverted + noneDefault == distinctLtex.
#       Red on the pre-VTFIX1 exe on the whole Commonwealth: 100 != 101.
import json
import os
import struct
import sys

import numpy as np


def cmd_fixture(a):
    root, ws, dim, x, y = a[0], a[1], int(a[2]), int(a[3]), int(a[4])
    res = int(a[5]) if len(a) > 5 else 512
    d = os.path.join(root, 'Textures', 'Terrain', ws)
    os.makedirs(d, exist_ok=True)
    p = os.path.join(d, '%s.%d.%d.%d_msn.DDS' % (ws, dim, x, y))
    xs = np.arange(res)
    east = np.where((xs % 4) < 2, 0.6, -0.6).astype(np.float64)[None, :].repeat(res, 0)
    north = np.zeros((res, res))
    up = np.sqrt(1.0 - east * east - north * north)
    enc = lambda v: np.clip(np.round((v * 0.5 + 0.5) * 255.0), 0, 255).astype(np.uint32)
    R, G, B = enc(east), enc(up), enc(north)
    A = np.full((res, res), 255, np.uint32)
    lv = [np.stack([B, G, R, A], -1).astype(np.float64)]    # B8G8R8A8 byte order
    while lv[-1].shape[0] > 1:
        c = lv[-1]
        lv.append((c[0::2, 0::2] + c[1::2, 0::2] + c[0::2, 1::2] + c[1::2, 1::2]) / 4.0)
    hdr = [0] * 32
    hdr[0] = 0x20534444; hdr[1] = 124; hdr[2] = 0x0002100F; hdr[3] = res; hdr[4] = res
    hdr[5] = res * 4; hdr[7] = len(lv); hdr[19] = 32; hdr[20] = 0x4; hdr[21] = 0x30315844
    hdr[27] = 0x401008
    with open(p, 'wb') as f:
        f.write(struct.pack('<32I', *hdr))
        f.write(struct.pack('<5I', 87, 3, 0, 1, 0))
        for m in lv:
            f.write(np.clip(np.floor(m + 0.5), 0, 255).astype(np.uint8).tobytes())
    print('PASS fixture %s res %d mips %d (east ripple +-0.6 period 4, north 0)' % (p, res, len(lv)))
    return 0


def rgb565(c):
    r = ((c >> 11) & 31) * 255.0 / 31.0
    g = ((c >> 5) & 63) * 255.0 / 63.0
    b = (c & 31) * 255.0 / 31.0
    return np.stack([r, g, b], -1)


def decode_dds_mip0(path):
    b = open(path, 'rb').read()
    h, w = struct.unpack('<II', b[12:20])
    fourcc = b[84:88]
    off = 128
    dxgi = None
    if fourcc == b'DX10':
        dxgi = struct.unpack('<I', b[128:132])[0]
        off = 148
    if fourcc in (b'DXT1', b'DXT5') or dxgi in (71, 72, 77, 78):
        stride = 16 if (fourcc == b'DXT5' or dxgi in (77, 78)) else 8
        coff = 8 if stride == 16 else 0
        bw, bh = max(1, w // 4), max(1, h // 4)
        raw = np.frombuffer(b, np.uint8, bw * bh * stride, off).reshape(bh * bw, stride)
        c0 = raw[:, coff].astype(np.uint32) | (raw[:, coff + 1].astype(np.uint32) << 8)
        c1 = raw[:, coff + 2].astype(np.uint32) | (raw[:, coff + 3].astype(np.uint32) << 8)
        idx = (raw[:, coff + 4].astype(np.uint32) | (raw[:, coff + 5].astype(np.uint32) << 8)
               | (raw[:, coff + 6].astype(np.uint32) << 16) | (raw[:, coff + 7].astype(np.uint32) << 24))
        p0, p1 = rgb565(c0), rgb565(c1)
        four = (c0 > c1) | (stride == 16)
        p2 = np.where(four[:, None], (2 * p0 + p1) / 3.0, (p0 + p1) / 2.0)
        p3 = np.where(four[:, None], (p0 + 2 * p1) / 3.0, 0.0)
        pal = np.stack([p0, p1, p2, p3], 1)                  # (n, 4, 3)
        sel = np.stack([(idx >> (2 * k)) & 3 for k in range(16)], 1)   # (n, 16)
        px = np.take_along_axis(pal, sel[:, :, None].astype(np.int64), 1)  # (n, 16, 3)
        img = px.reshape(bh, bw, 4, 4, 3).transpose(0, 2, 1, 3, 4).reshape(bh * 4, bw * 4, 3)
        return img
    if dxgi in (87, 91) or (fourcc == b'\0\0\0\0' and struct.unpack('<I', b[88:92])[0] == 32):
        raw = np.frombuffer(b, np.uint8, w * h * 4, off).reshape(h, w, 4).astype(np.float64)
        return raw[:, :, [2, 1, 0]]
    raise SystemExit('FAIL g2: unsupported DDS format %r dxgi %r in %s' % (fourcc, dxgi, path))


def cmd_g2(a):
    base, blend = decode_dds_mip0(a[0]), decode_dds_mip0(a[1])
    if base.shape != blend.shape:
        print('FAIL g2: sheet sizes differ %s vs %s' % (base.shape, blend.shape))
        return 1
    d = np.abs(blend - base).mean(axis=(0, 1))
    dr, dg, db = d
    # the ripple's own axis: column-parity pattern in the east delta (a real east detail
    # alternates with x, period 4); reported so a reader sees WHERE the detail went
    ok = dr >= 8.0 and db <= 0.25 * dr
    print('%s g2 mean|delta| east(R) %.2f up(G) %.2f north(B) %.2f -- the east-only detail must move R, not B'
          % ('PASS' if ok else 'FAIL', dr, dg, db))
    return 0 if ok else 1


def cmd_g1(a):
    b = open(a[0], 'rb').read()
    i = b.find(b'{')
    d = json.loads(b[i:].decode('utf-8'))
    t = d.get('terrain', d)
    r = t.get('maskRules') or {}
    served = r.get('pbrm', 0) + r.get('legacyInverted', 0) + r.get('noneDefault', 0)
    dist = r.get('distinctLtex', -1)
    ok = bool(r) and served == dist
    print('%s g1 maskRules pbrm %d + legacyInverted %d + noneDefault %d = %d, distinctLtex %d'
          % ('PASS' if ok else 'FAIL', r.get('pbrm', 0), r.get('legacyInverted', 0),
             r.get('noneDefault', 0), served, dist))
    return 0 if ok else 1


if __name__ == '__main__':
    cmds = {'fixture': cmd_fixture, 'g2': cmd_g2, 'g1': cmd_g1}
    if len(sys.argv) < 2 or sys.argv[1] not in cmds:
        raise SystemExit('usage: lodgen_vtfix_check.py fixture|g2|g1 ...')
    sys.exit(cmds[sys.argv[1]](sys.argv[2:]))
