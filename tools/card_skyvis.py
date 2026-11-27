#!/usr/bin/env python3
"""Card sky visibility and the `_rvaos` mask (lanes SKYVIS1127 / SKYVIS1127B, bungo 11-27).

  python card_skyvis.py bake --objects <ws Objects dir> --sheet 512x512 --kernel <card_skyvis_rays.exe>
         --encoder <card_mask_bc7.exe> --stage <dir> --work <dir> [--rays 256] [--layers 0,3] [--red const1]
  python card_skyvis.py pack --objects <dir> --gsaos <the _gsaos before> --sheet 512x512 --sky <a _gsaos whose
         A holds sky> --encoder <exe> --stage <dir> --work <dir>
  python card_skyvis.py gate --dds <_rvaos.dds> --orig <_gsaos before> --sky <sky sheet> --bins <bins.npz> [--d <_d>]

bake: for each layer of the card array (the `.lodm` beside the sheets) the source model's triangle soup
(tools/card_skyvis_assets.py) and the layer's `_d` coverage + `_n` normal/height go to the ray kernel
(tools/card_skyvis_rays.cpp), which returns sky visibility per covered texel. Uncovered texels are
flooded from their covered neighbours inside the frame and the mips are coverage-weighted 2x2 means (a
frame is a multiple of 16 texels, so no mip mixes two frames). finish() then writes, into --stage only:
`<set>_rvaos.DDS`, BC7 (tools/card_mask_bc7.cpp, the repo encoder) with R = roughness (255 - gloss on
legacy), G = sky, B = AO, A = subsurface, every mip encoded from the decoded `_gsaos` and the sky; and
the `.lodm` with the mask key and path -> `rvaos` / `_rvaos`, array.maskLayout "rvaos" and array.specular (one value a
layer: the coverage-weighted mean of the source G). Spec: docs/LODGEN_LODM_FORMAT.md 4.2.

gate: per-channel max / mean read-back error against the values meant (flags any max over 4 counts),
the header (BC7, same size / mips / slices), and the sky per layer: covered mean/min/max, the spread and
the inner-crown vs outer/top means (bins fixed at bake time from the surface points: see BINS).
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


# ---- the _rvaos sheet (BC7): R roughness, G sky visibility, B AO, A subsurface -----------------------

def sheet_mips(path, layer):
    """Every mip of one layer, decoded RGBA, padded to whole 4x4 blocks."""
    w, h, mips, fmt, _arr = dds_info(path)
    out = []
    with open(path, 'rb') as f:
        f.seek(HDR + layer * layer_span(w, h, mips))
        for mw, mh, size in mip_sizes(w, h, mips):
            out.append(decode(f.read(size), mw + (-mw) % 4, mh + (-mh) % 4, 7 if fmt in (98, 99) else 3))
    return out


def rvaos_layer(g_path, layer, sky):
    """The values meant for one layer: R = 255 - gloss, G = sky (list of padded uint8 mips), B AO, A subsurface."""
    out = []
    for m, s in zip(sheet_mips(g_path, layer), sky):
        v = m.copy()
        v[..., 0] = 255 - m[..., 0]
        v[..., 1] = s
        out.append(v)
    return out


def write_rvaos(g_path, sky_by_layer, dst, encoder, work):
    """BC7 copy of `g_path` with the rvaos channels; returns the per-channel error of the encode."""
    W, H, mips, fmt, nl = dds_info(g_path)
    meant = []
    for li in range(nl):
        meant += rvaos_layer(g_path, li, sky_by_layer[li])
    job = os.path.join(work, 'bc7_job.bin')
    out = os.path.join(work, 'bc7_out.bin')
    with open(job, 'wb') as f:
        f.write(struct.pack('<I', len(meant)))
        for v in meant:
            f.write(struct.pack('<II', v.shape[1], v.shape[0]))
            f.write(np.ascontiguousarray(v, np.uint8).tobytes())
    r = subprocess.run([encoder, job, out], capture_output=True, text=True)
    if r.returncode:
        raise SystemExit('bc7 encoder rc %d %s' % (r.returncode, r.stderr))
    hdr = bytearray(open(g_path, 'rb').read(HDR))
    struct.pack_into('<I', hdr, 128, 98)                      # DXGI_FORMAT_BC7_UNORM, the rest as the source
    body = open(out, 'rb').read()
    assert len(body) == nl * layer_span(W, H, mips), (len(body), nl * layer_span(W, H, mips))
    open(dst, 'wb').write(bytes(hdr) + body)
    return channel_error(dst, meant)


def channel_error(dst, meant):
    """Decode `dst` back; max and mean |got - meant| per channel over every mip of every layer."""
    W, H, mips, fmt, nl = dds_info(dst)
    mx = np.zeros(4, int)
    over = np.zeros(4, int)
    tot = np.zeros(4)
    cnt = 0
    k = 0
    for li in range(nl):
        for mw, mh, got in zip([s[0] for s in mip_sizes(W, H, mips)], [s[1] for s in mip_sizes(W, H, mips)],
                               sheet_mips(dst, li)):
            d = np.abs(got[:mh, :mw].astype(int) - meant[k][:mh, :mw].astype(int))
            mx = np.maximum(mx, d.reshape(-1, 4).max(0))
            tot += d.reshape(-1, 4).sum(0)
            over += (d.reshape(-1, 4) > 4).sum(0)
            cnt += mw * mh
            k += 1
    print('    texels over 4: R %d G %d B %d A %d of %d' % (*over, cnt))
    return mx, tot / max(cnt, 1)


def card_specular(g_path, d_path, layers):
    """Per layer: the coverage-weighted mean of the source G (specular, or metallic on pbr) at mip 0, 0..1."""
    out = []
    for li, L in enumerate(layers):
        g = mip0(g_path, li)[..., 1].astype(np.float64)
        cov = mip0(d_path, li)[..., 3].astype(np.float64)
        wgt = np.where(cov >= int(L.get('coverage', {}).get('test', 128)), cov, 0.0)
        out.append(round(float((g * wgt).sum() / wgt.sum()) / 255.0, 4) if wgt.sum() > 0 else 0.0)
    return out


def write_lodm(src, dst, specular):
    """The card .lodm with the mask under key `rvaos` -> `_rvaos`, array.maskLayout "rvaos" and array.specular per layer."""
    j = lodm(src)
    arr = j['array']
    assert len(specular) == len(arr['layers'])
    arr['maskLayout'] = 'rvaos'
    arr['specular'] = specular
    for fam in ('legacy', 'pbr'):
        t = j['textures'].get(fam, {})
        for key in ('gsaos', 'rmaos'):
            if key in t:
                t['rvaos'] = t.pop(key).replace('_gsaos.', '_rvaos.').replace('_rmaos.', '_rvaos.')
    body = json.dumps(j, separators=(',', ':'), sort_keys=True, ensure_ascii=False).encode('utf-8')
    head = open(src, 'rb').read(8)
    open(dst, 'wb').write(head + struct.pack('<I', len(body)) + body)


def report_error(name, mx, mean):
    flag = [c for c, v in zip('RGBA', mx) if v > 4]
    print('  bc7 %s max R %d G %d B %d A %d | mean R %.2f G %.2f B %.2f A %.2f%s'
          % (name, mx[0], mx[1], mx[2], mx[3], mean[0], mean[1], mean[2], mean[3],
             ' | OVER 4: ' + ','.join(flag) if flag else ''))
    return len(flag)


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
    os.makedirs(a.stage, exist_ok=True)
    os.makedirs(a.work, exist_ok=True)
    store = getattr(a, '_store', None)
    soups = {}
    want = [int(x) for x in a.layers.split(',')] if a.layers else range(nl)
    binsave = {}
    sky = {}
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
        sky[li] = [np.pad(np.clip(np.rint(v), 0, 255).astype(np.uint8), ((0, (-v.shape[0]) % 4), (0, (-v.shape[1]) % 4)),
                          mode='edge') for v in chain]
    np.savez_compressed(os.path.join(a.stage, os.path.basename(base) + '_rvaos.DDS.bins.npz'), **binsave)
    finish(a, base, sky)
    print('BAKE %s layers %d rays %d wall %.1f s' % (a.sheet, len(list(want)), a.rays, time.time() - t_all))


def finish(a, base, sky):
    """Write `<set>_rvaos.DDS` (BC7) and the `.lodm` that names it into --stage; layers without a sky value
    (a --layers run) read open sky (255)."""
    g_path, d_path = getattr(a, 'gsaos', '') or base + '_gsaos.DDS', base + '_d.DDS'
    W, H, mips, fmt, nl = dds_info(g_path)
    for li in range(nl):
        if li not in sky:
            sky[li] = [np.full((mh + (-mh) % 4, mw + (-mw) % 4), 255, np.uint8) for mw, mh, _s in mip_sizes(W, H, mips)]
    dst = os.path.join(a.stage, os.path.basename(base) + '_rvaos.DDS')
    mx, mean = write_rvaos(g_path, sky, dst, a.encoder, a.work)
    report_error(os.path.basename(dst), mx, mean)
    spec = card_specular(g_path, d_path, lodm(base + '.lodm')['array']['layers'])
    write_lodm(base + '.lodm', os.path.join(a.stage, os.path.basename(base) + '.lodm'), spec)
    print('  specular per layer', spec)
    print('WROTE', dst)


def pack(a):
    """The rvaos sheet from a sheet set that already has its sky (the 11-27 SKYVIS `_gsaos` A): no rays."""
    base = os.path.join(a.objects, a.ws + '.LodgenCards.' + a.sheet)
    W, H, mips, fmt, nl = dds_info(a.sky)
    sky = {li: [m[..., 3].copy() for m in sheet_mips(a.sky, li)] for li in range(nl)}
    finish(a, base, sky)


# ---- gate ---------------------------------------------------------------------------------------------

def gate(a):
    """--dds the `_rvaos` sheet, --orig the `_gsaos` it came from, --sky the sheet whose A held the sky."""
    W, H, mips, fmt, nl = dds_info(a.dds)
    oW, oH, omips, ofmt, onl = dds_info(a.orig)
    fails = 0
    size_ok = os.path.getsize(a.dds) == HDR + nl * layer_span(W, H, mips)
    hdr_ok = fmt == 98 and (W, H, mips, nl) == (oW, oH, omips, onl) and size_ok
    print('GATE %s dxgi %d (98 = BC7), %dx%d mips %d layers %d, source %dx%d mips %d layers %d, size ok %s'
          % (os.path.basename(a.dds), fmt, W, H, mips, nl, oW, oH, omips, onl, size_ok))
    fails += not hdr_ok
    meant = []
    for li in range(nl):
        sky = [m[..., 3] for m in sheet_mips(a.sky, li)]
        meant += rvaos_layer(a.orig, li, sky)
    mx, mean = channel_error(a.dds, meant)
    flags = report_error('read-back vs meant (R 255-gloss, G old A, B AO, A subsurface)', mx, mean)
    bins = np.load(a.bins)
    d_path = a.d or a.orig.replace('_gsaos', '_d')
    sheet = []
    for li in range(nl):
        key = 'layer%d' % li
        if key not in bins:
            continue
        G = sheet_mips(a.dds, li)[0][:H, :W, 1].astype(np.float32) / 255.0
        cov = mip0(d_path, li)[..., 3] >= 128
        b = bins[key]
        c = G[cov]
        sd = float(c.std()) if c.size else 0.0
        mi = float(G[b == BIN_INNER].mean()) if (b == BIN_INNER).any() else float('nan')
        mo = float(G[b == BIN_OUTER].mean()) if (b == BIN_OUTER).any() else float('nan')
        judged = (b == BIN_INNER).sum() >= 16 and (b == BIN_OUTER).sum() >= 16
        bad = sd < 0.03 or (judged and not mi < mo)
        fails += bad
        sheet.append(c)
        print('  layer %d sky (G) covered %d mean %.3f min %.3f max %.3f sd %.3f | inner %d %.3f outer %d %.3f %s'
              % (li, c.size, c.mean(), c.min(), c.max(), sd, (b == BIN_INNER).sum(), mi, (b == BIN_OUTER).sum(), mo,
                 ('FAIL' if bad else 'ok') + ('' if judged else ' (inner/outer not judged: too few)')))
    allc = np.concatenate(sheet) if sheet else np.zeros(1)
    print('SHEET %s covered %d sky mean %.3f min %.3f max %.3f layers %d fails %d flags %d'
          % (os.path.basename(a.dds), allc.size, allc.mean(), allc.min(), allc.max(), len(sheet), fails, flags))
    print('RESULT', 'PASS' if fails == 0 else 'FAIL', '(error-bar flags %d)' % flags)
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
    b.add_argument('--encoder', required=True, help='card_mask_bc7.exe (tools/card_mask_bc7.cpp)')
    k = sp.add_parser('pack')
    k.add_argument('--objects', required=True, help='the card set: .lodm, _d')
    k.add_argument('--gsaos', required=True, help='the _gsaos before any sky pass')
    k.add_argument('--ws', default='Commonwealth')
    k.add_argument('--sheet', required=True)
    k.add_argument('--sky', required=True, help='a _gsaos whose A holds sky visibility')
    k.add_argument('--stage', required=True)
    k.add_argument('--work', required=True)
    k.add_argument('--encoder', required=True)
    g = sp.add_parser('gate')
    g.add_argument('--dds', required=True)
    g.add_argument('--orig', required=True)
    g.add_argument('--sky', required=True)
    g.add_argument('--bins', required=True)
    g.add_argument('--d', default='')
    a = ap.parse_args()
    if a.cmd == 'gate':
        return gate(a)
    if a.cmd == 'pack':
        os.makedirs(a.stage, exist_ok=True)
        os.makedirs(a.work, exist_ok=True)
        return pack(a)
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
