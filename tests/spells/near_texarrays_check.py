"""Lane NEAR2 (2026-09-26): gate A -- the near library's Texture2DArray sidecar, `<ws>.near.texarrays.bin`.

The layout is in src/nearlib.h (TEXTURE ARRAYS). This reader shares no code with the writer. It checks:

  form     magic, version, header size, the four offsets tile the file with no gap, the FNV-1a 64 of every
           byte after the header, lodoIdentity equal to the .lodi's (so the sidecar names ITS library)
  pools    a D3D11 array's slices must agree on kind (2d/cube), DXGI, width, height and mips: every texture's
           OWN header, read here out of the bake's stack (`<ws>.near.resources.txt`, loose files and DX10 BA2s,
           near_texture_stack), equals its pool's five; layers <= 2048 (cube 341); a split pool's parts count
           up from 0; pools holding a slot-0 texture come before the others; within a pool the paths are sorted
           (layer = rank); every path is canonical (`textures\\...`, lower case, back slashes) and listed once
  slots    materialCount = the .lodo's; slot 0's pool/layer = the .lodo material's arraySet/layer; every slot
           agrees with textures.txt's `slot` line (path -> p<pool>:<layer>, or missing/unusable/-); a slot
           marked missing is missing from the stack too, and a slot with a layer resolves
  twin     `--twin <dir>`: a second bake of the same input -- the sidecar is byte-identical

`--sabotage` (each must turn the gate red):
  checksum    flip one byte after the header                         -> form
  pool-dxgi   pool 0's DXGI + 1                                      -> pools (headers disagree)
  swap-layer  swap the first two textures' layers in the first pool with two or more -> pools (order) + slots
  slot-off    material 0's slot 0 layer + 1                          -> slots

usage: near_texarrays_check.py --lib <dir> [--world Commonwealth] [--twin <dir>] [--sabotage K]
"""
import argparse
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import lodgen_native_decode as dec    # noqa: E402
import near_texture_stack as ts       # noqa: E402
import lodgen_loadorder_check as lo   # noqa: E402

BS = chr(92)
CAP = 2048
NONE = 0xFFFF


