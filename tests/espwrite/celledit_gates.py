#!/usr/bin/env python3
"""Lane ESPWRITE2 gates on `NifSkope -no-gui espwrite place` (src/io/espwritecli.cpp), independent of the C++ maths.

  ghost <place.log> <written.esp> <master.esm>
      gate 5 (placement bounds): every `item form F ghost ...` line's AABB and heading tip are recomputed from the
      REFR the file holds (NAME, XSCL, DATA) and the base's OBND in the master, with the drawn rule
      M = fromEuler(-rx, -ry, -rz) (Rx Ry Rz, niftypes.cpp), tip = pos + M (0, 48, 0). Red WW_CELLEDIT_RED=ghost
      (the un-negated euler) must fail on any yaw that is not 0 or 180 degrees.
  gap <in.esp> <out.esp>
      the append fixture: own REFR ids moved up by 0x80, HEDR next 0x900 (a file whose refs were deleted elsewhere).
  append <before.esp> <after.esp> [--added N] [--red]
      the "Add to existing" identity: every REFR of the old file is in the new one byte for byte with its form id,
      the new file holds N more, HEDR next grows, and HEDR count = records + groups. Red WW_ESPWRITE_RED=renumber
      must fail; pass --red on a red run: a <before> with contiguous own ids from 0x800 (renumber hands out the same
      ids) is refused as VACUOUS, rc 2. The red runs on the gap fixture (tests/espwrite/append_gate.sh).
Exit 0 = pass, 1 = a failure (each printed as FAIL ...), 2 = usage or a vacuous red.
"""
import math, os, re, struct, sys, zlib

F_COMPRESSED = 0x40000


def subs_of(payload):
    out, p, big = [], 0, None
    while p < len(payload):
        t = payload[p:p + 4].decode('ascii', 'replace'); n = struct.unpack_from('<H', payload, p + 4)[0]
        if t == 'XXXX':
            big = struct.unpack_from('<I', payload, p + 6)[0]; p += 6 + n
            continue
        if big is not None:
            n, big = big, None
        out.append((t, payload[p + 6:p + 6 + n])); p += 6 + n
    return out


def payload_of(rec):
    size, flags = struct.unpack_from('<II', rec, 4)
    body = rec[24:24 + size]
    if flags & F_COMPRESSED:
        body = zlib.decompress(body[4:])
    return body


def walk(buf):
    """TES4 subs, {form: (sig, raw)}, record count, group count."""
    t4 = 24 + struct.unpack_from('<I', buf, 4)[0]
    tes4 = subs_of(buf[24:t4])
    recs, nrec, ngrp = {}, 0, 0
    p = t4
    while p < len(buf):
        if buf[p:p + 4] == b'GRUP':
            ngrp += 1; p += 24
            continue
        size, flags, fid = struct.unpack_from('<III', buf, p + 4)
        recs[fid] = (buf[p:p + 4].decode(), buf[p:p + 24 + size]); nrec += 1
        p += 24 + size
    return tes4, recs, nrec, ngrp


def find_record(path, fid, sig):
    """One record of a master by form id: a linear walk of its top group `sig` only (no full index)."""
    buf = open(path, 'rb').read()
    p = 24 + struct.unpack_from('<I', buf, 4)[0]
    want = sig.encode()
    while p < len(buf):
        size, label = struct.unpack_from('<I4s', buf, p + 4)
        if label == want:
            q, end = p + 24, p + size
            while q < end:
                if buf[q:q + 4] == b'GRUP':
                    q += 24
                    continue
                rs, rf, rid = struct.unpack_from('<III', buf, q + 4)
                if rid == fid:
                    return buf[q:q + 24 + rs]
                q += 24 + rs
            return None
        p += size
    return None


def from_euler(x, y, z):
    sX, cX, sY, cY, sZ, cZ = math.sin(x), math.cos(x), math.sin(y), math.cos(y), math.sin(z), math.cos(z)
    return [[cY * cZ, -cY * sZ, sY],
            [sX * sY * cZ + sZ * cX, cX * cZ - sX * sY * sZ, -sX * cY],
            [sX * sZ - cX * sY * cZ, cX * sY * sZ + sX * cZ, cX * cY]]


FAILS = []


def check(ok, what):
    if not ok:
        FAILS.append(what); print('FAIL', what)
    return ok


