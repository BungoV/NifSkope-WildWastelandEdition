#!/usr/bin/env python3
"""Card sky visibility into `_gsaos` A (lane SKYVIS1127, bungo 11-27).

  python card_skyvis.py bake --objects <ws Objects dir> --sheet 512x512 --kernel <card_skyvis_rays.exe>
         --stage <dir> --work <dir> [--rays 256] [--layers 0,3] [--red const1] [--mods <BNS dir> ...]
  python card_skyvis.py gate --dds <gsaos.dds> --orig <the sheet before> --bins <bins.npz> [--d <_d.dds>]

bake: for each layer of the card array (the `.lodm` beside the sheets) the source model's triangle soup
(tools/card_skyvis_assets.py) and the layer's `_d` coverage + `_n` normal/height go to the ray kernel
(tools/card_skyvis_rays.cpp), which returns sky visibility per covered texel. Uncovered texels are
flooded from their covered neighbours inside the frame, the mips are coverage-weighted 2x2 means (a
frame is a multiple of 16 texels, so no mip mixes two frames), and the values are encoded as BC3 alpha
blocks that replace ONLY bytes 0..7 of each 16-byte block of a copy of `_gsaos`: the header and every
colour block (R gloss, G spec, B AO) stay byte for byte. The copy lands in --stage; nothing in
--objects is written.

gate: reads A back from the written sheet and prints per layer covered mean/min/max, the spread and
the inner-crown vs outer/top means (bins fixed at bake time from the surface points: see BINS). It
fails on a flat layer, on inner >= outer, and on any changed header or colour byte.
"""
import argparse
import json
import os
import struct
import subprocess
import sys
import time

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

HDR = 148


def lodm(p):
    b = open(p, 'rb').read()
    i = b.find(b'{')
    return json.loads(b[i:].decode('utf-8').rstrip('\x00'))


def dds_info(p):
    h = open(p, 'rb').read(HDR)
    assert h[:4] == b'DDS ' and h[84:88] == b'DX10', p
    hh, ww = struct.unpack_from('<II', h, 12)
    mips = max(1, struct.unpack_from('<I', h, 28)[0])
    fmt, _dim, _misc, arr = struct.unpack_from('<IIII', h, 128)
    return ww, hh, mips, fmt, arr


