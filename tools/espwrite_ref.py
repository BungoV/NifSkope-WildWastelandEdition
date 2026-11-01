"""espwrite_ref.py -- the Python reference for NifSkope WW's plugin writer (src/io/espwrite.cpp).

Two jobs, kept independent of the C++ on purpose (no shared code, no shared tables):

  build  <plan.json> <out.esp> [--data <Data dir>]
      Build the same cell-edit plugin the C++ writer builds from the same plan. The gate compares the two files
      byte for byte.
  verify <file.esp> [--data <Data dir>] [--expect-fail <reason substring>]
      Walk a written plugin with every check on and print `N checks, 0 failures` (exit 0) or one FAIL line per
      failed check (exit 1). Labels, group order and persistence are re-derived from the MASTER's own grouping
      of the same records, never from the writer's formulas.

Layouts follow xEdit's published definitions (wbDefinitionsFO4.pas, read as documentation). Every rule below was
measured on Fallout4.esm and the shipped CC .esl files (notes/espwrite-r/DESIGN.md section 2):
  * interior block = obj % 10, sub-block = (obj / 10) % 10, obj = fid & 0xFFFFFF (1195/1195 cells);
  * exterior block = i16 (y >> 5), i16 (x >> 5); sub-block = i16 (y >> 3), i16 (x >> 3); Y first (38,965/38,965);
  * blocks and sub-blocks sort by their label read as a u32, cells by form id (Fallout4.esm, every group);
  * type 8 (persistent) before type 9 (temporary); exterior persistent refs only under the world's persistent cell;
  * HEDR count = records + groups; ONAM = master REFR/ACHR overridden in temporary groups; INCC = interior CELLs.

Plan JSON:
  {"masters": ["Fallout4.esm"], "light": true, "author": "NifSkope WW",
   "cells": [{"cell": "0005C92E", "world": null,
              "adds": [{"base": "00000034", "pos": [x, y, z], "rot": [rx, ry, rz], "scale": 1.0,
                        "persistent": true}],
              "disable": ["0005C9A0"]}]}
  Form ids are hex in the written file's numbering (high byte = master index). "world" names the worldspace of an
  exterior cell. "disable" copies the master's REFR and sets Initially Disabled (0x800).
"""
import json, os, pickle, struct, sys, zlib

DATA = r'X:\Programs\Steam\steamapps\common\Fallout 4\Data'
CACHE = os.environ.get('ESPWRITE_REF_CACHE', '')
TAIL_NEW = bytes.fromhex('0000000083000000')
STAMP = bytes(8)
F_COMPRESSED, F_LOCALIZED, F_LIGHT = 0x40000, 0x80, 0x200
F_PERSISTENT, F_DISABLED = 0x400, 0x800


# ------------------------------------------------------------------ a plugin's index
class Plugin:
    """fid -> (offset, record size incl. header, chain); chain = ((gtype, label u32, group offset), ...) outermost first."""

    def __init__(self, path):
        self.path = path
        self.buf = open(path, 'rb').read()
        b = self.buf
        if b[:4] != b'TES4':
            raise SystemExit('%s: not a plugin' % path)
        self.flags = struct.unpack_from('<I', b, 8)[0]
        self.tes4_end = 24 + struct.unpack_from('<I', b, 4)[0]
        self.tes4_subs = subs_of(b[24:self.tes4_end])
        self.masters = [v.rstrip(b'\0').decode('cp1252') for t, v in self.tes4_subs if t == 'MAST']
        self.recs = None

    def index(self):
        if self.recs is not None:
            return
        key = None
        if CACHE:
            st = os.stat(self.path)
            key = os.path.join(CACHE, '%s.%d.%d.idx.pkl' % (os.path.basename(self.path), st.st_size, int(st.st_mtime)))
            if os.path.exists(key):
                self.recs = pickle.load(open(key, 'rb'))
                return
        b = self.buf; recs = {}
        stack = []  # (end, chain)
        p = self.tes4_end; chain = ()
        n = len(b)
        unpack = struct.unpack_from
        while p < n:
            while stack and p >= stack[-1][0]:
                stack.pop()
            chain = stack[-1][1] if stack else ()
            if b[p:p + 4] == b'GRUP':
                size, label, gtype = unpack('<II I', b, p + 4)
                stack.append((p + size, chain + ((gtype, label, p),)))
                p += 24
                continue
            size, flags, fid = unpack('<III', b, p + 4)
            recs[fid] = (p, 24 + size, chain)
            p += 24 + size
        self.recs = recs
        if key:
            os.makedirs(CACHE, exist_ok=True)
            pickle.dump(recs, open(key, 'wb'), protocol=4)

    def raw(self, fid):
        self.index()
        p, n, _ = self.recs[fid]
        return self.buf[p:p + n]

    def chain(self, fid):
        self.index()
        return self.recs[fid][2]


