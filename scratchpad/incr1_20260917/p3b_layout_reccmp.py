"""INCR1 step 3, patch B -- reccmp() has to say when the two sides are not the
same FORMAT at all.

MEASURED. `release/NifSkope.before_layout1.exe` (Sep 16 20:21) predates lane
BAKEREC1, so it writes the v1 BINARY container -- the record on its side starts
with the four bytes `LODB` and a version byte, not with `lodb<TAB>2`. Feeding it
to lodb_read.normalise_file() raises UnicodeDecodeError, which patch A turned
into a plain "differs" and would have reported as a real defect. It is not one:
the v1 -> v2 change was ratified with its own divergence row.

So reccmp grows a third answer. rc 0 equal, rc 1 differs, rc 2 vacuous (the
floor was not met), rc 3 NOT COMPARABLE because one side is the old container.
Leg (c) counts rc 3 separately and prints why, and prints WHERE the record is
still measured instead, so the exception cannot quietly become "nobody checks
the record".

Anchors asserted count == 1. Escapes from chr(). LF preserved.
"""
import io
import os
import sys

NL = chr(10)
TAB = chr(9)
SQ = chr(39)
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                    '..', '..'))
CHECK = '--check' in sys.argv[1:]
rel = 'tests/spells/lodgen_layout.sh'

with io.open(os.path.join(ROOT, rel), 'rb') as fh:
    b = fh.read()
assert b.count(chr(13).encode()) == 0, rel + ' is not LF-only'
t = b.decode('utf-8')
if 'NOT COMPARABLE' in t:
    raise SystemExit(rel + ' already knows about the v1 container')

i = t.index('reccmp() {')
j = t.index(NL + '}' + NL, i) + len(NL + '}' + NL)
assert t.count('reccmp() {') == 1, 'ANCHOR reccmp: not exactly one'

T1, T2, T3, T4 = TAB, TAB * 2, TAB * 3, TAB * 4

body = NL.join([
    'reccmp() {',
    T1 + '"$PY" - "$1" "$2" "$ROOT/tests/spells" <<' + SQ + 'PYEOF' + SQ,
    'import sys',
    'sys.path.insert(0, sys.argv[3])',
    'import lodb_read',
    '',
    'V2 = ("lodb" + chr(9)).encode("utf-8")',
    '',
    '',
    'def substance(path):',
    '    """The record minus everything that cannot survive a relink.',
    '',
    '    Dropped: the `lodb` header line, whose last two fields are the exe',
    '    version and the exe SIZE IN BYTES, and the `census` lines, which are',
    '    wall-clock timings. What is left is every claim the bake made about',
    '    the files it wrote.',
    '    """',
    '    text = lodb_read.normalise_file(path)',
    '    return [l for l in text.split(chr(10))',
    '            if l != "" and not l.startswith("census" + chr(9))',
    '            and not l.startswith("lodb" + chr(9))]',
    '',
    '',
    'for p in (sys.argv[1], sys.argv[2]):',
    '    with open(p, "rb") as fh:',
    '        head = fh.read(5)',
    '    if not head.startswith(V2):',
    '        print("      NOT COMPARABLE: %s is the v1 binary container"',
    '              " (magic %r), the rung exe predates the plain-text record."',
    '              % (p.split("/")[-1], head[:4]))',
    '        print("      The record is still measured, on this exe, by"',
    '              " tests/spells/lodgen_bakerec.sh and by lodgen_incremental.sh"',
    '              " arm (d). It is not measured here.")',
    '        sys.exit(3)',
    '',
    'a, b = substance(sys.argv[1]), substance(sys.argv[2])',
    'chunks = sum(1 for l in a if l.startswith("chunk" + chr(9)))',
    'outs = sum(1 for l in a if l.startswith("out" + chr(9)))',
    'print("      the record is excepted from the bytes (its header stamps the"',
    '      " exe version and size); substance: %d line(s), %d chunk row(s),"',
    '      " %d out row(s)" % (len(a), chunks, outs))',
    'if chunks < 1 or outs < 1:',
    '    print("      VACUOUS: no chunk/out rows to compare")',
    '    sys.exit(2)',
    'if a == b:',
    '    sys.exit(0)',
    'for i in range(min(len(a), len(b))):',
    '    if a[i] != b[i]:',
    '        print("      first difference at line %d:" % (i + 1))',
    '        print("        rung: %s" % a[i][:160])',
    '        print("        ours: %s" % b[i][:160])',
    '        break',
    'if len(a) != len(b):',
    '    print("      %d line(s) on the rung side, %d on ours" % (len(a), len(b)))',
    'sys.exit(1)',
    'PYEOF',
    '}',
    ''])

t = t[:i] + body + t[j:]

# ---- leg (c) learns the third answer -----------------------------------------
old = NL.join([
    T4 + 'excused=$((excused+1))',
    T4 + 'if reccmp "$W/stock_old_$d/$rel" "$W/stock_new_$d/$rel"; then',
    T4 + TAB + 'same=$((same+1))',
    T4 + 'else',
    T4 + TAB + 'diff=$((diff+1)); echo "    DIFFERS in SUBSTANCE: $rel"',
    T4 + 'fi',
    ''])
new = NL.join([
    T4 + 'excused=$((excused+1))',
    T4 + 'reccmp "$W/stock_old_$d/$rel" "$W/stock_new_$d/$rel"; rrc=$?',
    T4 + 'if [ "$rrc" = "0" ]; then same=$((same+1))',
    T4 + 'elif [ "$rrc" = "3" ]; then notcmp=$((notcmp+1))',
    T4 + 'else diff=$((diff+1)); echo "    DIFFERS in SUBSTANCE: $rel"; fi',
    ''])
n = t.count(old)
if n != 1:
    raise SystemExit('ANCHOR leg (c) branch: found %d times, wanted 1' % n)
t = t.replace(old, new, 1)

for pair in [
    (T2 + 'same=0; diff=0; only=0; excused=0' + NL,
     T2 + 'same=0; diff=0; only=0; excused=0; notcmp=0' + NL,
     'counters'),
    (T3 + 'note "(c) dim $d: the whole stock tree matches the rung'
     + SQ + 's ($same files, $excused of them the record on substance)"',
     T3 + 'note "(c) dim $d: the whole stock tree matches the rung'
     + SQ + 's ($same files; $excused record(s) excepted from the bytes, $notcmp of those not comparable at all)"',
     'note'),
]:
    old2, new2, what = pair
    n = t.count(old2)
    if n != 1:
        raise SystemExit('ANCHOR %s: found %d times, wanted 1' % (what, n))
    t = t.replace(old2, new2, 1)

if CHECK:
    print('  would write %s (%d bytes)' % (rel, len(t.encode('utf-8'))))
else:
    with io.open(os.path.join(ROOT, rel), 'wb') as fh:
        fh.write(t.encode('utf-8'))
    print('  wrote %s (%d bytes)' % (rel, len(t.encode('utf-8'))))
