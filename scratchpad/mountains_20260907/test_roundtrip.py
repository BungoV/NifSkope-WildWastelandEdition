"""test_roundtrip.py - THE GATE for lane ESPWRITE.

The brief: "Read a vanilla LAND, re-emit it through your writer with no
changes, and assert the payload is byte-identical to what you read. If that
does not hold, nothing downstream is trustworthy."

Five checks, run over the WHOLE Commonwealth (36,864 LAND records, 36,864
CELL records, 36 blocks, 576 sub-blocks) rather than a sample:

  A. GROUP TREE IDENTITY. Rebuild every exterior block group of the master
     from parsed structure - group headers re-emitted through
     esp_lib_land.build_group with sizes recomputed bottom-up, records
     re-emitted through build_record_raw - and compare against the master's
     own bytes. Proves the group-size convention, the header packing, the
     nesting and the ordering all at once.

  B. SUBRECORD ROUND TRIP. Every LAND payload: decompress, parse to
     subrecords, re-serialise, compare. Proves the subrecord reader/writer.

  C. COMPRESSION WRAPPER. Every LAND: the [u32 size][zlib] wrapper
     esp_lib.py flags as UNVALIDATED. Decompress the master's own bytes and
     check the declared size; then compress with our writer and read it back
     through our reader.

  D. IDENTITY SPLICE. splice_land_base_textures(payload, {}) - the writer's
     core operation asked to change nothing - must return the payload
     byte-identical. This is the brief's check, exactly.

  E. NEGATIVE CONTROLS. Every one of the above run against deliberately
     broken input, to prove the check can fail. "An invariant that cannot
     fail on broken code is not a test."

Read-only with respect to the master. Writes nothing.
"""

from __future__ import print_function

import struct
import sys
import time
import zlib

import fo4esm as E
import esp_lib_land as L

ESM = r'X:\Programs\Steam\steamapps\common\Fallout 4\Data\Fallout4.esm'


class Fail(Exception):
    pass


# --------------------------------------------------------------- rebuild

def rebuild_region(buf, start, end, stats):
    """Re-emit [start,end) from parsed structure. Records go back through
    L.build_record_raw from their (sig, formid, tail, payload, flags); groups
    go back through L.build_group with the size RECOMPUTED from the children,
    never copied from the master. So a wrong size convention cannot pass."""
    out = []
    off = start
    while off + 24 <= end:
        if buf[off:off + 4] == b'GRUP':
            sig, gsize, label, gtype, tail = E.read_group_header(buf, off)
            children = rebuild_region(buf, off + 24, off + gsize, stats)
            out.append(L.build_group(gtype, label, children, tail))
            stats['groups'] += 1
            off += gsize
        else:
            sig, dsize, flags, formid, tail = E.read_record_header(buf, off)
            raw = bytes(buf[off + 24:off + 24 + dsize])
            out.append(L.build_record_raw(sig, formid, tail, raw, flags))
            stats['records'] += 1
            off += 24 + dsize
    if off != end:
        raise Fail('region walk ended at 0x%X, expected 0x%X' % (off, end))
    return b''.join(out)


def find_world_children(buf):
    for node in E.top_level_groups(buf):
        if node.label != b'WRLD' or node.gtype != E.GT_TOP:
            continue
        found = []

        def on_grp(g, stack):
            if (g.gtype == E.GT_WORLD_CHILDREN
                    and struct.unpack('<I', g.label)[0] == E.COMMONWEALTH):
                found.append(g)

        E.walk(buf, node.offset + 24, node.offset + node.gsize, [node],
               lambda *a: None, on_grp)
        if found:
            return found[0]
    raise Fail('Commonwealth World Children group not found')


# ------------------------------------------------------------- the tests

