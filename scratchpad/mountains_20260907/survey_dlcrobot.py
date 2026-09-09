"""survey_dlcrobot.py - dissect DLCRobot.esm's WRLD group.

DLCRobot.esm overrides exactly 2 Commonwealth LAND records. It is therefore
the smallest RETAIL example of precisely the plugin this lane must emit, and
whatever it does is by definition a shape the shipping engine reads. Every
structural decision in make_landfix_esp.py is checked against this.

Questions answered here, all from bytes:
  * is the WRLD record itself present as an override, or only the group?
  * is a CELL override payload byte-identical to the master's, or trimmed?
  * are records compressed in the override plugin?
  * what does the plugin do about localized string subrecords (FULL) that
    would otherwise carry a master-relative string ID?
  * exact group nesting and sizes, printed as a tree.

Read-only.
"""

import struct
import sys

import fo4esm as E

DATA = r'X:\Programs\Steam\steamapps\common\Fallout 4\Data'
ESM = DATA + r'\Fallout4.esm'


def tree(buf, start, end, depth, out, maxdepth=99, limit=None):
    off = start
    n = 0
    while off + 24 <= end:
        if buf[off:off + 4] == b'GRUP':
            sig, gsize, label, gtype, tail = E.read_group_header(buf, off)
            lab = label.hex()
            extra = ''
            if gtype in (E.GT_WORLD_CHILDREN, E.GT_CELL_CHILDREN):
                extra = ' formid=%08X' % struct.unpack('<I', label)[0]
            elif gtype in (E.GT_EXTERIOR_BLOCK, E.GT_EXTERIOR_SUBBLOCK):
                y, x = struct.unpack('<hh', label)
                extra = ' (y=%d, x=%d)' % (y, x)
            out.append('%sGRUP type=%d %-18s label=%s%s size=%d @0x%X tail=%s'
                       % ('  ' * depth, gtype, E.GT_NAME.get(gtype, '?'), lab,
                          extra, gsize, off, tail.hex()))
            if depth < maxdepth:
                tree(buf, off + 24, off + gsize, depth + 1, out, maxdepth, limit)
            off += gsize
        else:
            sig, dsize, flags, formid, tail = E.read_record_header(buf, off)
            out.append('%s%s formid=%08X dsize=%d flags=0x%08X compressed=%s '
                       '@0x%X tail=%s'
                       % ('  ' * depth, sig.decode('latin1'), formid, dsize, flags,
                          bool(flags & E.COMPRESSED_FLAG), off, tail.hex()))
            off += 24 + dsize
        n += 1
        if limit and n >= limit:
            out.append('%s... (truncated)' % ('  ' * depth))
            return


def find_record(buf, want_formid, want_sig):
    """Return (offset, dsize, flags, tail, payload) for a record, searching the
    WRLD top group only."""
    hit = []

    def on_rec(off, sig, dsize, flags, formid, tail, stack):
        if formid == want_formid and sig == want_sig:
            hit.append((off, dsize, flags, tail,
                        E.record_payload(buf, off, dsize, flags)))

    for node in E.top_level_groups(buf):
        if node.label == b'WRLD' and node.gtype == E.GT_TOP:
            E.walk(buf, node.offset + 24, node.offset + node.gsize, [node], on_rec)
    return hit[0] if hit else None


def show_subs(payload, prefix):
    for tag, content in E.subrecords(payload):
        show = content.hex() if len(content) <= 32 else content[:32].hex() + '...'
        print('%s%s len=%-6d %s' % (prefix, tag.decode('latin1'), len(content), show))


def main():
    robot = E.load(DATA + r'\DLCRobot.esm')
    print('=== DLCRobot.esm WRLD top group, full tree ===')
    out = []
    for node in E.top_level_groups(robot):
        if node.label == b'WRLD' and node.gtype == E.GT_TOP:
            out.append('GRUP type=%d Top label=WRLD size=%d @0x%X tail=%s'
                       % (node.gtype, node.gsize, node.offset, node.tail.hex()))
            tree(robot, node.offset + 24, node.offset + node.gsize, 1, out)
    print('\n'.join(out))

    print()
    print('=== CELL 0000DF3C: DLCRobot override vs Fallout4.esm master ===')
    master = E.load(ESM)
    for cid in (0x0000DF3C, 0x0000DF5D):
        m = find_record(master, cid, b'CELL')
        r = find_record(robot, cid, b'CELL')
        print('CELL %08X' % cid)
        print('  master: dsize=%d compressed=%s payload=%d tail=%s'
              % (m[1], bool(m[2] & E.COMPRESSED_FLAG), len(m[4]), m[3].hex()))
        print('  robot : dsize=%d compressed=%s payload=%d tail=%s'
              % (r[1], bool(r[2] & E.COMPRESSED_FLAG), len(r[4]), r[3].hex()))
        print('  payload byte-identical: %s' % (m[4] == r[4]))
        print('  master subrecords:')
        show_subs(m[4], '    ')
        print('  robot subrecords:')
        show_subs(r[4], '    ')

    print()
    print('=== LAND overrides: DLCRobot vs master ===')
    for lid in (0x0000F145, 0x0000F124):
        m = find_record(master, lid, b'LAND')
        r = find_record(robot, lid, b'LAND')
        print('LAND %08X' % lid)
        print('  master: dsize=%d compressed=%s payload=%d tail=%s'
              % (m[1], bool(m[2] & E.COMPRESSED_FLAG), len(m[4]), m[3].hex()))
        print('  robot : dsize=%d compressed=%s payload=%d tail=%s'
              % (r[1], bool(r[2] & E.COMPRESSED_FLAG), len(r[4]), r[3].hex()))
        print('  payload byte-identical: %s' % (m[4] == r[4]))
        mtags = [(t, len(c)) for t, c in E.subrecords(m[4])]
        rtags = [(t, len(c)) for t, c in E.subrecords(r[4])]
        print('  master subrecord shape: %s' % (mtags,))
        print('  robot  subrecord shape: %s' % (rtags,))
        if m[4] != r[4]:
            msubs = list(E.subrecords(m[4]))
            rsubs = list(E.subrecords(r[4]))
            for i in range(max(len(msubs), len(rsubs))):
                a = msubs[i] if i < len(msubs) else None
                b = rsubs[i] if i < len(rsubs) else None
                if a != b:
                    print('    DIFFER at subrecord %d:' % i)
                    print('      master %r len=%s' % (a[0] if a else None,
                                                      len(a[1]) if a else None))
                    print('      robot  %r len=%s' % (b[0] if b else None,
                                                      len(b[1]) if b else None))
                    if a and b and a[0] == b[0] and len(a[1]) == len(b[1]):
                        ndiff = sum(1 for p, q in zip(a[1], b[1]) if p != q)
                        print('      same tag/len, %d differing bytes' % ndiff)
                    if a and a[0] in (b'BTXT', b'ATXT'):
                        print('      master content=%s' % a[1].hex())
                    if b and b[0] in (b'BTXT', b'ATXT'):
                        print('      robot  content=%s' % b[1].hex())


if __name__ == '__main__':
    main()
