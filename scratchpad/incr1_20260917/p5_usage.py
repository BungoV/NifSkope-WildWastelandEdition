"""INCR1 housekeeping -- `--no-native-cache` reaches the usage text.

A switch that exists and is not in `lodgen --help` is a switch nobody can find.
This adds it directly under the `--incremental` paragraph it belongs to, in the
same column as every other line, and says the ONE thing an operator needs: with
the flag, an FO4CS incremental run refuses, because there is nothing to replay.

Anchor asserted count == 1. Escapes from chr(). LF preserved.
"""
import io
import os
import sys

NL = chr(10)
TAB = chr(9)
DQ = chr(34)
BS = chr(92)
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                    '..', '..'))
CHECK = '--check' in sys.argv[1:]
rel = 'src/nifcli.cpp'

with io.open(os.path.join(ROOT, rel), 'rb') as fh:
    b = fh.read()
assert b.count(chr(13).encode()) == 0, rel + ' is not LF-only'
t = b.decode('utf-8')
if '--no-native-cache' in t.split('void lodgenUsage')[-1][:40000] and \
        'no-native-cache' in t[t.index('docs/LODGEN_LEDGER_FORMAT.md') - 4000:
                               t.index('docs/LODGEN_LEDGER_FORMAT.md') + 200]:
    raise SystemExit(rel + ' usage already names the flag')

PAD = ' ' * 42
NLE = BS + 'n'          # the two characters backslash + n inside the C string


def line(text):
    return TAB + TAB + '  << ' + DQ + text + NLE + DQ


anchor = line(PAD + 'docs/LODGEN_LEDGER_FORMAT.md') + NL
n = t.count(anchor)
if n != 1:
    raise SystemExit('ANCHOR usage: found %d times, wanted exactly 1' % n)

added = NL.join([
    line('  lodgen ... --incremental ... [--no-native-cache]'),
    line(PAD + 'do not write the per-chunk'),
    line(PAD + '.lodj cache the FO4CS target'),
    line(PAD + 'needs. A full bake still'),
    line(PAD + 'writes its LOD; an'),
    line(PAD + '--incremental --native run'),
    line(PAD + 'then REFUSES, because a'),
    line(PAD + 'skipped chunk has nothing to'),
    line(PAD + 'replay into the .lodo/.lodi'),
    line(PAD + 'pair and would be silently'),
    line(PAD + 'missing from it. Only for'),
    line(PAD + 'measuring what the cache'),
    line(PAD + 'costs.'),
]) + NL

t = t.replace(anchor, anchor + added, 1)

if CHECK:
    print('  would write %s (%d bytes)' % (rel, len(t.encode('utf-8'))))
else:
    with io.open(os.path.join(ROOT, rel), 'wb') as fh:
        fh.write(t.encode('utf-8'))
    print('  wrote %s (%d bytes)' % (rel, len(t.encode('utf-8'))))