def test_A_group_tree(buf, wc):
    print('--- A. group tree identity, every Commonwealth exterior block')
    blocks = []
    off = wc.offset + 24
    end = wc.offset + wc.gsize
    while off + 24 <= end:
        if buf[off:off + 4] == b'GRUP':
            sig, gsize, label, gtype, tail = E.read_group_header(buf, off)
            if gtype == E.GT_EXTERIOR_BLOCK:
                blocks.append((off, gsize, label, gtype, tail))
            off += gsize
        else:
            sig, dsize, flags, formid, tail = E.read_record_header(buf, off)
            off += 24 + dsize

    total_in = total_out = 0
    stats = {'groups': 0, 'records': 0}
    bad = 0
    for off, gsize, label, gtype, tail in blocks:
        original = bytes(buf[off:off + gsize])
        children = rebuild_region(buf, off + 24, off + gsize, stats)
        rebuilt = L.build_group(gtype, label, children, tail)
        stats['groups'] += 1
        total_in += len(original)
        total_out += len(rebuilt)
        if rebuilt != original:
            bad += 1
            y, x = struct.unpack('<hh', label)
            n = next((i for i in range(min(len(original), len(rebuilt)))
                      if original[i] != rebuilt[i]), None)
            print('    MISMATCH block (x=%d,y=%d): %d in, %d out, first diff at %s'
                  % (x, y, len(original), len(rebuilt), n))
    print('    %d blocks, %d groups, %d records rebuilt'
          % (len(blocks), stats['groups'], stats['records']))
    print('    bytes in  %s' % '{:,}'.format(total_in))
    print('    bytes out %s' % '{:,}'.format(total_out))
    if bad or total_in != total_out:
        raise Fail('group tree identity FAILED on %d of %d blocks' % (bad, len(blocks)))
    print('    PASS: all %d blocks byte-identical' % len(blocks))
    return total_in, stats


def collect_lands(buf, wc):
    """-> list of (offset, dsize, flags, formid, tail). Kept as offsets, not
    payloads, so the whole 76 MB of LAND does not sit in memory at once."""
    lands = []
    cells = []

    def on_rec(off, sig, dsize, flags, formid, tail, stack):
        if sig == b'LAND':
            lands.append((off, dsize, flags, formid, tail))
        elif sig == b'CELL' and stack[-1].gtype == E.GT_EXTERIOR_SUBBLOCK:
            cells.append((off, dsize, flags, formid, tail))

    E.walk(buf, wc.offset + 24, wc.offset + wc.gsize, [wc], on_rec)
    return lands, cells


def test_B_subrecords(buf, lands):
    print('--- B. subrecord round trip, every Commonwealth LAND')
    bad = 0
    maxlen = 0
    ntags = 0
    for off, dsize, flags, formid, tail in lands:
        payload = E.record_payload(buf, off, dsize, flags)
        subs = list(E.subrecords(payload))
        ntags += len(subs)
        for t, c in subs:
            if len(c) > maxlen:
                maxlen = len(c)
        rebuilt = b''.join(L.sub(t.decode('ascii'), c) for t, c in subs)
        if rebuilt != payload:
            bad += 1
            if bad <= 3:
                print('    MISMATCH LAND %08X: %d in, %d out'
                      % (formid, len(payload), len(rebuilt)))
    print('    %d LAND payloads, %d subrecords, longest subrecord %d bytes'
          % (len(lands), ntags, maxlen))
    if maxlen > 0xFFFF:
        raise Fail('a subrecord exceeds the u16 length field; XXXX escape needed '
                   'on write, which the writer does not implement')
    if bad:
        raise Fail('subrecord round trip FAILED on %d of %d' % (bad, len(lands)))
    print('    PASS: all %d byte-identical (u16 length field is sufficient, '
          'no XXXX needed for LAND)' % len(lands))


def test_C_compression(buf, lands):
    print('--- C. compression wrapper (esp_lib flags this format UNVALIDATED)')
    ncomp = 0
    bad = 0
    resave_bad = 0
    for off, dsize, flags, formid, tail in lands:
        raw = bytes(buf[off + 24:off + 24 + dsize])
        if not (flags & E.COMPRESSED_FLAG):
            continue
        ncomp += 1
        declared = struct.unpack_from('<I', raw, 0)[0]
        try:
            payload = zlib.decompress(raw[4:])
        except zlib.error as exc:
            bad += 1
            continue
        if len(payload) != declared:
            bad += 1
            continue
        # now through OUR writer and back through OUR reader
        rec = L.build_record_compressed(b'LAND', formid, tail, payload,
                                        flags & ~E.COMPRESSED_FLAG)
        sig2, dsize2, flags2, formid2, tail2 = E.read_record_header(rec, 0)
        back = E.record_payload(rec, 0, dsize2, flags2)
        if (back != payload or sig2 != b'LAND' or formid2 != formid
                or tail2 != tail):
            resave_bad += 1
    print('    %d of %d LAND records carry the compressed flag' % (ncomp, len(lands)))
    if bad or resave_bad:
        raise Fail('compression wrapper FAILED: %d master records did not '
                   'decompress to their declared size, %d did not survive our '
                   'own write/read' % (bad, resave_bad))
    print('    PASS: the [u32 uncompressed size][zlib] wrapper holds for all '
          '%d, and all %d survive our writer + reader' % (ncomp, ncomp))


