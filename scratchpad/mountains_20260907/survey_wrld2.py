"""survey_wrld2.py - decide the WRLD-override question with numbers.

survey_wrld.py showed every DLC's Commonwealth WRLD override drops most of the
master's RNAM subrecords (master ~7113, DLCCoast 168, DLCNukaWorld 134,
DLCRobot 54). RNAM on a FO4 WRLD is the LARGE REFERENCES table - decoded here
from the bytes: [int16 gridX][int16 gridY][u32 count] then count x
[u32 formid][int16 cellX][int16 cellY]. (232 = 8 + 28*8 checks out on the
first one, and every RNAM in the file must satisfy that arithmetic or this
script fails.)

Our plugin would load AFTER the DLCs, so a WRLD record copied from
Fallout4.esm reverts whatever they changed. Two things decide whether that is
acceptable:

  A. Are the DLC RNAM sets SUBSETS of the master's, or do they add grids/refs
     the master lacks? If subsets, a verbatim master copy loses nothing.
  B. Outside RNAM, what does each DLC actually change? Compare every
     non-RNAM subrecord by tag.

Also resolves the WRLD FULL string ID against Fallout4_en.STRINGS, so the
plugin can carry the worldspace name INLINE and stay non-localized.

Read-only.
"""

import os
import struct

import fo4esm as E

DATA = r'X:\Programs\Steam\steamapps\common\Fallout 4\Data'


def get_wrld(path, formid=E.COMMONWEALTH):
    buf = E.load(path)
    for node in E.top_level_groups(buf):
        if node.label != b'WRLD' or node.gtype != E.GT_TOP:
            continue
        off, end = node.offset + 24, node.offset + node.gsize
        while off + 24 <= end:
            if buf[off:off + 4] == b'GRUP':
                off += E.read_group_header(buf, off)[1]
                continue
            sig, dsize, flags, fid, tail = E.read_record_header(buf, off)
            if sig == b'WRLD' and fid == formid:
                return list(E.subrecords(E.record_payload(buf, off, dsize, flags)))
            off += 24 + dsize
    return None


def decode_rnam(content):
    """-> (gridX, gridY, [(formid, cellX, cellY), ...]). Asserts the length."""
    gx, gy, count = struct.unpack_from('<hhI', content, 0)
    assert len(content) == 8 + count * 8, (
        'RNAM length %d does not match 8 + 8*%d' % (len(content), count))
    refs = [struct.unpack_from('<Ihh', content, 8 + i * 8) for i in range(count)]
    return gx, gy, refs


def rnam_sets(subs):
    grids = {}
    for tag, content in subs:
        if tag != b'RNAM':
            continue
        gx, gy, refs = decode_rnam(content)
        grids.setdefault((gx, gy), set()).update(refs)
    return grids


def read_strings(path):
    """Fallout4_en.STRINGS: u32 count, u32 dataSize, then count x
    [u32 stringID][u32 offset], then a data block of NUL-terminated strings
    at those offsets. (.STRINGS is the null-terminated flavour; .DLSTRINGS /
    .ILSTRINGS prefix each entry with a u32 length.)"""
    with open(path, 'rb') as f:
        blob = f.read()
    count, data_size = struct.unpack_from('<II', blob, 0)
    dir_end = 8 + count * 8
    out = {}
    for i in range(count):
        sid, off = struct.unpack_from('<II', blob, 8 + i * 8)
        p = dir_end + off
        q = blob.index(b'\x00', p)
        out[sid] = blob[p:q].decode('utf-8', 'replace')
    return out


def main():
    master = get_wrld(DATA + r'\Fallout4.esm')
    mg = rnam_sets(master)
    m_refs = set()
    for s in mg.values():
        m_refs |= s
    print('master Commonwealth WRLD: %d subrecords, %d RNAM grids, %d large refs'
          % (len(master), len(mg), len(m_refs)))

    print()
    print('=== A. are the DLC large-reference sets subsets of the master? ===')
    for n in ('DLCCoast.esm', 'DLCNukaWorld.esm', 'DLCRobot.esm',
              'DLCworkshop01.esm', 'DLCworkshop02.esm', 'DLCworkshop03.esm'):
        p = os.path.join(DATA, n)
        subs = get_wrld(p)
        if subs is None:
            print('  %-22s no Commonwealth WRLD override' % n)
            continue
        g = rnam_sets(subs)
        refs = set()
        for s in g.values():
            refs |= s
        new_grids = set(g) - set(mg)
        new_refs = refs - m_refs
        print('  %-22s %d grids, %d refs | grids not in master: %d | '
              'refs not in master: %d'
              % (n, len(g), len(refs), len(new_grids), len(new_refs)))
        if new_refs:
            sample = sorted(new_refs)[:5]
            print('        sample refs absent from master: %s'
                  % ['%08X@(%d,%d)' % r for r in sample])

    print()
    print('=== B. non-RNAM subrecords: master vs each DLC ===')
    def nonrnam(subs):
        d = {}
        for tag, content in subs:
            if tag == b'RNAM':
                continue
            d.setdefault(tag, []).append(content)
        return d

    md = nonrnam(master)
    print('  master non-RNAM tags: %s'
          % sorted((t.decode('latin1'), len(v)) for t, v in md.items()))
    for n in ('DLCCoast.esm', 'DLCNukaWorld.esm', 'DLCRobot.esm'):
        subs = get_wrld(os.path.join(DATA, n))
        if subs is None:
            continue
        dd = nonrnam(subs)
        print('  --- %s' % n)
        for tag in sorted(set(md) | set(dd)):
            a = md.get(tag)
            b = dd.get(tag)
            t = tag.decode('latin1')
            if a is None:
                print('      %s: ADDED by this plugin (%d)' % (t, len(b)))
            elif b is None:
                print('      %s: REMOVED by this plugin (master had %d)' % (t, len(a)))
            elif a == b:
                pass  # identical, say nothing
            else:
                print('      %s: CHANGED  master %s  this %s' % (
                    t,
                    [len(x) for x in a] if sum(len(x) for x in a) > 48
                    else [x.hex() for x in a],
                    [len(x) for x in b] if sum(len(x) for x in b) > 48
                    else [x.hex() for x in b]))
        ident = sum(1 for tag in md if tag in dd and md[tag] == dd[tag])
        print('      non-RNAM tags identical to master: %d of %d' % (ident, len(md)))

    print()
    print('=== C. the WRLD FULL string ID ===')
    full = md.get(b'FULL')
    print('  master WRLD FULL raw bytes: %s' % (full[0].hex() if full else None))
    if full:
        sid = struct.unpack('<I', full[0])[0]
        print('  as a string ID: 0x%08X (%d)' % (sid, sid))
        sp = os.path.join(DATA, 'Strings', 'Fallout4_en.STRINGS')
        print('  %s exists: %s' % (sp, os.path.exists(sp)))
        if os.path.exists(sp):
            table = read_strings(sp)
            print('  entries in Fallout4_en.STRINGS: %d' % len(table))
            print('  resolved: %r' % table.get(sid))


if __name__ == '__main__':
    main()
