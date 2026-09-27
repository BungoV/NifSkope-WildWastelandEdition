#!/usr/bin/env python3
"""WATER1 task 3 gate: a vanilla .lodl from the flat-only writer against the same bake from the
sloped-water writer. Reads bytes only (shares no code with src/lodtfile.cpp).

  lodl_cmp.py <old.lodl> <new.lodl> [--floor]

Claims checked:
  * header: every field but the offsets is equal; the section word gains bit 9 (surface) only;
    every non-zero section offset moved by exactly +8 (the header grew 0xF8 -> 0x100);
  * the old file's bytes from 0xF8 on are the new file's bytes from 0x100 up to the surface plane,
    except for positions explained as a u64 that is exactly 8 larger (an absolute offset inside a
    directory); the body table, cell table and WATR table are byte-identical with no exception;
  * the surface plane is the LAST section and every tile of it is uniform 0 (csize 0, value 0).
--floor flips one byte of the new body table in memory: the gate must FAIL.
"""
import struct
import sys

SECT_SURFACE = 1 << 9
OFFS_V2 = [0x48, 0x50, 0x58, 0x60, 0x68, 0x70, 0x78, 0x80, 0x88]
OFFS_V3 = [0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xE8]


def u32(b, o):
    return struct.unpack_from('<I', b, o)[0]


def u64(b, o):
    return struct.unpack_from('<Q', b, o)[0]


def main():
    old = open(sys.argv[1], 'rb').read()
    new = bytearray(open(sys.argv[2], 'rb').read())
    floor = '--floor' in sys.argv
    fails = []
    if old[:4] != b'LODT' or new[:4] != b'LODT' or u32(old, 4) != 3 or u32(new, 4) != 3:
        print('REFUSED: not two version-3 .lodl files')
        sys.exit(2)
    oBody, nBodies, rec = u64(new, 0xA0), u32(new, 0xA8), u32(new, 0xAC)
    if floor:
        new[oBody + rec // 2] ^= 0x01
    # ---- header
    for o in range(0x08, 0x44, 4):
        if old[o:o + 4] != new[o:o + 4]:
            fails.append('header field 0x%02X differs' % o)
    so, sn = u32(old, 0x44), u32(new, 0x44)
    if sn != (so | SECT_SURFACE) or (so & SECT_SURFACE):
        fails.append('section word 0x%X -> 0x%X is not "bit 9 added"' % (so, sn))
    for o in (0x98, 0x9C, 0xA8, 0xAC, 0xB8, 0xBC, 0xC8, 0xCC, 0xD8, 0xDC, 0xF0, 0xF4):
        if old[o:o + 4] != new[o:o + 4]:
            fails.append('header field 0x%02X differs' % o)
    moved = 0
    for o in OFFS_V2 + OFFS_V3:
        a, b = u64(old, o), u64(new, o)
        if a == 0 and b == 0:
            continue
        if b != a + 8:
            fails.append('offset at 0x%02X: %d -> %d, not +8' % (o, a, b))
        else:
            moved += 1
    oSurf = u64(new, 0xF8)
    oldTotal, newTotal = u64(old, 0x90), u64(new, 0x90)
    if oldTotal != len(old) or newTotal != len(new):
        fails.append('a total-size field does not match its file')
    if oSurf != oldTotal + 8:
        fails.append('surface plane at %d, not right after the old file\'s end + 8 (%d)' % (oSurf, oldTotal + 8))
    # ---- body: old[0xF8:] vs new[0x100:oSurf]
    A, B = old[0xF8:], bytes(new[0x100:oSurf])
    if len(A) != len(B):
        fails.append('body lengths differ: %d vs %d' % (len(A), len(B)))
    n = min(len(A), len(B))
    i, patched, unexplained, firstBad = 0, 0, 0, None
    while i < n:
        if A[i] == B[i]:
            i += 1
            continue
        j = i
        while j < n and A[j] != B[j]:
            j += 1
        ok = False
        for s in range(max(0, j - 8), i + 1):
            if s + 8 <= n and s <= i and s + 8 >= j and u64(B, s) == u64(A, s) + 8:
                ok = True
                i = s + 8
                patched += 1
                break
        if not ok:
            unexplained += 1
            if firstBad is None:
                firstBad = i + 0xF8
            i = j
    if unexplained:
        fails.append('%d byte run(s) differ that are not an offset moved by 8 (first at old byte %d)'
                     % (unexplained, firstBad))

    def region(name, oOld, length):
        if length <= 0:
            return
        a = old[oOld:oOld + length]
        b = bytes(new[oOld + 8:oOld + 8 + length])
        if a != b:
            fails.append('%s (%d bytes) differs' % (name, length))
        else:
            identical.append('%s %d bytes' % (name, length))
    identical = []
    region('body table', u64(old, 0xA0), u32(old, 0xA8) * u32(old, 0xAC))
    region('WATR table', u64(old, 0x50), u32(old, 0x34) * 4)
    # the per-cell table: 16 bytes a cell in this revision? measured, not assumed: up to the next section
    cells = (u32(old, 0x10) - u32(old, 0x08) + 1) * (u32(old, 0x14) - u32(old, 0x0C) + 1)
    oCell = u64(old, 0x68)
    later = sorted(u64(old, o) for o in OFFS_V2 + OFFS_V3 if u64(old, o) > oCell)
    region('cell table (%d cells, to the next section)' % cells, oCell, (later[0] if later else len(old)) - oCell)
    # ---- the surface plane
    tx, ty, edge, bps = u32(new, oSurf), u32(new, oSurf + 4), u32(new, oSurf + 8), u32(new, oSurf + 12)
    dirAt = u64(new, oSurf + 16)
    tiles = tx * ty
    uni = sum(1 for t in range(tiles) if u32(new, dirAt + t * 16 + 8) == 0 and u32(new, dirAt + t * 16 + 12) == 0)
    end = dirAt + tiles * 16
    for t in range(tiles):
        c = u32(new, dirAt + t * 16 + 8)
        if c:
            end = max(end, u64(new, dirAt + t * 16) + c)
    if uni != tiles:
        fails.append('surface plane: %d of %d tiles uniform 0' % (uni, tiles))
    if end != len(new):
        fails.append('surface plane ends at %d of %d: not the last section' % (end, len(new)))
    if edge != u32(new, 0xBC) or bps != 4:
        fails.append('surface plane rate %d / %d bytes a sample (body rate %d)' % (edge, bps, u32(new, 0xBC)))
    print('old %d bytes, new %d bytes (+%d = 8 header + %d surface plane); %d header offsets moved by 8; '
          '%d u64 offsets inside the sections moved by 8, %d other byte runs differ'
          % (len(old), len(new), len(new) - len(old), len(new) - oSurf, moved, patched, unexplained))
    print('byte-identical: %s' % '; '.join(identical))
    print('surface plane: %dx%d tiles at %d a cell, %d bytes a sample, %d of %d tiles uniform 0, the last section%s'
          % (tx, ty, edge, bps, uni, tiles, ' (FLOOR: one body-table byte flipped)' if floor else ''))
    print('bodies %d, record %d bytes' % (nBodies, rec))
    for f in fails[:10]:
        print('  FAIL', f)
    print('FAIL' if fails else 'PASS')
    sys.exit(1 if fails else 0)


main()