def test_C2_xxxx_escape(buf):
    """The XXXX oversize escape, on the real record that forced it: the
    Commonwealth WRLD's OFST subrecord is 148,996 bytes and the first attempt
    to emit a WRLD override died in struct.pack('<H', ...). Prove the writer
    now round-trips it, and prove the plain u16 writer still cannot - if it
    could, this check would be measuring nothing."""
    print('--- C2. the XXXX oversize escape')
    wrld = None
    for node in E.top_level_groups(buf):
        if node.label != b'WRLD' or node.gtype != E.GT_TOP:
            continue
        off, end = node.offset + 24, node.offset + node.gsize
        while off + 24 <= end:
            if buf[off:off + 4] == b'GRUP':
                off += E.read_group_header(buf, off)[1]
                continue
            sig, dsize, flags, formid, tail = E.read_record_header(buf, off)
            if sig == b'WRLD' and formid == E.COMMONWEALTH:
                wrld = E.record_payload(buf, off, dsize, flags)
                break
            off += 24 + dsize
        break
    if wrld is None:
        raise Fail('Commonwealth WRLD record not found')
    subs = list(E.subrecords(wrld))
    longest = max(len(c) for _, c in subs)
    over = [t.decode('latin1') for t, c in subs if len(c) > 0xFFFF]
    print('    %d subrecords, longest %s bytes, over the u16 limit: %s'
          % (len(subs), '{:,}'.format(longest), over))
    if not over:
        raise Fail('expected at least one oversize subrecord to test against')
    out = L.serialize_subrecords(subs)
    if list(E.subrecords(out)) != subs:
        raise Fail('XXXX round trip FAILED: the re-emitted WRLD payload does '
                   'not parse back to the same subrecords')
    if out != wrld:
        raise Fail('XXXX round trip FAILED: %d bytes in, %d bytes out, not '
                   'byte-identical' % (len(wrld), len(out)))
    # negative control: the u16-only writer must still be unable to do this
    try:
        L.sub('OFST', subs[[t for t, _ in subs].index(b'OFST')][1])
        plain_ok = True
    except struct.error:
        plain_ok = False
    print('    plain u16 writer still rejects the 148,996-byte OFST: %s'
          % (not plain_ok))
    if plain_ok:
        raise Fail('the plain u16 writer accepted an oversize subrecord, so '
                   'this check proves nothing')
    print('    PASS: WRLD payload re-emitted byte-identical through the XXXX escape')


def test_D_identity_splice(buf, lands):
    print('--- D. identity splice: the writer asked to change nothing')
    bad = 0
    for off, dsize, flags, formid, tail in lands:
        payload = E.record_payload(buf, off, dsize, flags)
        out, n_inserted, at = L.splice_land_base_textures(payload, {})
        if out != payload or n_inserted != 0:
            bad += 1
            if bad <= 3:
                print('    MISMATCH LAND %08X: %d in, %d out, inserted %d'
                      % (formid, len(payload), len(out), n_inserted))
    if bad:
        raise Fail('identity splice FAILED on %d of %d' % (bad, len(lands)))
    print('    PASS: all %d LAND payloads survive the splice byte-identical'
          % len(lands))


