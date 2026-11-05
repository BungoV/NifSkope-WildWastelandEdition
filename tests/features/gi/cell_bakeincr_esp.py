"""Lane BAKEINCR1 (2026-10-04): the incremental probe bake gate's EDITS, written at gate time.

Nothing here is game data in the repo: the plugins and the model override are generated from the
user's own master and unpacked data into a scratch folder, and never committed.

usage: cell_bakeincr_esp.py <Fallout4.esm> <unpacked Data dir> <out dir> <cx,cy> <x0,y0,x1,y1>

Writes into <out dir>:
  bakeincr_ref.esp    one temporary STAT reference of cell (cx,cy) moved +512 units in X -- the
                      one-object edit (the base with the biggest model in the cell, so it shows)
  bakeincr_light.esp  one model-less LIGH reference of (cx,cy) moved +256 X, and its LIGH base's
                      radius doubled and colour changed -- a light edit, which must dirty nothing
  res/Meshes/<M>      a loose override of model M (another, bigger model's bytes under M's name):
                      M is a STAT model placed by exactly ONE reference in the region's 2-cell ring,
                      in a corner cell, so it reaches a few cells only
  lodl/...            nothing (the LOD leg is set up by the gate itself)
  facts.txt           key value lines: ref, light, light_base, model, model_ref, model_cell,
                      model_expect (the region cells whose 5x5 block holds that reference),
                      big_model (the replacement's source)

Reads the master with mmap (bounded memory); runs on Windows.
"""
import mmap
import os
import struct
import sys
import zlib

REC_HDR = 24
COMPRESSED = 0x00040000
DELETED = 0x20
DISABLED = 0x800
MODEL_TYPES = (b'STAT', b'MSTT', b'FURN', b'ACTI', b'CONT', b'TREE', b'FLOR', b'TERM', b'LIGH', b'DOOR',
               b'MISC', b'SCOL', b'BNDS', b'IDLM', b'TXST', b'ADDN', b'MOVS', b'PKIN', b'NOTE', b'BOOK',
               b'WEAP', b'ARMO', b'ALCH', b'AMMO', b'KEYM', b'TACT', b'HAZD', b'ARTO')


def subrecords(data):
    out, i, big = [], 0, None
    while i + 6 <= len(data):
        t = data[i:i + 4]
        n = struct.unpack_from('<H', data, i + 4)[0]
        i += 6
        if t == b'XXXX':
            big = struct.unpack_from('<I', data, i)[0]
            i += n
            continue
        if big is not None:
            n, big = big, None
        out.append((t, data[i:i + n]))
        i += n
    return out


def rec_data(m, off):
    size, flags = struct.unpack_from('<II', m, off + 4)
    d = m[off + REC_HDR:off + REC_HDR + size]
    if flags & COMPRESSED:
        d = zlib.decompress(d[4:])
    return d


def zstr(b):
    return b.split(b'\0', 1)[0].decode('latin-1')


def walk_group(m, off, end, cb, chain):
    """Calls cb(kind, offset, chain) for each record; chain = the enclosing group header offsets."""
    while off < end:
        t = m[off:off + 4]
        size = struct.unpack_from('<I', m, off + 4)[0]
        if t == b'GRUP':
            walk_group(m, off + REC_HDR, off + size, cb, chain + [off])
            off += size
        else:
            cb(t, off, chain)
            off += REC_HDR + size