def subs_of(payload):
    """(tag, bytes) pairs. XXXX (u32 size) carries the length of the next subrecord, whose own u16 is then 0."""
    out, p, big = [], 0, None
    while p < len(payload):
        t = payload[p:p + 4].decode('ascii', 'replace'); n = struct.unpack_from('<H', payload, p + 4)[0]
        if t == 'XXXX':
            big = struct.unpack_from('<I', payload, p + 6)[0]; p += 6 + n
            continue
        if big is not None:
            n = big; big = None
        out.append((t, payload[p + 6:p + 6 + n])); p += 6 + n
    if p != len(payload):
        raise ValueError('subrecords overrun the payload')
    return out


def payload_of(rec):
    size, flags = struct.unpack_from('<II', rec, 4)
    body = rec[24:24 + size]
    if flags & F_COMPRESSED:
        want = struct.unpack_from('<I', body, 0)[0]
        body = zlib.decompress(body[4:])
        assert len(body) == want
    return flags, body


def sub(t, v):
    if len(v) > 0xFFFF:
        return b'XXXX' + struct.pack('<HI', 4, len(v)) + t.encode('ascii') + struct.pack('<H', 0) + v
    return t.encode('ascii') + struct.pack('<H', len(v)) + v


def record(sig, flags, fid, payload, tail=TAIL_NEW):
    return struct.pack('<4sIII', sig.encode('ascii'), len(payload), flags, fid) + tail + payload


def group(label, gtype, body):
    return b'GRUP' + struct.pack('<I', 24 + len(body)) + label + struct.pack('<I', gtype) + STAMP + body


def u32(v):
    return struct.pack('<I', v)


def lab_u32(v):
    return struct.pack('<I', v & 0xFFFFFFFF)


def int_block(fid):
    return struct.pack('<i', (fid & 0xFFFFFF) % 10)