def test_D2_replace_only(buf, lands):
    """A splice that only REPLACES an existing BTXT FormID must change exactly
    4 bytes and leave the payload the same length - a stronger statement than
    'it didn't crash'."""
    print('--- D2. replace-in-place changes exactly the 4 FormID bytes')
    checked = 0
    for off, dsize, flags, formid, tail in lands:
        payload = E.record_payload(buf, off, dsize, flags)
        base, has_alpha = L.land_quadrant_state(payload)
        if not base:
            continue
        q = sorted(base)[0]
        out, n_ins, at = L.splice_land_base_textures(payload, {q: 0x0BADF00D})
        if len(out) != len(payload):
            raise Fail('replace changed payload length %d -> %d on LAND %08X'
                       % (len(payload), len(out), formid))
        ndiff = sum(1 for a, b in zip(payload, out) if a != b)
        if ndiff > 4:
            raise Fail('replace changed %d bytes, expected <= 4, on LAND %08X'
                       % (ndiff, formid))
        back, _ = L.land_quadrant_state(out)
        if back[q] != 0x0BADF00D:
            raise Fail('replace did not take effect on LAND %08X' % formid)
        for oq in base:
            if oq != q and back[oq] != base[oq]:
                raise Fail('replace disturbed quadrant %d on LAND %08X' % (oq, formid))
        checked += 1
        if checked >= 2000:
            break
    print('    PASS: %d LANDs with an existing BTXT, replacement touched at most '
          'the 4 FormID bytes and no other quadrant' % checked)


def test_D3_insert(buf, lands):
    """Inserting 4 BTXTs into an untextured LAND must add exactly 4*(6+8)=56
    bytes, leave every original subrecord untouched, and read back correctly."""
    print('--- D3. insert into an untextured LAND adds exactly 56 bytes')
    checked = 0
    for off, dsize, flags, formid, tail in lands:
        payload = E.record_payload(buf, off, dsize, flags)
        base, has_alpha = L.land_quadrant_state(payload)
        if base or has_alpha:
            continue
        want = {0: 0x00021336, 1: 0x00021336, 2: 0x00021336, 3: 0x00021336}
        out, n_ins, at = L.splice_land_base_textures(payload, want)
        if n_ins != 4:
            raise Fail('expected 4 insertions, got %d on LAND %08X' % (n_ins, formid))
        if len(out) != len(payload) + 4 * (6 + 8):
            raise Fail('expected +56 bytes, got %+d on LAND %08X'
                       % (len(out) - len(payload), formid))
        # every original subrecord must survive unchanged and in order
        orig = list(E.subrecords(payload))
        new = [(t, c) for t, c in E.subrecords(out) if t != b'BTXT']
        if new != orig:
            raise Fail('insertion disturbed the original subrecords on LAND %08X'
                       % formid)
        back, _ = L.land_quadrant_state(out)
        if back != want:
            raise Fail('inserted BTXTs do not read back on LAND %08X: %r' % (formid, back))
        checked += 1
        if checked >= 2000:
            break
    print('    PASS: %d untextured LANDs, +56 bytes each, all original '
          'subrecords bit-identical and in order' % checked)


# --------------------------------------------------------- negative controls