def main():
    if len(sys.argv) != 6:
        print(__doc__)
        return 2
    esm, unpacked, out = sys.argv[1], sys.argv[2], sys.argv[3]
    cx, cy = (int(v) for v in sys.argv[4].split(','))
    x0, y0, x1, y1 = (int(v) for v in sys.argv[5].split(','))
    x0, x1, y0, y1 = min(x0, x1), max(x0, x1), min(y0, y1), max(y0, y1)
    os.makedirs(out, exist_ok=True)
    f = open(esm, 'rb')
    m = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
    assert m[:4] == b'TES4'
    tes4_size = struct.unpack_from('<I', m, 4)[0]
    hedr_ver = 1.0
    for t, d in subrecords(m[REC_HDR:REC_HDR + tes4_size]):
        if t == b'HEDR':
            hedr_ver = struct.unpack_from('<f', d, 0)[0]

    models, types, scol, lightdata, lightoff = {}, {}, {}, {}, {}
    wrld = {}
    top = REC_HDR + tes4_size
    off = top
    while off < len(m):
        size = struct.unpack_from('<I', m, off + 4)[0]
        label = m[off + 8:off + 12]
        if label in MODEL_TYPES:
            def cb(t, ro, chain):
                fid = struct.unpack_from('<I', m, ro + 12)[0]
                types[fid] = t
                d = rec_data(m, ro)
                parts = []
                for st, sd in subrecords(d):
                    if st == b'MODL':
                        models[fid] = zstr(sd).replace('/', '\\').lower()
                    elif st == b'ONAM' and t == b'SCOL' and len(sd) >= 4:
                        parts.append(struct.unpack_from('<I', sd, 0)[0])
                    elif st == b'DATA' and t == b'LIGH':
                        lightdata[fid] = sd
                if t == b'SCOL':
                    scol[fid] = parts
                if t == b'LIGH':
                    lightoff[fid] = ro
            walk_group(m, off + REC_HDR, off + size, cb, [off])
        elif label == b'WRLD':
            wrld = {'top': off, 'end': off + size}
        off += size

    # the Commonwealth (0x3C): its record, then its children group
    W = 0x0000003C
    refs = {}      # (cx, cy) -> list of (fid, base, pos, offset, chain, flags)
    cell_of = {}   # cell record offset -> (x, y) or None (persistent)
    cell_rec = {}
    wrec = None
    state = {'cell': None}

    def wcb(t, ro, chain):
        fid = struct.unpack_from('<I', m, ro + 12)[0]
        if t == b'WRLD':
            if fid == W:
                state['wrec'] = ro
            return
        if t == b'CELL':
            xy = None
            for st, sd in subrecords(rec_data(m, ro)):
                if st == b'XCLC':
                    xy = struct.unpack_from('<ii', sd, 0)
            state['cell'] = (ro, xy, list(chain))
            return
        if t != b'REFR':
            return
        flags = struct.unpack_from('<I', m, ro + 8)[0]
        base, pos = None, None
        for st, sd in subrecords(rec_data(m, ro)):
            if st == b'NAME':
                base = struct.unpack_from('<I', sd, 0)[0]
            elif st == b'DATA':
                pos = struct.unpack_from('<6f', sd, 0)
        if base is None or pos is None:
            return
        crec, xy, cchain = state['cell']
        persistent = xy is None
        if persistent:
            key = (int(pos[0] // 4096), int(pos[1] // 4096))
        else:
            key = tuple(xy)
        refs.setdefault(key, []).append(dict(fid=fid, base=base, pos=pos, off=ro, chain=list(chain), flags=flags,
                                             persistent=persistent, cell=crec, cchain=cchain))

    # walk only the Commonwealth's world children (label == W, group type 1)
    off = wrld['top'] + REC_HDR
    while off < wrld['end']:
        t = m[off:off + 4]
        size = struct.unpack_from('<I', m, off + 4)[0]
        if t == b'GRUP':
            label, gtype = struct.unpack_from('<II', m, off + 8)
            if gtype == 1 and label == W:
                walk_group(m, off + REC_HDR, off + size, wcb, [wrld['top'], off])
            off += size
        else:
            if struct.unpack_from('<I', m, off + 12)[0] == W:
                state['wrec'] = off
            off += REC_HDR + size
    wrec = state['wrec']

    def ref_models(r):
        b = r['base']
        ms = set()
        if b in models:
            ms.add(models[b])
        for p in scol.get(b, []):
            if p in models:
                ms.add(models[p])
        return ms

    def size_of(model):
        p = os.path.join(unpacked, 'Meshes', model)
        return os.path.getsize(p) if os.path.isfile(p) else -1

    live = lambda r: not (r['flags'] & (DELETED | DISABLED))
    # --- the one-object edit: the centre cell's temporary STAT with the biggest model
    centre = [r for r in refs.get((cx, cy), []) if not r['persistent'] and live(r) and types.get(r['base']) == b'STAT'
              and r['base'] in models and not (struct.unpack_from('<I', m, r['off'] + 8)[0] & COMPRESSED)]
    centre.sort(key=lambda r: (-size_of(models[r['base']]), r['fid']))
    moved = centre[0]
    # --- the light edit: a model-less LIGH in the centre cell
    lights = [r for r in refs.get((cx, cy), []) if not r['persistent'] and live(r) and types.get(r['base']) == b'LIGH'
              and r['base'] not in models and r['base'] in lightoff
              and not (struct.unpack_from('<I', m, r['off'] + 8)[0] & COMPRESSED)
              and not (struct.unpack_from('<I', m, lightoff[r['base']] + 8)[0] & COMPRESSED)]
    lights.sort(key=lambda r: r['fid'])
    light = lights[0] if lights else None

    # --- the model leg: a STAT model placed exactly once in the ring, in a corner cell of the region
    use = {}
    for (x, y), rs in refs.items():
        if x0 - 2 <= x <= x1 + 2 and y0 - 2 <= y <= y1 + 2:
            for r in rs:
                for mdl in ref_models(r):
                    use.setdefault(mdl, []).append((x, y, r))
    corners = [(x1, y1), (x0, y1), (x1, y0), (x0, y0)]
    cand = []
    for mdl, us in use.items():
        if len(us) != 1:
            continue
        x, y, r = us[0]
        if (x, y) in corners and types.get(r['base']) == b'STAT' and live(r) and size_of(mdl) > 0:
            cand.append((corners.index((x, y)), -size_of(mdl), mdl, x, y, r))
    cand.sort(key=lambda c: c[:3])
    _, _, mdl, mx, my, mref = cand[0]
    expect = [(x, y) for y in range(y0, y1 + 1) for x in range(x0, x1 + 1) if abs(x - mx) <= 2 and abs(y - my) <= 2]
    big = moved and models[moved['base']]
    res = os.path.join(out, 'res', 'Meshes', *mdl.split('\\'))
    os.makedirs(os.path.dirname(res), exist_ok=True)
    with open(os.path.join(unpacked, 'Meshes', *big.split('\\')), 'rb') as s, open(res, 'wb') as d:
        d.write(s.read())

    # --- writing plugins
    def rec_bytes(ro, new_data=None):
        size = struct.unpack_from('<I', m, ro + 4)[0]
        hdr = bytearray(m[ro:ro + REC_HDR])
        if new_data is None:
            return bytes(hdr) + m[ro + REC_HDR:ro + REC_HDR + size]
        struct.pack_into('<I', hdr, 4, len(new_data))
        return bytes(hdr) + new_data

    def grup(hoff, body):
        h = bytearray(m[hoff:hoff + REC_HDR])
        struct.pack_into('<I', h, 4, REC_HDR + len(body))
        return bytes(h) + body

    def with_sub(data, tag, fn):
        outb, i = bytearray(), 0
        while i + 6 <= len(data):
            t = data[i:i + 4]
            n = struct.unpack_from('<H', data, i + 4)[0]
            body = data[i + 6:i + 6 + n]
            if t == tag:
                body = fn(bytearray(body))
            outb += t + struct.pack('<H', len(body)) + bytes(body)
            i += 6 + n
        return bytes(outb)

    def ref_override(r, dx):
        def mv(b):
            struct.pack_into('<f', b, 0, struct.unpack_from('<f', b, 0)[0] + dx)
            return b
        new = with_sub(rec_data(m, r['off']), b'DATA', mv)
        rb = rec_bytes(r['off'], new)
        # chain: [WRLD top, world children, block, sub-block, cell children, temporary]
        ch = r['chain']
        inner = grup(ch[-1], rb)              # temporary
        inner = grup(ch[-2], inner)           # cell children
        cell = rec_bytes(r['cell']) + inner
        sub = grup(ch[-3], cell)              # sub-block
        blk = grup(ch[-4], sub)               # block
        wch = grup(ch[1], blk)                # world children
        return grup(ch[0], rec_bytes(wrec) + wch), 4   # WRLD, CELL, REFR (+TES4 counted apart)

    def plugin(path, groups, nrec):
        hedr = struct.pack('<fII', hedr_ver, nrec, 0x800)
        subs = b''.join(t + struct.pack('<H', len(d)) + d for t, d in (
            (b'HEDR', hedr), (b'CNAM', b'bakeincr\0'), (b'MAST', b'Fallout4.esm\0'), (b'DATA', b'\0' * 8)))
        tes4 = bytearray(m[0:REC_HDR])
        struct.pack_into('<II', tes4, 4, len(subs), 0)   # size, flags: a plain plugin
        with open(path, 'wb') as fo:
            fo.write(bytes(tes4) + subs + b''.join(groups))

    g, n = ref_override(moved, 512.0)
    plugin(os.path.join(out, 'bakeincr_ref.esp'), [g], 3)
    facts = ['ref %08X' % moved['fid'], 'ref_model %s' % models[moved['base']]]
    if light:
        g, n = ref_override(light, 256.0)

        def relight(b):
            struct.pack_into('<I', b, 4, struct.unpack_from('<I', b, 4)[0] * 2)
            b[8], b[9], b[10] = 255 - b[8], b[9] // 2, 255 - b[10]
            return b
        lo = lightoff[light['base']]
        lrec = rec_bytes(lo, with_sub(rec_data(m, lo), b'DATA', relight))
        # the LIGH top group's header: rebuilt (label LIGH, type 0)
        lg = b'GRUP' + struct.pack('<I', REC_HDR + len(lrec)) + b'LIGH' + struct.pack('<IHHHH', 0, 0, 0, 0, 0) + lrec
        plugin(os.path.join(out, 'bakeincr_light.esp'), [lg, g], 4)
        facts += ['light %08X' % light['fid'], 'light_base %08X' % light['base']]
    else:
        facts += ['light none']
    facts += ['model %s' % mdl, 'model_ref %08X' % mref['fid'], 'model_cell %d,%d' % (mx, my),
              'model_expect %s' % ' '.join('%d,%d' % c for c in expect), 'big_model %s' % big]
    with open(os.path.join(out, 'facts.txt'), 'w') as fo:
        fo.write('\n'.join(facts) + '\n')
    print('esp: ' + '; '.join(facts))
    return 0


if __name__ == '__main__':
    sys.exit(main())