def gate_ghost(log, esp, master):
    lines = [l for l in open(log, encoding='utf-8', errors='replace') if l.startswith('item form ')]
    check(len(lines) >= 1, 'no `item form` line in %s' % log)
    tes4, recs, _, _ = walk(open(esp, 'rb').read())
    masters = [v.rstrip(b'\0').decode('cp1252') for t, v in tes4 if t == 'MAST']
    nums = r'(-?[0-9.]+)'
    n = 0
    for l in lines:
        m = re.match(r'item form ([0-9A-F]{8}) ghost base [0-9A-F]{8} pos ' + ' '.join([nums] * 3) + ' rot ' + ' '.join([nums] * 3)
                     + ' scale ' + nums + ' obnd ' + ' '.join([nums] * 6) + ' aabb ' + ' '.join([nums] * 6) + ' tip ' + ' '.join([nums] * 3), l)
        if not check(m is not None, 'cannot read line: %s' % l.strip()):
            continue
        g = m.groups(); form = int(g[0], 16); vals = [float(v) for v in g[1:]]
        aabb_c, tip_c = vals[13:19], vals[19:22]
        if not check(form in recs and recs[form][0] == 'REFR', 'form %08X is no REFR of %s' % (form, esp)):
            continue
        subs = dict(subs_of(payload_of(recs[form][1])))
        base = struct.unpack('<I', subs['NAME'])[0]
        scale = struct.unpack('<f', subs['XSCL'])[0] if 'XSCL' in subs else 1.0
        d = struct.unpack('<6f', subs['DATA'])
        pos, rot = d[0:3], d[3:6]
        mi = base >> 24
        if not check(mi < len(masters), 'base %08X has no master' % base):
            continue
        mpath = os.path.join(os.path.dirname(master), masters[mi])
        brec = None
        for sig in ('STAT', 'MSTT', 'FURN', 'ACTI', 'MISC'):
            brec = find_record(mpath, base & 0xFFFFFF, sig)
            if brec:
                break
        if not check(brec is not None, 'base %08X not in %s' % (base, masters[mi])):
            continue
        bs = dict(subs_of(payload_of(brec)))
        ob = struct.unpack('<6h', bs['OBND'][:12]) if 'OBND' in bs else (-16, -16, -16, 16, 16, 16)
        M = from_euler(-rot[0], -rot[1], -rot[2])
        lo, hi = [1e30] * 3, [-1e30] * 3
        for c in range(8):
            p = (ob[3] if c & 1 else ob[0], ob[4] if c & 2 else ob[1], ob[5] if c & 4 else ob[2])
            for i in range(3):
                w = pos[i] + scale * sum(M[i][j] * p[j] for j in range(3))
                lo[i] = min(lo[i], w); hi[i] = max(hi[i], w)
        tip = [pos[i] + M[i][1] * 48.0 for i in range(3)]
        want = lo + hi
        err_box = max(abs(a - b) for a, b in zip(want, aabb_c))
        err_tip = max(abs(a - b) for a, b in zip(tip, tip_c))
        check(err_box < 0.01, 'form %08X aabb off by %.4f (file %s, drawn %s)' % (form, err_box, ['%.3f' % v for v in want], aabb_c))
        check(err_tip < 0.01, 'form %08X tip off by %.4f (file %s, drawn %s)' % (form, err_tip, ['%.3f' % v for v in tip], tip_c))
        check(abs(math.dist(tip, pos) - 48.0) < 0.01, 'form %08X tip is not 48 from pos' % form)
        print('ghost %08X base %08X obnd %s yaw %.6f aabb err %.5f tip err %.5f' % (form, base, ob, rot[2], err_box, err_tip))
        n += 1
    print('gate 5: %d item(s), %d failures' % (n, len(FAILS)))


FIRST_ID = 0x800   # the writer's first own id (espwrite.cpp kLightFirst)


def renumber_blind(tes4, recs, next_id):
    """True when the old rule (every ref a fresh id from FIRST_ID) hands out the very ids append keeps: own REFR ids
    FIRST_ID .. FIRST_ID+n-1 and HEDR next FIRST_ID+n. Such a file cannot tell the red renumber from the green."""
    nm = sum(1 for t, v in tes4 if t == 'MAST')
    own = sorted(f & 0xFFFFFF for f, (s, r) in recs.items() if s == 'REFR' and (f >> 24) == nm)
    return own == list(range(FIRST_ID, FIRST_ID + len(own))) and (next_id & 0xFFFFFF) == FIRST_ID + len(own)