def test_E_negative_controls(buf, wc, lands):
    """Every check above, run against deliberately broken input. If a check
    passes here, it is not a check."""
    print('--- E. negative controls: each gate made to fail on purpose')
    results = []

    # E1: group size convention wrong (copy the master's size instead of
    #     recomputing, after injecting an extra byte) - simulate by building a
    #     group whose declared size excludes its header, the classic error.
    def bad_build_group(gtype, label, children, tail):
        body = children if isinstance(children, bytes) else b''.join(children)
        return (b'GRUP' + struct.pack('<I', len(body))      # <-- header not counted
                + bytes(label) + struct.pack('<i', gtype) + bytes(tail) + body)

    off = wc.offset + 24
    end = wc.offset + wc.gsize
    blk = None
    while off + 24 <= end:
        if buf[off:off + 4] == b'GRUP':
            sig, gsize, label, gtype, tail = E.read_group_header(buf, off)
            if gtype == E.GT_EXTERIOR_BLOCK:
                blk = (off, gsize, label, gtype, tail)
                break
            off += gsize
        else:
            sig, dsize, flags, formid, tail = E.read_record_header(buf, off)
            off += 24 + dsize
    off, gsize, label, gtype, tail = blk
    original = bytes(buf[off:off + gsize])
    stats = {'groups': 0, 'records': 0}
    children = rebuild_region(buf, off + 24, off + gsize, stats)
    wrong = bad_build_group(gtype, label, children, tail)
    results.append(('E1 group size excludes its own 24-byte header',
                    wrong != original))

    # E2: block label written (x,y) instead of (y,x)
    lbl_right = L.exterior_label(*L.block_coords(-40, 7))
    lbl_wrong = struct.pack('<hh', *L.block_coords(-40, 7))   # x first: wrong
    results.append(('E2 block label byte order swapped', lbl_right != lbl_wrong))

    # E3: floor vs truncate-toward-zero on a negative cell
    def trunc(v, d):
        q = abs(v) // d
        return -q if v < 0 else q
    results.append(('E3 truncate-toward-zero differs from floor at x=-40',
                    L.block_coords(-40, 7)[0] != trunc(-40, 32)))

    # E4: subrecord round trip against a payload with one byte flipped
    o, ds, fl, fid, tl = lands[0]
    payload = bytearray(E.record_payload(buf, o, ds, fl))
    payload[-1] ^= 0xFF
    subs = list(E.subrecords(bytes(payload)))
    rebuilt = b''.join(L.sub(t.decode('ascii'), c) for t, c in subs)
    results.append(('E4 subrecord round trip detects a flipped payload byte',
                    rebuilt != E.record_payload(buf, o, ds, fl)))

    # E5: compression wrapper assert fires on a corrupted declared size
    good = E.record_payload(buf, o, ds, fl)
    blob = struct.pack('<I', len(good) + 1) + zlib.compress(good)
    try:
        E.decompress_payload(blob)
        fired = False
    except AssertionError:
        fired = True
    results.append(('E5 wrapper assert fires on a wrong declared size', fired))

    # E6: identity splice detects a writer that drops a subrecord
    def bad_splice(payload):
        subs = [s for s in E.subrecords(payload) if s[0] != b'VCLR']
        return b''.join(L.sub(t.decode('ascii'), c) for t, c in subs)
    vp = None
    for o2, ds2, fl2, fid2, tl2 in lands:
        p = E.record_payload(buf, o2, ds2, fl2)
        if any(t == b'VCLR' for t, _ in E.subrecords(p)):
            vp = p
            break
    results.append(('E6 identity check detects a writer that drops VCLR',
                    bad_splice(vp) != vp))

    # E7: the walker's "children consume exactly the group size" assert fires
    fake = bytearray(b'GRUP' + struct.pack('<I', 24 + 30) + b'ABCD'
                     + struct.pack('<i', 0) + b'\x00' * 8)
    fake += b'TEST' + struct.pack('<IiI', 100, 0, 1) + b'\x00' * 8  # claims 100 bytes
    try:
        E.walk(bytes(fake), 0, len(fake), [], lambda *a: None)
        fired7 = False
    except AssertionError:
        fired7 = True
    results.append(('E7 walker assert fires on a group whose children overrun',
                    fired7))

    allok = True
    for name, did_fail in results:
        print('    %-62s %s' % (name, 'detected' if did_fail else '*** NOT DETECTED ***'))
        allok = allok and did_fail
    if not allok:
        raise Fail('a negative control was not detected - that check is not a check')
    print('    PASS: all %d negative controls were detected' % len(results))


def main():
    t0 = time.time()
    print('gate: reading %s' % ESM)
    buf = E.load(ESM)
    print('       %s bytes' % '{:,}'.format(len(buf)))
    wc = find_world_children(buf)
    print('       Commonwealth World Children @0x%X, %s bytes'
          % (wc.offset, '{:,}'.format(wc.gsize)))
    print()

    lands, cells = collect_lands(buf, wc)
    print('collected %d LAND and %d CELL records' % (len(lands), len(cells)))
    print()

    test_A_group_tree(buf, wc)
    print()
    test_B_subrecords(buf, lands)
    print()
    test_C_compression(buf, lands)
    print()
    test_C2_xxxx_escape(buf)
    print()
    test_D_identity_splice(buf, lands)
    print()
    test_D2_replace_only(buf, lands)
    print()
    test_D3_insert(buf, lands)
    print()
    test_E_negative_controls(buf, wc, lands)
    print()
    print('ALL GATES PASS  (%.1f s)' % (time.time() - t0))


if __name__ == '__main__':
    try:
        main()
    except Fail as exc:
        print()
        print('GATE FAILED: %s' % exc)
        sys.exit(1)