def int_sub(fid):
    return struct.pack('<i', ((fid & 0xFFFFFF) // 10) % 10)


def ext_block(x, y):
    return struct.pack('<hh', y >> 5, x >> 5)


def ext_sub(x, y):
    return struct.pack('<hh', y >> 3, x >> 3)


def cell_grid(rec):
    _, body = payload_of(rec)
    for t, v in subs_of(body):
        if t == 'XCLC':
            return struct.unpack_from('<ii', v, 0)
    raise ValueError('exterior CELL without XCLC')


def copy_for(rec, src_localized, ours_localized=False):
    """A copied CELL/WRLD: verbatim, unless a localised source's FULL (an lstring id) must leave a non-localised
    plugin: then decompress, drop FULL, write plain."""
    if not src_localized or ours_localized:
        return rec, False
    flags, body = payload_of(rec)
    ss = subs_of(body)
    if not any(t == 'FULL' for t, _ in ss):
        return rec, False
    body = b''.join(sub(t, v) for t, v in ss if t != 'FULL')
    fid = struct.unpack_from('<I', rec, 12)[0]
    return record(rec[:4].decode(), flags & ~F_COMPRESSED, fid, body, rec[16:24]), True


# ------------------------------------------------------------------ build
def build(plan, data):
    masters = plan['masters']
    if len(masters) != 1:
        raise SystemExit('reference build covers one master (the C++ refuses a non-prefix source the same way)')
    M = Plugin(os.path.join(data, masters[0])); M.index()
    src_loc = bool(M.flags & F_LOCALIZED)
    nm = len(masters); light = bool(plan.get('light'))
    local = 0x800; nrec = 0; ngrp = 0; onam = []; incc = 0
    interior = {}   # cell fid -> {'rec', 'pers': [], 'temp': []}
    worlds = {}     # world fid -> {'rec', 'pcell', 'pers': [], 'grid': {cell fid: {'rec','temp'}}}
    for c in plan['cells']:
        cf = int(c['cell'], 16)
        adds_p, adds_t = [], []
        for a in c.get('adds', []):
            fid = (nm << 24) | local
            if light and local > 0xFFF:
                raise SystemExit('light plugin full')
            local += 1
            ss = [('NAME', u32(int(a['base'], 16)))]
            if a.get('scale', 1.0) != 1.0:
                ss.append(('XSCL', struct.pack('<f', a['scale'])))
            ss.append(('DATA', struct.pack('<6f', *(list(a['pos']) + list(a['rot'])))))
            fl = F_PERSISTENT if a.get('persistent') else 0
            r = record('REFR', fl, fid, b''.join(sub(t, v) for t, v in ss))
            (adds_p if fl else adds_t).append(r)
        ov_p, ov_t = [], []
        for d in c.get('disable', []):
            df = int(d, 16)
            r = bytearray(M.raw(df)); fl = struct.unpack_from('<I', r, 8)[0] | F_DISABLED
            struct.pack_into('<I', r, 8, fl)
            pers = M.chain(df)[-1][0] == 8
            (ov_p if pers else ov_t).append(bytes(r))
            if not pers:
                onam.append(df)
        if c.get('world'):
            wf = int(c['world'], 16)
            W = worlds.setdefault(wf, {'rec': copy_for(M.raw(wf), src_loc)[0], 'pcell': None, 'pers': [], 'grid': {}})
            W['pers'] += adds_p + ov_p
            if adds_t or ov_t:
                g = W['grid'].setdefault(cf, {'rec': copy_for(M.raw(cf), src_loc)[0], 'temp': []})
                g['temp'] += adds_t + ov_t
        else:
            I = interior.setdefault(cf, {'rec': copy_for(M.raw(cf), src_loc)[0], 'pers': [], 'temp': []})
            I['pers'] += adds_p + ov_p; I['temp'] += adds_t + ov_t
    if os.environ.get('WW_ESPWRITE_RED') == 'temp':
        pass  # the reference never honours a red: it is the answer the C++ red must miss

    def children(cf, pers, temp):
        nonlocal nrec, ngrp
        body = b''
        if pers:
            body += group(u32(cf), 8, b''.join(pers)); ngrp += 1; nrec += len(pers)
        if temp:
            body += group(u32(cf), 9, b''.join(temp)); ngrp += 1; nrec += len(temp)
        ngrp += 1
        return group(u32(cf), 6, body)

    out = b''
    if interior:
        blocks = {}
        for cf in interior:
            blocks.setdefault(int_block(cf), {}).setdefault(int_sub(cf), []).append(cf)
        bb = b''
        for bl in sorted(blocks, key=lambda k: struct.unpack('<I', k)[0]):
            sb = b''
            for sl in sorted(blocks[bl], key=lambda k: struct.unpack('<I', k)[0]):
                cb = b''
                for cf in sorted(blocks[bl][sl]):
                    I = interior[cf]; cb += I['rec']; nrec += 1; incc += 1
                    cb += children(cf, I['pers'], I['temp'])
                sb += group(sl, 3, cb); ngrp += 1
            bb += group(bl, 2, sb); ngrp += 1
        out += group(b'CELL', 0, bb); ngrp += 1
    if worlds:
        wb = b''
        for wf in sorted(worlds):
            W = worlds[wf]; wb += W['rec']; nrec += 1
            kb = b''
            if W['pers']:
                pc = [f for f, (_, _, ch) in M.recs.items() if ch and ch[-1][0] == 1 and ch[-1][1] == wf
                      and M.buf[M.recs[f][0]:M.recs[f][0] + 4] == b'CELL']
                assert len(pc) == 1, 'world persistent cell'
                kb += copy_for(M.raw(pc[0]), src_loc)[0]; nrec += 1
                kb += children(pc[0], W['pers'], [])
            blocks = {}
            for cf in W['grid']:
                x, y = cell_grid(W['grid'][cf]['rec'])
                blocks.setdefault(ext_block(x, y), {}).setdefault(ext_sub(x, y), []).append(cf)
            for bl in sorted(blocks, key=lambda k: struct.unpack('<I', k)[0]):
                sb = b''
                for sl in sorted(blocks[bl], key=lambda k: struct.unpack('<I', k)[0]):
                    cb = b''
                    for cf in sorted(blocks[bl][sl]):
                        cb += W['grid'][cf]['rec']; nrec += 1
                        cb += children(cf, [], W['grid'][cf]['temp'])
                    sb += group(sl, 5, cb); ngrp += 1
                kb += group(bl, 4, sb); ngrp += 1
            wb += group(u32(wf), 1, kb); ngrp += 1
        out += group(b'WRLD', 0, wb); ngrp += 1
    nxt = local
    t4 = sub('HEDR', struct.pack('<fII', 1.0, nrec + ngrp, nxt))
    t4 += sub('CNAM', plan.get('author', 'NifSkope WW').encode('cp1252') + b'\0')
    for m in masters:
        t4 += sub('MAST', m.encode('cp1252') + b'\0') + sub('DATA', bytes(8))
    if onam:
        t4 += sub('ONAM', b''.join(u32(f) for f in sorted(onam)))
    t4 += sub('INTV', u32(1)) + sub('INCC', u32(incc))
    return record('TES4', F_LIGHT if light else 0, 0, t4) + out


# ------------------------------------------------------------------ verify
class Checks:
    def __init__(self):
        self.n = 0; self.fails = []

    def ok(self, cond, what):
        self.n += 1
        if not cond:
            self.fails.append(what)
        return cond


def verify(path, data):
    C = Checks()
    P = Plugin(path); b = P.buf
    nm = len(P.masters)
    C.ok(nm >= 1, 'header: no master')
    Ms = [Plugin(os.path.join(data, m)) for m in P.masters]
    for M in Ms:
        M.index()
    order = [t for t, _ in P.tes4_subs]
    want = ['HEDR', 'CNAM'] + ['MAST', 'DATA'] * nm + (['ONAM'] if 'ONAM' in order else []) + ['INTV', 'INCC']
    C.ok(order == want, 'header: subrecord order %s, want %s' % (' '.join(order), ' '.join(want)))
    ver, hcount, hnext = struct.unpack('<fII', dict(P.tes4_subs)['HEDR'])
    light = bool(P.flags & F_LIGHT)
    C.ok(not P.flags & 0x1, 'header: ESM flag set')
    # walk to EOF, exact sizes, collect the tree
    nrec = nrg = 0; recs = []  # (fid, sig, flags, chain, offset)
    def walk(p, end, chain):
        nonlocal nrec, nrg
        while p < end:
            if b[p:p + 4] == b'GRUP':
                size, label, gtype = struct.unpack_from('<III', b, p + 4)
                if not C.ok(size >= 24 and p + size <= end, 'size: group at %d (%d bytes) overruns its parent' % (p, size)):
                    return
                C.ok(b[p + 16:p + 24] == STAMP, 'group stamp at %d not zero' % p)
                nrg += 1
                walk(p + 24, p + size, chain + ((gtype, label, p),))
                p += size
            else:
                size, flags, fid = struct.unpack_from('<III', b, p + 4)
                if not C.ok(p + 24 + size <= end, 'size: record at %d overruns its group' % p):
                    return
                recs.append((fid, b[p:p + 4].decode(), flags, chain, p)); nrec += 1
                try:
                    payload_of(b[p:p + 24 + size])
                except Exception as e:
                    C.ok(False, 'record %08X payload: %s' % (fid, e))
                p += 24 + size
        C.ok(p == end, 'size: walk ended at %d, group ends at %d' % (p, end))
    walk(P.tes4_end, len(b), ())
    C.ok(hcount == nrec + nrg, 'count: HEDR count %d, records + groups %d' % (hcount, nrec + nrg))
    own = [f & 0xFFFFFF for f, *_ in recs if f >> 24 == nm]
    C.ok(all(o < hnext for o in own), 'ids: own id at or past HEDR next %06X' % hnext)
    if light:
        C.ok(all(0x800 <= o <= 0xFFF for o in own), 'ids: light plugin id outside 000800..000FFF')
    def master_of(fid):
        i = fid >> 24
        return Ms[i] if i < nm else None
    # every record: master copies agree with where the master keeps them
    incc = 0; onam = []
    for fid, sig, flags, chain, off in recs:
        M = master_of(fid)
        if sig == 'CELL':
            interior = chain and chain[0][1] == struct.unpack('<I', b'CELL')[0]
            if interior:
                incc += 1
            if not C.ok(M is not None and fid in M.recs, 'fids: CELL %08X is not a master record' % fid):
                continue
            mchain = M.recs[fid][2]
            C.ok([(t, l) for t, l, _ in chain] == [(t, l) for t, l, _ in mchain],
                 'label: CELL %08X sits under %s, the master under %s' % (
                     fid, [(t, struct.pack('<I', l).hex()) for t, l, _ in chain],
                     [(t, struct.pack('<I', l).hex()) for t, l, _ in mchain]))
        elif sig == 'WRLD':
            C.ok(M is not None and fid in M.recs, 'fids: WRLD %08X is not a master record' % fid)
        elif sig in ('REFR', 'ACHR'):
            gt = chain[-1][0] if chain else -1
            C.ok(gt in (8, 9), 'REFR %08X outside a type 8/9 group' % fid)
            pers = bool(flags & F_PERSISTENT)
            C.ok(not (pers and gt == 9), 'persistent in 9: REFR %08X' % fid)
            C.ok(not (not pers and gt == 8), 'temporary in 8: REFR %08X' % fid)
            # the type 6/8/9 labels name the cell record right before the type 6 group
            cell = chain[-1][1]
            C.ok(len(chain) >= 2 and chain[-2][0] == 6 and chain[-2][1] == cell, 'label: REFR %08X group labels' % fid)
            # exterior persistent: the cell must be the world's persistent cell (its master parent is type 1)
            if chain[0][1] == struct.unpack('<I', b'WRLD')[0]:
                cm = master_of(cell)
                mch = cm.recs[cell][2] if cm and cell in cm.recs else None
                is_pcell = bool(mch) and mch[-1][0] == 1
                C.ok(is_pcell == pers, 'exterior %s REFR %08X under %s cell %08X' % (
                    'persistent' if pers else 'temporary', fid, 'the persistent' if is_pcell else 'a grid', cell))
            q = b[off:]; _, body = payload_of(q[:24 + struct.unpack_from('<I', q, 4)[0]])
            ss = subs_of(body)
            name = dict(ss).get('NAME')
            if fid >> 24 == nm:   # our own: NAME first, DATA last (a master copy may lead with EDID / VMAD)
                C.ok(ss[0][0] == 'NAME' and ss[-1][0] == 'DATA', 'REFR %08X: NAME not first or DATA not last' % fid)
            if C.ok(name is not None, 'REFR %08X: no NAME' % fid):
                base = struct.unpack('<I', name)[0]; BM = master_of(base)
                C.ok(BM is not None and base in BM.recs, 'fids: REFR %08X base %08X resolves in no master' % (fid, base))
            if fid >> 24 < nm:
                C.ok(M is not None and fid in M.recs, 'fids: override %08X not in its master' % fid)
                if M is not None and fid in M.recs:
                    mgt = M.recs[fid][2][-1][0]
                    C.ok(mgt == gt, 'override %08X in group %d, the master keeps it in %d' % (fid, gt, mgt))
                    if gt == 9:
                        onam.append(fid)
    # group order: siblings in the master's order (block / sub-block labels as u32, cells by fid)
    sib = {}
    for fid, sig, flags, chain, off in recs:
        for i in range(len(chain)):
            sib.setdefault(tuple((t, l) for t, l, _ in chain[:i]), []).append((chain[i][2], chain[i][0], chain[i][1]))
    for parent, kids in sib.items():
        seen = []
        for k in kids:
            if k not in seen:
                seen.append(k)
        for t in (2, 3, 4, 5):
            labs = [l for _, gt, l in seen if gt == t]
            C.ok(labs == sorted(labs), 'order: type %d groups not ascending' % t)
    ts = dict(P.tes4_subs)
    C.ok(struct.unpack('<I', ts['INCC'])[0] == incc, 'incc: INCC %d, interior CELLs %d' % (struct.unpack('<I', ts['INCC'])[0], incc))
    got = list(struct.unpack('<%dI' % (len(ts['ONAM']) // 4), ts['ONAM'])) if 'ONAM' in ts else []
    C.ok(got == sorted(onam), 'onam: ONAM %s, temporary overrides %s' % (
        ['%08X' % f for f in got], ['%08X' % f for f in sorted(onam)]))
    return C, {'records': nrec, 'groups': nrg, 'bytes': len(b)}


def main(argv):
    data = DATA
    if '--data' in argv:
        i = argv.index('--data'); data = argv[i + 1]; del argv[i:i + 2]
    expect = None
    if '--expect-fail' in argv:
        i = argv.index('--expect-fail'); expect = argv[i + 1]; del argv[i:i + 2]
    if len(argv) >= 3 and argv[0] == 'build':
        out = build(json.load(open(argv[1])), data)
        open(argv[2], 'wb').write(out)
        print('ref wrote %s %d bytes' % (argv[2], len(out)))
        return 0
    if len(argv) >= 2 and argv[0] == 'verify':
        C, info = verify(argv[1], data)
        for f in C.fails:
            print('FAIL ' + f)
        print('%s: %d checks, %d failures (%d records, %d groups, %d bytes)' % (
            os.path.basename(argv[1]), C.n, len(C.fails), info['records'], info['groups'], info['bytes']))
        if expect is not None:
            hit = any(expect in f for f in C.fails)
            print('RED %s: %s' % ('OK (refused on "%s")' % expect if hit else 'MISSED (no failure names "%s")' % expect, argv[1]))
            return 0 if hit else 1
        return 1 if C.fails else 0
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