def mip_sizes(w, h, mips):
    out = []
    for m in range(mips):
        mw, mh = max(1, w >> m), max(1, h >> m)
        out.append((mw, mh, ((mw + 3) // 4) * ((mh + 3) // 4) * 16))
    return out


def layer_span(w, h, mips):
    return sum(s for _w, _h, s in mip_sizes(w, h, mips))


def decode(raw, w, h, n):
    return np.asarray(Image.frombytes('RGBA', (w, h), raw, 'bcn', n))


def mip0(path, layer):
    w, h, mips, fmt, _arr = dds_info(path)
    span = layer_span(w, h, mips)
    with open(path, 'rb') as f:
        f.seek(HDR + layer * span)
        raw = f.read(mip_sizes(w, h, mips)[0][2])
    return decode(raw, w, h, 7 if fmt in (98, 99) else 3)


# ---- BC3 alpha blocks ------------------------------------------------------------------------------

def bc3_alpha_blocks(a):
    """a: uint8 (h, w), h and w multiples of 4 -> (nblocks, 8) uint8, 8-value mode (a0 > a1)."""
    h, w = a.shape
    blk = a.reshape(h // 4, 4, w // 4, 4).transpose(0, 2, 1, 3).reshape(-1, 16).astype(np.int32)
    a0 = blk.max(1)
    a1 = blk.min(1)
    flat = a0 == a1
    a1 = np.where(flat, np.maximum(a0 - 1, 0), a1)
    a0 = np.where(flat & (a0 == 0), 1, a0)
    # palette step s: 0 = a0 .. 7 = a1 (codes 0, 2, 3, 4, 5, 6, 7, 1)
    pal = np.stack([((7 - s) * a0 + s * a1 + 3) // 7 for s in range(8)], 1)        # (n, 8) by step
    d = np.abs(blk[:, :, None] - pal[:, None, :])
    s = d.argmin(2)
    code = np.where(s == 0, 0, np.where(s == 7, 1, s + 1)).astype(np.uint64)
    bits = np.zeros(len(blk), np.uint64)
    for t in range(16):
        bits |= code[:, t] << np.uint64(3 * t)
    out = np.zeros((len(blk), 8), np.uint8)
    out[:, 0] = a0
    out[:, 1] = a1
    for k in range(6):
        out[:, 2 + k] = ((bits >> np.uint64(8 * k)) & np.uint64(255)).astype(np.uint8)
    return out


# ---- flood + mips ----------------------------------------------------------------------------------

def flood(v, covered, fw, fh):
    """Fill uncovered texels from covered neighbours, never across a frame border."""
    from scipy import ndimage
    v = v.astype(np.float32).copy()
    H, W = v.shape
    for j in range(H // fh):
        for i in range(W // fw):
            sl = (slice(j * fh, (j + 1) * fh), slice(i * fw, (i + 1) * fw))
            c = covered[sl]
            if c.all():
                continue
            if not c.any():                # nothing covered in the frame: open sky
                v[sl] = 255.0
                continue
            _d, (iy, ix) = ndimage.distance_transform_edt(~c, return_indices=True)
            f = v[sl]
            v[sl] = f[iy, ix]              # each texel takes its nearest covered texel's value
    return v


def mip_chain(v0, w0, mips):
    """v0 flooded float (H, W); w0 coverage weight 0/1. Coverage-weighted 2x2 means."""
    out = [v0]
    v, w = v0, w0.astype(np.float32)
    for _m in range(1, mips):
        H, W = v.shape
        vb = v.reshape(H // 2, 2, W // 2, 2)
        wb = w.reshape(H // 2, 2, W // 2, 2)
        num = (vb * wb).sum((1, 3))
        den = wb.sum((1, 3))
        nv = np.where(den > 0, num / np.maximum(den, 1e-9), vb.mean((1, 3)))
        v, w = nv, (den > 0).astype(np.float32)
        out.append(v)
    return out


# ---- bins (fixed before the first run, lane SKYVIS1127) ----------------------------------------------
# Over a layer's covered texels with a primary-ray surface point: r = distance from the layer's vertical
# axis (centre x, y), z = height. R = 95th percentile of r, z0/z1 = 5th/95th percentile of z, zr = (z-z0)/(z1-z0).
#   inner = r <= 0.35 R and 0.35 <= zr <= 0.75      (inside the crown, above the trunk)
#   outer = r >= 0.75 R and zr >= 0.35, or zr >= 0.90 (the crown's skin and its top)
BIN_INNER, BIN_OUTER = 1, 2


def bins_of(pos, state, centre):
    b = np.zeros(state.shape, np.uint8)
    m = state == 1
    if m.sum() < 32:
        return b
    x, y, z = pos[..., 0], pos[..., 1], pos[..., 2]
    r = np.hypot(x - centre[0], y - centre[1])
    R = np.percentile(r[m], 95)
    z0, z1 = np.percentile(z[m], 5), np.percentile(z[m], 95)
    zr = (z - z0) / max(z1 - z0, 1e-6)
    inner = m & (r <= 0.35 * R) & (zr >= 0.35) & (zr <= 0.75)
    outer = m & (((r >= 0.75 * R) & (zr >= 0.35)) | (zr >= 0.90))
    b[inner] = BIN_INNER
    b[outer] = BIN_OUTER
    return b


# ---- bake ---------------------------------------------------------------------------------------------

def write_job(path, W, H, oct_n, conv, half, centre, span, off, cov_test, cov, nrm, tris, maps):
    with open(path, 'wb') as f:
        f.write(struct.pack('<I7i', 0x31564B53, W, H, oct_n, oct_n, oct_n, 0, conv))
        f.write(struct.pack('<2f3ff', half[0], half[1], centre[0], centre[1], centre[2], span))
        f.write(struct.pack('<%df' % len(off), *off))
        f.write(struct.pack('<i', cov_test))
        f.write(cov.astype(np.uint8).tobytes())
        f.write(nrm.astype(np.uint8).tobytes())
        f.write(struct.pack('<i', len(tris)))
        buf = bytearray()
        for p, m, thr, uv in tris:
            buf += struct.pack('<9fii6f', *p, m, thr, *uv)
        f.write(buf)
        f.write(struct.pack('<i', len(maps)))
        for w, h, a in maps:
            f.write(struct.pack('<ii', w, h))
            f.write(a)


def bake(a):
    import card_skyvis_assets as assets
    base = os.path.join(a.objects, a.ws + '.LodgenCards.' + a.sheet)
    j = lodm(base + '.lodm')
    arr = j['array']
    oct_n = int(arr['oct'])
    d_path, n_path = base + '_d.DDS', base + '_n.DDS'
    g_path = base + '_gsaos.DDS'
    W, H, mips, fmt, nl = dds_info(g_path)
    assert fmt == 77 and dds_info(d_path)[:2] == (W, H) and dds_info(n_path)[:2] == (W, H), 'sheet layout'
    assert nl == len(arr['layers']), (nl, len(arr['layers']))
    fw, fh = W // oct_n, H // oct_n
    span = layer_span(W, H, mips)
    data = bytearray(open(g_path, 'rb').read())
    os.makedirs(a.stage, exist_ok=True)
    os.makedirs(a.work, exist_ok=True)
    store = getattr(a, '_store', None)
    soups = {}
    want = [int(x) for x in a.layers.split(',')] if a.layers else range(nl)
    binsave = {}
    t_all = time.time()
    for li in want:
        L = arr['layers'][li]
        src = L['source']
        if src not in soups:
            hits = store.find_by_basename(src.lower())
            if not hits:
                raise SystemExit('source not found: ' + src)
            tris, maps, notes = assets.soup(store, hits[0][0])
            soups[src] = (tris, maps, hits[0])
            print('  soup %s <- %s (%s): %d tris, %d maps; %s' % (src, hits[0][0], os.path.basename(hits[0][1]),
                  len(tris), len(maps), '; '.join(n for n in notes if not n.startswith('shape'))))
        tris, maps, _hit = soups[src]
        d0 = mip0(d_path, li)
        n0 = mip0(n_path, li)
        test = int(L.get('coverage', {}).get('test', 128))
        cov = d0[..., 3]
        conv = 0 if L.get('conv') == 'legacyBake' else 1
        job = os.path.join(a.work, 'job.bin')
        out = os.path.join(a.work, 'out.bin')
        write_job(job, W, H, oct_n, conv, L['half'], L['center'], float(L['depthSpan']), L['frameOffset'], test,
                  cov, n0[..., :3], tris, maps)
        cmd = [a.kernel, job, out, str(a.rays)] + (['--red', a.red] if a.red else [])
        t0 = time.time()
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode:
            raise SystemExit('kernel rc %d %s' % (r.returncode, r.stderr))
        print('  layer %d %s: %s (wall %.1f s)' % (li, src, r.stdout.strip(), time.time() - t0))
        raw = open(out, 'rb').read()
        vis = np.frombuffer(raw, np.uint8, W * H).reshape(H, W)
        state = np.frombuffer(raw, np.uint8, W * H, W * H).reshape(H, W)
        pos = np.frombuffer(raw, np.float32, W * H * 3, 2 * W * H).reshape(H, W, 3)
        covered = state > 0
        binsave['layer%d' % li] = bins_of(pos, state, L['center'])
        v0 = flood(vis, covered, fw, fh)
        chain = mip_chain(v0, covered, mips)
        o = HDR + li * span
        for (mw, mh, size), v in zip(mip_sizes(W, H, mips), chain):
            q = np.clip(np.rint(v), 0, 255).astype(np.uint8)
            if mw % 4 or mh % 4:
                q = np.pad(q, ((0, (-mh) % 4), (0, (-mw) % 4)), mode='edge')
            blocks = bc3_alpha_blocks(q)
            view = np.frombuffer(data, np.uint8, size, o).reshape(-1, 16)   # read-only view of the copy
            patched = view.copy()
            patched[:, 0:8] = blocks
            data[o:o + size] = patched.tobytes()
            if mw == W:                    # the encode, read back: mip 0 alpha against the values meant
                got = decode(bytes(data[o:o + size]), W, H, 3)[..., 3].astype(int)
                err = np.abs(got - q[:H, :W].astype(int))
                print('    encode mip0: max |A - meant| %d, mean %.2f (of 255) over %d texels'
                      % (err.max(), err.mean(), err.size))
            o += size
    dst = os.path.join(a.stage, os.path.basename(g_path))
    open(dst, 'wb').write(bytes(data))
    np.savez_compressed(os.path.join(a.stage, os.path.basename(g_path) + '.bins.npz'), **binsave)
    print('BAKE %s layers %d rays %d wall %.1f s -> %s' % (a.sheet, len(list(want)), a.rays, time.time() - t_all, dst))


# ---- gate ---------------------------------------------------------------------------------------------

def gate(a):
    W, H, mips, fmt, nl = dds_info(a.dds)
    new = open(a.dds, 'rb').read()
    old = open(a.orig, 'rb').read()
    fails = 0
    hdr_same = new[:HDR] == old[:HDR] and len(new) == len(old)
    nb = (len(new) - HDR) // 16
    nv = np.frombuffer(new, np.uint8, nb * 16, HDR).reshape(-1, 16)
    ov = np.frombuffer(old, np.uint8, nb * 16, HDR).reshape(-1, 16)
    col_diff = int((nv[:, 8:] != ov[:, 8:]).any(1).sum())
    alpha_diff = int((nv[:, :8] != ov[:, :8]).any(1).sum())
    print('GATE %s header same %s, colour blocks differing %d of %d, alpha blocks rewritten %d'
          % (os.path.basename(a.dds), hdr_same, col_diff, nb, alpha_diff))
    if not hdr_same or col_diff:
        fails += 1
    bins = np.load(a.bins)
    d_path = a.d or a.orig.replace('_gsaos', '_d')
    span = layer_span(W, H, mips)
    sheet = []
    for li in range(nl):
        key = 'layer%d' % li
        if key not in bins:
            continue
        with open(a.dds, 'rb') as f:
            f.seek(HDR + li * span)
            A = decode(f.read(mip_sizes(W, H, mips)[0][2]), W, H, 3)[..., 3].astype(np.float32) / 255.0
        cov = mip0(d_path, li)[..., 3] >= 128
        b = bins[key]
        c = A[cov]
        sd = float(c.std()) if c.size else 0.0
        mi = float(A[b == BIN_INNER].mean()) if (b == BIN_INNER).any() else float('nan')
        mo = float(A[b == BIN_OUTER].mean()) if (b == BIN_OUTER).any() else float('nan')
        ok_flat = sd >= 0.03
        ok_io = (b == BIN_INNER).sum() >= 16 and (b == BIN_OUTER).sum() >= 16 and mi < mo
        # a layer with too few inner texels (a sparse model seen from outside) is reported, not judged
        judged = (b == BIN_INNER).sum() >= 16 and (b == BIN_OUTER).sum() >= 16
        bad = (not ok_flat) or (judged and not ok_io)
        fails += bad
        sheet.append(c)
        print('  layer %d covered %d mean %.3f min %.3f max %.3f sd %.3f | inner %d %.3f outer %d %.3f %s'
              % (li, c.size, c.mean(), c.min(), c.max(), sd, (b == BIN_INNER).sum(), mi, (b == BIN_OUTER).sum(), mo,
                 ('FAIL' if bad else 'ok') + ('' if judged else ' (inner/outer not judged: too few)')))
    allc = np.concatenate(sheet) if sheet else np.zeros(1)
    print('SHEET %s covered %d mean %.3f min %.3f max %.3f layers %d fails %d'
          % (os.path.basename(a.dds), allc.size, allc.mean(), allc.min(), allc.max(), len(sheet), fails))
    print('RESULT', 'PASS' if fails == 0 else 'FAIL')
    return 1 if fails else 0


def main():
    ap = argparse.ArgumentParser()
    sp = ap.add_subparsers(dest='cmd', required=True)
    b = sp.add_parser('bake')
    b.add_argument('--objects', required=True)
    b.add_argument('--ws', default='Commonwealth')
    b.add_argument('--sheet', required=True, help='comma list of WxH, or all')
    b.add_argument('--kernel', required=True)
    b.add_argument('--stage', required=True)
    b.add_argument('--work', required=True)
    b.add_argument('--rays', type=int, default=256)
    b.add_argument('--layers', default='')
    b.add_argument('--red', default='')
    b.add_argument('--data', default=r'X:/Programs/Steam/steamapps/common/Fallout 4/Data')
    b.add_argument('--mod-archive', action='append', default=[])
    g = sp.add_parser('gate')
    g.add_argument('--dds', required=True)
    g.add_argument('--orig', required=True)
    g.add_argument('--bins', required=True)
    g.add_argument('--d', default='')
    a = ap.parse_args()
    if a.cmd == 'gate':
        return gate(a)
    import card_skyvis_assets as assets
    t0 = time.time()
    a._store = assets.Store(a.data, mod_archives=a.mod_archive)
    print('archives %d indexed in %.1f s' % (len(a._store.archives), time.time() - t0))
    sheets = a.sheet.split(',')
    if sheets == ['all']:
        sheets = sorted(f.split('.LodgenCards.')[1][:-5] for f in os.listdir(a.objects)
                        if '.LodgenCards.' in f and f.startswith(a.ws + '.') and f.endswith('.lodm'))
    for s in sheets:
        a.sheet = s
        bake(a)
    return 0


if __name__ == '__main__':
    sys.exit(main())
