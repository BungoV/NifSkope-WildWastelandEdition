"""INCR1 step 2, patch E -- a lost CACHE does not widen.

Measured, not guessed. `s2_proof.sh` leg C deleted one chunk's `.lodj` and the
run printed:

    incremental: 4 of 4 chunks dirty (0 inputs moved, 0 not in the ledger,
                 1 output lost, 3 by neighbour, 0 with no native chunk cache)

-- so deleting one cache file rebaked the WHOLE region, and the leg that was
meant to prove "one chunk rebuilt beside three cached ones" proved nothing at
all, because there were no cached ones.

The widening exists because the terrain ring and the AO skirt each reach one
cell: a chunk whose INPUTS moved changes what its neighbours draw. A `.lodj` is
not an input to anything. It is this run's own record of what a chunk once
emitted, and losing it changes exactly one chunk's work: rebake that chunk and
it writes the same bytes again. So a chunk whose only lost output is its cache
goes on the dirty list and NOT on the list the widening is seeded from.

Anchors asserted count == 1. Escapes from chr(). LF preserved.
"""
import io
import os
import sys

NL = chr(10)
TAB = chr(9)
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                    '..', '..'))
CHECK = '--check' in sys.argv[1:]
rel = 'src/nifcli.cpp'

with io.open(os.path.join(ROOT, rel), 'rb') as fh:
    b = fh.read()
assert b.count(chr(13).encode()) == 0, rel + ' is not LF-only'
t = b.decode('utf-8')
if 'dirtyWide' in t:
    raise SystemExit(rel + ' already distinguishes the two lists')


def sub1(text, old, new, what):
    n = text.count(old)
    if n != 1:
        raise SystemExit('ANCHOR %s: found %d times, wanted exactly 1' % (what, n))
    return text.replace(old, new, 1)


def block(lines):
    return NL.join(lines)


T3 = TAB * 3
T4 = TAB * 4
T5 = TAB * 5
T6 = TAB * 6
T7 = TAB * 7

# ---- 1. the second list ------------------------------------------------------
t = sub1(t,
         T3 + 'QSet<QString> dirty;' + NL,
         block([
             T3 + 'QSet<QString> dirty;',
             T3 + '/* THE LIST THE WIDENING IS SEEDED FROM (lane INCR1, 2026-09-17),',
             T3 + ' * which is NOT the same list. The widening exists because the',
             T3 + ' * terrain ring and the AO skirt each reach one cell, so a chunk',
             T3 + ' * whose INPUTS moved changes what its neighbours draw. A chunk',
             T3 + ' * that is dirty only because its own `.lodj` cache went missing',
             T3 + ' * changes nothing for anybody: the cache is this tree\'s record of',
             T3 + ' * what that chunk once emitted, not an input to it, and rebaking',
             T3 + ' * the chunk writes the same bytes again. Seeding the widening',
             T3 + ' * from it made deleting ONE cache file rebake the whole region,',
             T3 + ' * measured in `scratchpad/incr1_20260917/s2_proof.txt` leg C. */',
             T3 + 'QSet<QString> dirtyWide;',
         ]) + NL,
         'dirty set')

# ---- 2. unknown + moved inputs seed both -------------------------------------
t = sub1(t,
         T5 + 'dirty.insert( key );' + NL
         + T5 + 'unknown++;',
         T5 + 'dirty.insert( key );' + NL
         + T5 + 'dirtyWide.insert( key );' + NL
         + T5 + 'unknown++;',
         'unknown branch')

t = sub1(t,
         T5 + 'dirty.insert( key );' + NL
         + T5 + 'movedInputs++;',
         T5 + 'dirty.insert( key );' + NL
         + T5 + 'dirtyWide.insert( key );' + NL
         + T5 + 'movedInputs++;',
         'moved inputs branch')

# ---- 3. the output loop tells the two kinds apart ----------------------------
t = sub1(t,
         block([
             T4 + 'for ( int k = 0; k < e.outFiles.size(); k++ ) {',
             T5 + 'const QString fp = prevRecordDir + "/" + e.outFiles[k];',
             T5 + 'if ( lodgenFileDigest( fp ) != e.outDigests[k] ) {',
             T6 + 'dirty.insert( key );',
             T6 + 'lostOutput++;',
             T6 + 'if ( reasons.size() < 8 )',
             T7 + 'reasons.append( QString( "  (%1,%2) output %3 is missing or edited" )',
             T7 + TAB + '.arg( j.cx ).arg( j.cy ).arg( e.outFiles[k] ) );',
             T6 + 'break;',
             T5 + '}',
             T4 + '}',
         ]),
         block([
             T4 + '/* EVERY output, not the first lost one, because WHICH output was',
             T4 + ' * lost decides whether the neighbours are dragged in with it. */',
             T4 + 'bool lostAny = false, lostReal = false;',
             T4 + 'for ( int k = 0; k < e.outFiles.size(); k++ ) {',
             T5 + 'const QString fp = prevRecordDir + "/" + e.outFiles[k];',
             T5 + 'if ( lodgenFileDigest( fp ) == e.outDigests[k] )',
             T6 + 'continue;',
             T5 + 'const bool isCache = e.outFiles[k].endsWith( QLatin1String( ".lodj" ) );',
             T5 + 'if ( !lostAny && reasons.size() < 8 )',
             T6 + 'reasons.append( QString( "  (%1,%2) output %3 is missing or edited%4" )',
             T6 + TAB + '.arg( j.cx ).arg( j.cy ).arg( e.outFiles[k] )',
             T6 + TAB + '.arg( isCache ? QStringLiteral( " (a chunk cache: this chunk only)" )',
             T6 + TAB + TAB + ': QString() ) );',
             T5 + 'lostAny = true;',
             T5 + 'if ( !isCache )',
             T6 + 'lostReal = true;',
             T4 + '}',
             T4 + 'if ( lostAny ) {',
             T5 + 'dirty.insert( key );',
             T5 + 'lostOutput++;',
             T5 + 'if ( lostReal )',
             T6 + 'dirtyWide.insert( key );',
             T4 + '}',
         ]),
         'output loop')

# ---- 4. the widening walks the smaller list ---------------------------------
t = sub1(t,
         T4 + 'for ( const QString & dk : dirty ) {',
         T4 + 'for ( const QString & dk : dirtyWide ) {',
         'widening seed')

if CHECK:
    print('  would write %s (%d bytes)' % (rel, len(t.encode('utf-8'))))
else:
    with io.open(os.path.join(ROOT, rel), 'wb') as fh:
        fh.write(t.encode('utf-8'))
    print('  wrote %s (%d bytes)' % (rel, len(t.encode('utf-8'))))