def fnv(b):
    h = 0xCBF29CE484222325
    for x in b:
        h = ((h ^ x) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--lib', required=True)
    ap.add_argument('--world', default='Commonwealth')
    ap.add_argument('--twin', default='')
    ap.add_argument('--sabotage', default='')
    ap.add_argument('--profile', default='E:/Projects/Fallout 4 Mods/profiles/Default')
    ap.add_argument('--mods', default='E:/Projects/Fallout 4 Mods/mods')
    ap.add_argument('--data', default='X:/Programs/Steam/steamapps/common/Fallout 4/Data')
    a = ap.parse_args()
    sab = a.sabotage
    stem = os.path.join(a.lib, a.world + '.near')
    B = bytearray(open(stem + '.texarrays.bin', 'rb').read())
    red = {'form': [], 'pools': [], 'slots': [], 'twin': []}

    (magic, ver, hb, npool, ntex, nmat, nslot, nstr, lodo_id, o_pool, o_tex, o_slot, o_str, _sum) = \
        struct.unpack_from('<4sIIIIIIIQIIIIQ', B, 0)
    if sab == 'checksum':
        B[hb + 3] ^= 0x01
    # the sabotages that edit records (read the offsets first)
    if sab == 'pool-dxgi':
        B[o_pool + 1] = (B[o_pool + 1] + 1) & 0xFF
    pools = [struct.unpack_from('<BBBBHHIHH', B, o_pool + i * 16) for i in range(npool)]
    if sab == 'swap-layer':
        p = next(i for i, q in enumerate(pools) if q[7] >= 2)
        t0 = pools[p][6]
        l0 = struct.unpack_from('<H', B, o_tex + t0 * 8 + 6)[0]
        l1 = struct.unpack_from('<H', B, o_tex + (t0 + 1) * 8 + 6)[0]
        struct.pack_into('<H', B, o_tex + t0 * 8 + 6, l1)
        struct.pack_into('<H', B, o_tex + (t0 + 1) * 8 + 6, l0)
    if sab == 'slot-off':
        v = struct.unpack_from('<I', B, o_slot)[0]
        struct.pack_into('<I', B, o_slot, (v & 0xFFFF0000) | ((v + 1) & 0xFFFF))
    texs = [struct.unpack_from('<IHH', B, o_tex + i * 8) for i in range(ntex)]
    slots = [struct.unpack_from('<I', B, o_slot + i * 4)[0] for i in range(nmat * nslot)]

    # ---- form
    F = red['form']
    if magic != b'NTXA' or ver != 1 or hb != 64:
        F.append('magic %r version %d header %d' % (magic, ver, hb))
    if (o_pool, o_tex, o_slot, o_str) != (hb, hb + 16 * npool, hb + 16 * npool + 8 * ntex,
                                          hb + 16 * npool + 8 * ntex + 4 * nmat * nslot) or o_str + nstr != len(B):
        F.append('offsets do not tile the file')
    if fnv(bytes(B[hb:])) != _sum:
        F.append('the FNV-1a 64 of the body disagrees with the header')
    L = dec.read_lodo(stem + '.lodo')
    T = dec.read_lodi(stem + '.lodi')
    if lodo_id != T['header']['lodoIdentity'] or lodo_id != dec.lodo_identity(L['header']):
        F.append('lodoIdentity %016x names another library (.lodi %016x)' % (lodo_id, T['header']['lodoIdentity']))

    def path(i):
        o = o_str + texs[i][0]
        return bytes(B[o:B.index(0, o)]).decode('utf-8')

    # ---- pools
    P = red['pools']
    res = [ln.strip() for ln in open(stem + '.resources.txt', encoding='utf-8') if ln.strip() and not ln.startswith('#')]
    pl = lo.expected(a.profile, a.mods, a.data)[0]
    S = ts.TexStack(res, 'bake', plugins=pl)
    seen = {}
    diffuse_pools = {slots[m * nslot] >> 16 for m in range(nmat) if slots[m * nslot] >> 16 != NONE}
    last_diffuse = -1
    first_other = npool
    parts = {}
    for pi, (kind, dxgi, mips, _pad, w, h, first, layers, part) in enumerate(pools):
        key = (kind, dxgi, w, h, mips)
        cap = CAP // 6 if kind == 1 else CAP
        if layers > cap or layers == 0:
            P.append('pool %d holds %d layers (cap %d)' % (pi, layers, cap))
        if part != parts.get(key, -1) + 1:
            P.append('pool %d part %d follows part %d' % (pi, part, parts.get(key, -1)))
        parts[key] = part
        if pi in diffuse_pools:
            last_diffuse = pi
        else:
            first_other = min(first_other, pi)
        prev = None
        for li in range(layers):
            ti = first + li
            if ti >= ntex:
                P.append('pool %d runs past the texture table' % pi)
                break
            p = path(ti)
            if texs[ti][1] != pi or texs[ti][2] != li:
                P.append('texture %d (%s) says pool %d layer %d, sits at pool %d layer %d'
                         % (ti, p, texs[ti][1], texs[ti][2], pi, li))
            if prev is not None and not prev < p:
                P.append('pool %d: %s after %s (layers are sorted paths)' % (pi, p, prev))
            prev = p
            if p != p.lower() or '/' in p or not p.startswith('textures' + BS):
                P.append('not canonical: %s' % p)
            if p in seen:
                P.append('%s listed twice' % p)
            seen[p] = (pi, li)
            hd = S.header(p)
            if hd is None:
                P.append('%s: in a pool, not in the stack' % p)
            elif (hd[0], hd[1], hd[2], hd[3], int(hd[4])) != (dxgi, w, h, mips, kind):
                P.append('%s: header dxgi %d %dx%d mips %d cube %d, pool %d says dxgi %d %dx%d mips %d cube %d'
                         % (p, hd[0], hd[1], hd[2], hd[3], hd[4], pi, dxgi, w, h, mips, kind))
    if last_diffuse > first_other:
        P.append('pool %d holds a diffuse after pool %d that holds none' % (last_diffuse, first_other))
    if len(seen) != ntex:
        P.append('%d textures, %d distinct paths' % (ntex, len(seen)))

    # ---- slots
    Q = red['slots']
    if nmat != len(L['materials']):
        Q.append('materialCount %d, the .lodo has %d' % (nmat, len(L['materials'])))
    for m, row in enumerate(L['materials'][:nmat]):
        v = slots[m * nslot]
        want = (v >> 16, v & 0xFFFF) if v >> 16 != NONE and v >> 16 <= 255 else (0, 0xFFFF)
        if (row['arraySet'], row['layer']) != want:
            Q.append('material %d: .lodo arraySet %d layer %d, slot 0 says %s'
                     % (m, row['arraySet'], row['layer'], want))
    cur = -1
    txt_slots = {}
    for ln in open(stem + '.textures.txt', encoding='utf-8'):
        if ln.startswith('mat '):
            cur = int(ln.split(' ')[1])
        elif ln.startswith('  slot '):
            f = ln.strip().split(' ')
            txt_slots[(cur, int(f[1]))] = (' '.join(f[2:-1]), f[-1])
    n_layer = n_missing = n_empty = 0
    for m in range(nmat):
        for k in range(nslot):
            v = slots[m * nslot + k]
            pool, low = v >> 16, v & 0xFFFF
            p_txt, tag = txt_slots.get((m, k), ('-', '-'))
            if pool != NONE:
                n_layer += 1
                ok = pool < npool and low < pools[pool][7]
                p_bin = path(pools[pool][6] + low) if ok else '?'
                if not ok or tag != 'p%d:%d' % (pool, low) or p_txt != p_bin:
                    Q.append('material %d slot %d: bin %s (%s), textures.txt %s %s' % (m, k, 'p%d:%d' % (pool, low),
                                                                                      p_bin, p_txt, tag))
            else:
                reason = {0: '-', 1: 'missing', 2: 'unusable'}.get(low, '?')
                if reason == '-':
                    n_empty += 1
                if reason != tag:
                    Q.append('material %d slot %d: bin %s, textures.txt %s %s' % (m, k, reason, p_txt, tag))
                if reason == 'missing':
                    n_missing += 1
                    if S.where(p_txt) is not None:
                        Q.append('material %d slot %d: marked missing, the stack has %s' % (m, k, p_txt))

    # ---- twin
    if a.twin:
        tw = open(os.path.join(a.twin, a.world + '.near.texarrays.bin'), 'rb').read()
        if tw != bytes(B):
            red['twin'].append('the twin bake differs (%d vs %d bytes)' % (len(tw), len(B)))

    print('near texarrays gate: %s -- %d pools, %d textures, %d materials x %d slots (%d with a layer, %d missing, '
          '%d empty), %d bytes' % (stem + '.texarrays.bin', npool, ntex, nmat, nslot, n_layer, n_missing, n_empty, len(B)))
    bad = 0
    for k in ('form', 'pools', 'slots', 'twin'):
        if k == 'twin' and not a.twin:
            print('  twin   SKIP (no --twin)')
            continue
        v = red[k]
        bad += bool(v)
        print('  %-6s %s%s' % (k, 'PASS' if not v else 'FAIL %d' % len(v), ''.join('\n      ' + x for x in v[:4])))
    print('RESULT %s%s' % ('PASS' if not bad else 'FAIL', ' [sabotage %s]' % sab if sab else ''))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
