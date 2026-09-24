"""INCR1 step 3 -- lodgen_layout.sh leg (c) stops measuring the exe's size.

MEASURED, not guessed. Leg (c) bakes the STOCK target with the rung exe and
with the new one and byte-compares the whole tree. Against the rung it reported
5 failures, and 4 of them were:

    DIFFERS: Commonwealth.lodb            (at dim 4, 8, 16 and 32)

The bake record's first line is

    lodb<TAB>2<TAB>Commonwealth<TAB>0.3.3+720762a<TAB>22477824
                                    ^ exe version   ^ exe SIZE IN BYTES

so two DIFFERENT exes can never write the same record bytes, however identical
the LOD they baked. Byte identity on that one file measures the linker, not the
lodgen. It went red the moment this lane's exe grew (22,477,824 -> 22,533,632)
and would go red again on any lane that adds a line of code.

What leg (c) is FOR is still worth measuring on the record: the two bakes must
have made the same claims about the same files. So the record is EXCEPTED from
the byte comparison, the exception is PRINTED with its reason, and the record is
compared on its substance instead -- the normalised text minus the header line
(exe version + size) and minus the `census` lines (wall-clock timings, which are
volatile by construction). The comparison has a floor: it must have seen at
least one `chunk` row and one `out` row, or it is vacuous and counts as a
failure, because an empty file compares equal to an empty file.

Anchors asserted count == 1. Every escape from chr(). LF preserved.
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
if 'reccmp' in t:
    raise SystemExit(rel + ' already excepts the record')


def sub1(text, old, new, what):
    n = text.count(old)
    if n != 1:
        raise SystemExit('ANCHOR %s: found %d times, wanted exactly 1' % (what, n))
    return text.replace(old, new, 1)


T1, T2, T3, T4 = TAB, TAB * 2, TAB * 3, TAB * 4

# ---- 1. the helper, in front of leg (c) --------------------------------------
ANCH = (NL + '# ============================================================================'
        + NL + 'echo' + NL
        + 'echo "== (c) the stock target is untouched, dim 4/8/16/32 =="' + NL)

helper = NL.join([
    '',
    '# reccmp <old.lodb> <new.lodb> -- the bake record compared on its SUBSTANCE.',
    '#',
    '# The record cannot be byte-compared across two exes: its header line is',
    '#     lodb<TAB>2<TAB><ws><TAB><exe version><TAB><exe size in bytes>',
    '# so it moves whenever the linker does. This drops that line and the `census`',
    '# lines (wall-clock timings) from lodbNormalise()-equivalent text and compares',
    '# what is left, which is every claim the bake made about the files on disk.',
    '# FLOOR: at least one `chunk` row and one `out` row, or the comparison is',
    '# vacuous -- two empty files are equal -- and it fails.',
    'reccmp() {',
    T1 + '"$PY" - "$1" "$2" "$ROOT/tests/spells" <<' + SQ + 'PYEOF' + SQ,
    'import sys',
    'sys.path.insert(0, sys.argv[3])',
    'import lodb_read',
    '',
    '',
    'def substance(path):',
    '    text = lodb_read.normalise_file(path)',
    '    keep = [l for l in text.split(chr(10))',
    '            if l != "" and not l.startswith("census" + chr(9))',
    '            and not l.startswith("lodb" + chr(9))]',
    '    return keep',
    '',
    '',
    'a, b = substance(sys.argv[1]), substance(sys.argv[2])',
    'chunks = sum(1 for l in a if l.startswith("chunk" + chr(9)))',
    'outs = sum(1 for l in a if l.startswith("out" + chr(9)))',
    'print("      the record is excepted from the bytes (its header stamps the exe"',
    '      " version and size); substance: %d line(s), %d chunk row(s), %d out row(s)"',
    '      % (len(a), chunks, outs))',
    'if chunks < 1 or outs < 1:',
    '    print("      VACUOUS: the record carries no chunk/out rows to compare")',
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
    '',
])
t = sub1(t, ANCH, NL + helper + ANCH[1:], 'leg (c) header')

# ---- 2. the counter ----------------------------------------------------------
t = sub1(t,
         T2 + 'same=0; diff=0; only=0' + NL,
         T2 + 'same=0; diff=0; only=0; excused=0' + NL,
         'leg (c) counters')

# ---- 3. the record leaves the byte comparison --------------------------------
old = (T3 + 'if [ ! -f "$W/stock_new_$d/$rel" ]; then only=$((only+1)); echo "    only in the rung' + SQ + 's: $rel"' + NL
       + T3 + 'elif cmp -s "$W/stock_old_$d/$rel" "$W/stock_new_$d/$rel"; then same=$((same+1))' + NL)
new = NL.join([
    T3 + 'if [ ! -f "$W/stock_new_$d/$rel" ]; then only=$((only+1)); echo "    only in the rung' + SQ + 's: $rel"',
    T3 + 'elif [ "${rel%.lodb}" != "$rel" ]; then',
    T4 + '# THE ONE EXCEPTION, and it says so out loud. See reccmp() above:',
    T4 + '# the record stamps the exe' + SQ + 's version AND SIZE, so byte identity',
    T4 + '# here would only ever measure that the binary grew. Its substance',
    T4 + '# is compared instead, and that comparison has its own floor.',
    T4 + 'excused=$((excused+1))',
    T4 + 'if reccmp "$W/stock_old_$d/$rel" "$W/stock_new_$d/$rel"; then',
    T4 + TAB + 'same=$((same+1))',
    T4 + 'else',
    T4 + TAB + 'diff=$((diff+1)); echo "    DIFFERS in SUBSTANCE: $rel"',
    T4 + 'fi',
    T3 + 'elif cmp -s "$W/stock_old_$d/$rel" "$W/stock_new_$d/$rel"; then same=$((same+1))',
    ''])
t = sub1(t, old, new, 'leg (c) compare')

# ---- 4. the verdicts name the exception --------------------------------------
t = sub1(t,
         T3 + 'note "(c) dim $d: the whole stock tree is byte-identical to the rung' + SQ + 's ($same files)"',
         T3 + 'note "(c) dim $d: the whole stock tree matches the rung' + SQ + 's ($same files, $excused of them the record on substance)"',
         'leg (c) note')
t = sub1(t,
         T3 + 'bad "(c) dim $d: the whole stock tree is byte-identical to the rung' + SQ + 's ($diff differ, $only on one side only)"',
         T3 + 'bad "(c) dim $d: the stock tree does not match the rung' + SQ + 's ($diff differ, $only on one side only, $excused excepted)"',
         'leg (c) bad')

if CHECK:
    print('  would write %s (%d bytes)' % (rel, len(t.encode('utf-8'))))
else:
    with io.open(os.path.join(ROOT, rel), 'wb') as fh:
        fh.write(t.encode('utf-8'))
    print('  wrote %s (%d bytes)' % (rel, len(t.encode('utf-8'))))