def gate_append(before, after, added, red=False):
    b = open(before, 'rb').read(); a = open(after, 'rb').read()
    tb, rb, _, _ = walk(b); ta, ra, nrec, ngrp = walk(a)
    hb = dict(tb)['HEDR']; ha = dict(ta)['HEDR']
    _, cb, nb = struct.unpack('<fII', hb[:12]); _, ca, na = struct.unpack('<fII', ha[:12])
    if red and renumber_blind(tb, rb, nb):
        # lane BUGS1102D (ESPWRITE2-RENUMBER-RED): the red on such a file read rc 0 in every exe gate run
        print('VACUOUS red: %s holds contiguous own ids from %X with next %X; renumber gives the same ids. '
              'Run the red on the gap fixture (gap <in> <out>).' % (before, FIRST_ID, nb))
        return 2
    check(ca == nrec + ngrp, 'HEDR count %d, the file holds %d records + %d groups' % (ca, nrec, ngrp))
    old = {f: r for f, (s, r) in rb.items() if s == 'REFR'}
    new = {f: r for f, (s, r) in ra.items() if s == 'REFR'}
    kept = sum(1 for f, r in old.items() if new.get(f) == r)
    check(len(old) >= 1, 'the old file holds no REFR')
    check(kept == len(old), '%d of %d old REFRs kept byte for byte with their ids' % (kept, len(old)))
    check(len(new) - len(old) == added, 'the new file holds %d more REFRs, want %d' % (len(new) - len(old), added))
    check(na > nb, 'HEDR next %X does not grow past %X' % (na, nb))
    fresh = sorted(set(new) - set(old))
    check(all((f & 0xFFF) >= (nb & 0xFFF) for f in fresh), 'a new id %s is below the old next %X' % (['%08X' % f for f in fresh], nb))
    print('append: old %d REFR, kept %d, new %d (%s), next %X -> %X, count %d' % (len(old), kept, len(new),
          ' '.join('%08X' % f for f in fresh), nb, na, ca))
    print('append gate: %d failures' % len(FAILS))


def make_gap(src, dst, shift=0x80, next_id=0x900):
    """A plugin whose ids do not start at the writer's first id (refs deleted in xEdit, ids never reused): every own
    REFR id moves up by `shift` and HEDR next becomes `next_id`. Append to it must keep the moved ids and start past
    next_id; the red `renumber` hands out 0x800 again."""
    buf = bytearray(open(src, 'rb').read())
    tes4, recs, _, _ = walk(bytes(buf))
    nm = sum(1 for t, v in tes4 if t == 'MAST')
    t4 = 24 + struct.unpack_from('<I', buf, 4)[0]
    p, moved = t4, 0
    while p < len(buf):
        if buf[p:p + 4] == b'GRUP':
            p += 24
            continue
        size, flags, fid = struct.unpack_from('<III', buf, p + 4)
        if buf[p:p + 4] == b'REFR' and (fid >> 24) == nm:
            struct.pack_into('<I', buf, p + 12, fid + shift); moved += 1
        p += 24 + size
    q = 24
    while q < t4:
        n = struct.unpack_from('<H', buf, q + 4)[0]
        if buf[q:q + 4] == b'HEDR':
            struct.pack_into('<I', buf, q + 6 + 8, next_id)
        q += 6 + n
    open(dst, 'wb').write(bytes(buf))
    print('gap: %d own REFR id(s) moved by %X, HEDR next %X -> %s' % (moved, shift, next_id, dst))


def main(argv):
    if len(argv) >= 4 and argv[1] == 'gap':
        make_gap(argv[2], argv[3])
    elif len(argv) >= 5 and argv[1] == 'ghost':
        gate_ghost(argv[2], argv[3], argv[4])
    elif len(argv) >= 4 and argv[1] == 'append':
        added = int(argv[argv.index('--added') + 1]) if '--added' in argv else 1
        if gate_append(argv[2], argv[3], added, red='--red' in argv) == 2:
            return 2
    else:
        print(__doc__); return 2
    return 1 if FAILS else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
