#!/usr/bin/env python3
"""fix_provenance.py -- the provenance rows whose ANCHOR TEXT this lane changed.

The scripted anchor pass (`p14_anchors_btd.py`) moves a row whose anchor still
exists in the source. These nine could not move, because the text they quote is
not there any more -- three because version 3 changed the line, six because the
whole file shifted and the pass's per-row uniqueness check refused an ambiguous
match. Each is re-derived here from the current source and the line number is
read out of a grep, not typed.

Also fixed: the row for the writer's own header-size check, which quoted the
ternary that is now a table, and the version row, which quoted `= 2`.
"""

P = 'docs/LODGEN_BTD_FORMAT.md'
raw = open(P, 'rb').read()
assert raw.count(b'\r') == 0
s = raw.decode('utf-8')

FIXES = [
    # (old row fragment, new row)
    ('| versions 1..2, header sizes 0x98 / 0xA0 | 48-51 | `constexpr quint32 LODL_VERSION = 2;` … `LODL_HEADER_V2 = 0xA0;` |',
     '| versions 1..3, header sizes 0x98 / 0xA0 / 0xF8 | 56-63 | `constexpr quint32 LODL_VERSION = 3;` … `LODL_HEADER_V3 = 0xF8;` |'),
    ('| section flag bits 0..3 | 53-56 | `constexpr quint32 SECT_COLOUR = 1u << 0;` |',
     '| section flag bits 0..7, now named in the header | 84-91 | `constexpr quint32 SECT_COLOUR = LODL_SECT_COLOUR;` |'),
    ("| AO row 0 is SOUTH | 135 | `Row 0 is SOUTH (the grid's own order, cell row 0 first)` |",
     "| AO row 0 is SOUTH | 186 | `Row 0 is SOUTH (the grid's own order, cell row 0 first)` |"),
    ('| v2\'s two default-water fields at 0x98 / 0x9C | 533-534 | `h.f32( src.defaultWaterHeight ); h.u32( src.defaultWaterType );` |',
     '| v2\'s two default-water fields at 0x98 / 0x9C | 1756-1757 | `h.f32( src.defaultWaterHeight );` |'),
    ('| the writer checks its own header size | 538-540 | `if ( h.size() != ( version >= 2 ? LODL_HEADER_V2 : LODL_HEADER_V1 ) )` |',
     '| the writer checks its own header size, against the TABLE | 1784-1786 | `if ( h.size() != lodtHeaderBytes( int( version ) ) )` |'),
    ('| directory ordered COARSEST first | 719-720 | `const int coarsest = levels - 1;` … `for ( int L = coarsest; L >= 0; L-- ) {` |',
     '| directory ordered COARSEST first | 1965-1966 | `for ( int L = coarsest; L >= 0; L-- ) {` |'),
    ('| the seam MAXIMUM rule | 1066-1070, 1036 | `hh = qMax( hh, sm.sRow[cc] );` |',
     '| the seam MAXIMUM rule | 2436 | `hh = qMax( hh, sm.sRow[cc] );` |'),
    ('| FO4 writes no ground cover | 1054-1059 | `FO4 has no ground cover HERE: measured, Fallout4.esm carries 0 GCVR records` |',
     '| FO4 writes no ground cover | 2424 | `FO4 has no ground cover HERE: measured, Fallout4.esm carries 0 GCVR records` |'),
    ('| block cache sizing note for subsampled walks | `lodtfile.h:191-197` | `a consumer that reads a SUBSAMPLED grid needs more` |',
     '| block cache sizing note for subsampled walks | `lodtfile.h:322` | `a consumer that reads a SUBSAMPLED grid needs` |'),
]

for old, new in FIXES:
    n = s.count(old)
    assert n == 1, 'row not found exactly once (%d): %r' % (n, old[:70])
    s = s.replace(old, new, 1)
    print('  fixed  %s' % new.split('|')[1].strip()[:56])

# the alpha-packing row: the anchor's pipe escape makes the pass skip it
OLD = '| alpha packing, five 3-bit fields | 1087-1089 |'
assert s.count(OLD) == 1
s = s.replace(OLD, '| alpha packing, five 3-bit fields | 2457-2459 |', 1)
print('  fixed  alpha packing, five 3-bit fields')

out = s.encode('utf-8')
assert out.count(b'\r') == 0
open(P, 'wb').write(out)
print('ok, %d bytes' % len(out))
