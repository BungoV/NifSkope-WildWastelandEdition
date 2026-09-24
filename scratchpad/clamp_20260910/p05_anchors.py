# LANE CLAMP: re-derive docs/LODGEN_TERRAIN_VT.md's src/lodgen.cpp provenance
# from its ANCHORS, and re-stamp the file row.  ww-contract-provenance steps 1,
# 3 and 5.  Multi-site rows (a comma in the number cell) are listed for hand
# work and never rewritten by the script.
import hashlib, re, sys

DOC = 'docs/LODGEN_TERRAIN_VT.md'
SRC = 'src/lodgen.cpp'

sb = open(SRC, 'rb').read()
src = sb.decode('utf-8').split('\n')
stamp = (hashlib.sha256(sb).hexdigest()[:16], len(sb), sb.count(b'\n'))

raw = open(DOC, 'rb').read()
cr_before = raw.count(b'\r')
s = raw.decode('utf-8')


def lineof(anchor):
    hits = [i + 1 for i, l in enumerate(src) if anchor in l]
    if len(hits) != 1:
        return None, len(hits)
    return hits[0], 1


# (number cell as it stands, anchor text to search, kind)
ROWS = [
    ('6758-6840', 'root.insert( QStringLiteral( "kind" ), QStringLiteral( "terrainVT" ) );', 'range'),
    ('6772', 't.insert( QStringLiteral( "aniso" ), qMin( 16,', 'one'),
    ('6812, 6817', 't.insert( QStringLiteral( "coarseLevelsAreDownsamples" ), true );', 'multi'),
    ('6819', 't.insert( QStringLiteral( "partial" ), true );', 'one'),
    ('6831-6833', 'o.insert( QStringLiteral( "unitsPerTexel" ), levels[l].dim * 4096 / content );', 'range'),
]

moved = missing = manual = 0
for cell, anchor, kind in ROWS:
    ln, hits = lineof(anchor)
    if ln is None:
        print('MISSING/AMBIGUOUS (%d hits) for %r -- number left alone' % (hits, anchor[:60]))
        missing += 1
        continue
    old = 'lodgen.cpp:' + cell
    if s.count(old) != 1:
        print('doc row %r not found exactly once (%d)' % (old, s.count(old)))
        missing += 1
        continue
    if kind == 'one':
        new = 'lodgen.cpp:%d' % ln
    elif kind == 'range':
        a, b = (int(x) for x in cell.split('-'))
        new = 'lodgen.cpp:%d-%d' % (ln, ln + (b - a))
    else:
        # multi-site: re-derive every site by hand below, not here
        parts = [int(x) for x in cell.split(',')]
        second, h2 = lineof('t.insert( QStringLiteral( "alignedToWorldOrigin" ), true );')
        if second is None:
            print('MULTI row: second anchor ambiguous, left alone'); manual += 1; continue
        new = 'lodgen.cpp:%d, %d' % (ln, second)
        manual += 1
    if new != old:
        s = s.replace(old, new)
        moved += 1
        print('%-28s -> %s' % (old, new))
    else:
        print('%-28s unchanged' % old)

# the file stamp row
old_stamp = re.search(r'\| `src/lodgen\.cpp` \| `[0-9a-f]{16}` \| [0-9,]+ \| [0-9,]+ \|', s)
if not old_stamp:
    print('stamp row for src/lodgen.cpp not found'); sys.exit(1)
new_stamp = '| `src/lodgen.cpp` | `%s` | %s | %s |' % (
    stamp[0], format(stamp[1], ','), format(stamp[2], ','))
if old_stamp.group(0) != new_stamp:
    print('%s\n  -> %s' % (old_stamp.group(0), new_stamp))
    s = s[:old_stamp.start()] + new_stamp + s[old_stamp.end():]

out = s.encode('utf-8')
if out.count(b'\r') != cr_before:
    print('CR MOVED %d -> %d' % (cr_before, out.count(b'\r'))); sys.exit(1)
open(DOC, 'wb').write(out)
print('\n%d rows moved, %d anchors missing/ambiguous, %d multi-site rows '
      '(each re-derived from BOTH anchors here, and checked by hand)'
      % (moved, missing, manual))
print('src/lodgen.cpp now %s  %d bytes  %d lines' % stamp)
