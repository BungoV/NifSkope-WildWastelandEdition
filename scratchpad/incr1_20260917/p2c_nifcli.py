"""INCR1 step 2, patch D -- the driver: `--incremental` stops refusing `--native`.

  * `--no-native-cache` is the exact way back (CONSTITUTION 10) and, with it,
    the refusal is exactly the one that was there before.
  * A clean chunk whose `.lodj` is not on disk is marked DIRTY, so a deleted
    cache self-heals and a record written before this lane cannot make an empty
    pair look like a rebake.
  * The cache is written on every `--native` bake, listed in the record's `out`
    rows, and read back for every chunk the run skips.
  * An arrival lit by more than one chunk makes the run REFUSE rather than
    write a pair that is nearly right.

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
if 'gLgNativeCache' in t:
    raise SystemExit(rel + ' already wired')


def sub1(text, old, new, what):
    n = text.count(old)
    if n != 1:
        raise SystemExit('ANCHOR %s: found %d times, wanted exactly 1' % (what, n))
    return text.replace(old, new, 1)


def block(lines):
    return NL.join(lines)


# ---- 1. the sticky flag ------------------------------------------------------
t = sub1(t, 'static QString gLgBakeRecord;',
         block([
             'static QString gLgBakeRecord;',
             '/*! THE PER-CHUNK NATIVE CACHE (lane INCR1, 2026-09-17), on by default.',
             ' *  `--no-native-cache` is the EXACT way back: no `.lodj` is written, the',
             ' *  record\'s `out` rows are what they were before this lane, and',
             ' *  `--incremental --native` refuses exactly as it used to (CONSTITUTION 10).',
             ' *  Sticky like `gLgBakeRecord` beside it, for the same reason: the parser',
             ' *  and the driver are different functions and this is not worth a parameter',
             ' *  in a signature that already has thirty. */',
             'static bool gLgNativeCache = true;',
         ]), 'gLgBakeRecord global')

t = sub1(t, TAB + 'gLgBakeRecord.clear();',
         TAB + 'gLgBakeRecord.clear();' + NL + TAB + 'gLgNativeCache = true;',
         'parser reset')

t = sub1(t,
         TAB + TAB + 'else if ( t == QLatin1String( "--bake-record" ) ) gLgBakeRecord = next();',
         TAB + TAB + 'else if ( t == QLatin1String( "--bake-record" ) ) gLgBakeRecord = next();' + NL
         + TAB + TAB + 'else if ( t == QLatin1String( "--no-native-cache" ) ) gLgNativeCache = false;',
         'parser bake-record')

# ---- 2. the refusal, narrowed to the way back --------------------------------
old_refusal = block([
    TAB + TAB + TAB + 'if ( !nativeDir.isEmpty() ) {',
    TAB + TAB + TAB + TAB + 'err() << "refused: --native builds ONE .lodo/.lodi pair for the whole region out of "',
    TAB + TAB + TAB + TAB + TAB + TAB + ' "the placements the chunk pass hands it, so an incremental run would write "',
    TAB + TAB + TAB + TAB + TAB + TAB + ' "a pair covering only the chunks it rebaked and say nothing about the rest."',
    TAB + TAB + TAB + TAB + TAB + '  << Qt::endl;',
    TAB + TAB + TAB + TAB + 'err() << "  bake without --incremental, or drop --native from this run and do the "',
    TAB + TAB + TAB + TAB + TAB + TAB + ' "pair in a separate full pass." << Qt::endl;',
    TAB + TAB + TAB + TAB + 'return 1;',
    TAB + TAB + TAB + '}',
])
new_refusal = block([
    TAB + TAB + TAB + '/* ...and that refusal is what lane INCR1 was opened to remove, because',
    TAB + TAB + TAB + ' * it made `--incremental` refuse the RULED pipeline: bungo\'s FO4CS',
    TAB + TAB + TAB + ' * command is `--native <dir>`, so "refuse --native" read "refuse the',
    TAB + TAB + TAB + ' * only target anybody bakes". A skipped chunk now speaks from its',
    TAB + TAB + TAB + ' * `.lodj` cache instead of being silently missing from the pair; the',
    TAB + TAB + TAB + ' * chunk pass replays it in that chunk\'s own queue position, so the',
    TAB + TAB + TAB + ' * arrival order -- which the library\'s mesh ids depend on -- is the',
    TAB + TAB + TAB + ' * order a full bake would have produced.',
    TAB + TAB + TAB + ' *',
    TAB + TAB + TAB + ' * With the cache turned OFF there is nothing to read back, so the',
    TAB + TAB + TAB + ' * refusal is exactly the one that was here before. */',
    TAB + TAB + TAB + 'if ( !nativeDir.isEmpty() && !gLgNativeCache ) {',
    TAB + TAB + TAB + TAB + 'err() << "refused: --native builds ONE .lodo/.lodi pair for the whole region out of "',
    TAB + TAB + TAB + TAB + TAB + TAB + ' "the placements the chunk pass hands it, so an incremental run would write "',
    TAB + TAB + TAB + TAB + TAB + TAB + ' "a pair covering only the chunks it rebaked and say nothing about the rest."',
    TAB + TAB + TAB + TAB + TAB + '  << Qt::endl;',
    TAB + TAB + TAB + TAB + 'err() << "  --no-native-cache is what turned the per-chunk cache off; drop it and "',
    TAB + TAB + TAB + TAB + TAB + TAB + ' "the skipped chunks speak from their .lodj files. Or bake without "',
    TAB + TAB + TAB + TAB + TAB + TAB + ' "--incremental." << Qt::endl;',
    TAB + TAB + TAB + TAB + 'return 1;',
    TAB + TAB + TAB + '}',
])
t = sub1(t, old_refusal, new_refusal, 'native refusal')

# ---- 3. a clean chunk with no cache is dirty ---------------------------------
t = sub1(t,
         TAB + TAB + TAB + 'const int spread = widened.size() - dirty.size();' + NL,
         block([
             TAB + TAB + TAB + 'const int spread = widened.size() - dirty.size();',
             TAB + TAB + TAB + '/* THE CACHE HAS TO BE THERE FOR EVERY CHUNK THIS RUN WILL SKIP',
             TAB + TAB + TAB + ' * (lane INCR1). Two cases, one check. A cache file deleted by hand',
             TAB + TAB + TAB + ' * self-heals: its chunk is rebaked and writes it again. And a',
             TAB + TAB + TAB + ' * record written BEFORE this lane existed lists no `.lodj` at all,',
             TAB + TAB + TAB + ' * so every chunk would look clean, nothing would be replayed, and',
             TAB + TAB + TAB + ' * the run would write a valid, self-consistent, EMPTY pair. That is',
             TAB + TAB + TAB + ' * the worst shape this bug has, so it is checked against the DISK',
             TAB + TAB + TAB + ' * and not against the ledger. */',
             TAB + TAB + TAB + 'int lostCache = 0;',
             TAB + TAB + TAB + 'if ( !nativeDir.isEmpty() ) {',
             TAB + TAB + TAB + TAB + 'const QString cdir = lodgenFo4csWorldDir( nativeDir, world.worldspaceEdid() );',
             TAB + TAB + TAB + TAB + 'for ( const LodgenChunkJob & j : allJobs ) {',
             TAB + TAB + TAB + TAB + TAB + 'const QString key = QString( "%1,%2" ).arg( j.cx ).arg( j.cy );',
             TAB + TAB + TAB + TAB + TAB + 'if ( widened.contains( key ) )',
             TAB + TAB + TAB + TAB + TAB + TAB + 'continue;',
             TAB + TAB + TAB + TAB + TAB + 'const QString cp = QString( "%1/%2.%3.%4.%5.lodj" )',
             TAB + TAB + TAB + TAB + TAB + TAB + '.arg( cdir, world.worldspaceEdid() )',
             TAB + TAB + TAB + TAB + TAB + TAB + '.arg( j.dim ).arg( j.cx ).arg( j.cy );',
             TAB + TAB + TAB + TAB + TAB + 'if ( !QFileInfo::exists( cp ) ) {',
             TAB + TAB + TAB + TAB + TAB + TAB + 'widened.insert( key );',
             TAB + TAB + TAB + TAB + TAB + TAB + 'lostCache++;',
             TAB + TAB + TAB + TAB + TAB + TAB + 'if ( reasons.size() < 8 )',
             TAB + TAB + TAB + TAB + TAB + TAB + TAB + 'reasons.append( QString( "  (%1,%2) has no native chunk cache" )',
             TAB + TAB + TAB + TAB + TAB + TAB + TAB + TAB + '.arg( j.cx ).arg( j.cy ) );',
             TAB + TAB + TAB + TAB + TAB + '}',
             TAB + TAB + TAB + TAB + '}',
             TAB + TAB + TAB + '}',
         ]) + NL,
         'spread')

t = sub1(t,
         TAB + TAB + TAB + 'censusOut( QString( "incremental: %1 of %2 chunks dirty "' + NL
         + TAB + TAB + TAB + TAB + TAB + TAB + TAB + '  "(%3 inputs moved, %4 not in the ledger, %5 output lost, "' + NL
         + TAB + TAB + TAB + TAB + TAB + TAB + TAB + '  "%6 by neighbour)" )' + NL
         + TAB + TAB + TAB + TAB + '.arg( kept.size() ).arg( allJobs.size() )' + NL
         + TAB + TAB + TAB + TAB + '.arg( movedInputs ).arg( unknown ).arg( lostOutput ).arg( spread ) );',
         TAB + TAB + TAB + 'censusOut( QString( "incremental: %1 of %2 chunks dirty "' + NL
         + TAB + TAB + TAB + TAB + TAB + TAB + TAB + '  "(%3 inputs moved, %4 not in the ledger, %5 output lost, "' + NL
         + TAB + TAB + TAB + TAB + TAB + TAB + TAB + '  "%6 by neighbour, %7 with no native chunk cache)" )' + NL
         + TAB + TAB + TAB + TAB + '.arg( kept.size() ).arg( allJobs.size() )' + NL
         + TAB + TAB + TAB + TAB + '.arg( movedInputs ).arg( unknown ).arg( lostOutput ).arg( spread )' + NL
         + TAB + TAB + TAB + TAB + '.arg( lostCache ) );',
         'incremental census line')

# ---- 4. the hooks ------------------------------------------------------------
t = sub1(t,
         TAB + TAB + 'QHash<QString, QStringList> producedFiles;' + NL
         + TAB + TAB + '{' + NL
         + TAB + TAB + TAB + 'QString passErr;',
         block([
             TAB + TAB + 'QHash<QString, QStringList> producedFiles;',
             '',
             TAB + TAB + '/* ---- THE PER-CHUNK NATIVE CACHE (lane INCR1, 2026-09-17) --------',
             TAB + TAB + ' *',
             TAB + TAB + ' * Written on every `--native` bake, full or incremental, because a',
             TAB + TAB + ' * cache only helps the run AFTER the one that made it. It is listed',
             TAB + TAB + ' * in the record\'s `out` rows below like every other output, so the',
             TAB + TAB + ' * layout census counts it and editing one by hand makes its chunk',
             TAB + TAB + ' * dirty by the ordinary output check. */',
             TAB + TAB + 'const QString lodjDir = nativeDir.isEmpty() ? QString()',
             TAB + TAB + TAB + ': lodgenFo4csWorldDir( nativeDir, world.worldspaceEdid() );',
             TAB + TAB + 'auto lodjPath = [&]( int dm, int cx, int cy ) {',
             TAB + TAB + TAB + 'return QString( "%1/%2.%3.%4.%5.lodj" )',
             TAB + TAB + TAB + TAB + '.arg( lodjDir, world.worldspaceEdid() ).arg( dm ).arg( cx ).arg( cy );',
             TAB + TAB + '};',
             TAB + TAB + 'int lodjWritten = 0, lodjReplayed = 0, lodjFailed = 0;',
             TAB + TAB + 'qint64 lodjPlacements = 0;',
             TAB + TAB + 'if ( !nativeDir.isEmpty() && gLgNativeCache ) {',
             TAB + TAB + TAB + 'pass.nativeJournalSink = [&]( const LodgenChunkOutcome & r,',
             TAB + TAB + TAB + TAB + 'LodgenNativeJournal * jr ) {',
             TAB + TAB + TAB + TAB + 'const QString cp = lodjPath( r.dim, r.cx, r.cy );',
             TAB + TAB + TAB + TAB + 'QString jerr;',
             TAB + TAB + TAB + TAB + 'if ( !lodgenNativeJournalWriteCache( jr, cp, world.worldspaceEdid(),',
             TAB + TAB + TAB + TAB + TAB + TAB + 'r.dim, r.cx, r.cy, &jerr ) ) {',
             TAB + TAB + TAB + TAB + TAB + 'err() << "native cache (" << r.cx << "," << r.cy << "): "',
             TAB + TAB + TAB + TAB + TAB + TAB + '  << jerr << Qt::endl;',
             TAB + TAB + TAB + TAB + TAB + 'lodjFailed++;',
             TAB + TAB + TAB + TAB + TAB + 'return;',
             TAB + TAB + TAB + TAB + '}',
             TAB + TAB + TAB + TAB + 'lodjWritten++;',
             TAB + TAB + TAB + TAB + '/* the retire callback for this same job appends to this very',
             TAB + TAB + TAB + TAB + ' * list a moment later, and it runs on this thread too. */',
             TAB + TAB + TAB + TAB + 'producedFiles[QString( "%1,%2" ).arg( r.cx ).arg( r.cy )].append( cp );',
             TAB + TAB + TAB + '};',
             TAB + TAB + TAB + 'if ( incremental ) {',
             TAB + TAB + TAB + TAB + 'pass.nativeAllJobs = allJobs;',
             TAB + TAB + TAB + TAB + 'pass.nativeReplayCached = [&]( const LodgenChunkJob & j ) {',
             TAB + TAB + TAB + TAB + TAB + 'QString jerr;',
             TAB + TAB + TAB + TAB + TAB + 'if ( !lodgenNativeReplayCache( lodjPath( j.dim, j.cx, j.cy ), &jerr ) ) {',
             TAB + TAB + TAB + TAB + TAB + TAB + 'err() << "native cache: " << jerr << Qt::endl;',
             TAB + TAB + TAB + TAB + TAB + TAB + 'lodjFailed++;',
             TAB + TAB + TAB + TAB + TAB + TAB + 'return;',
             TAB + TAB + TAB + TAB + TAB + '}',
             TAB + TAB + TAB + TAB + TAB + 'lodjReplayed++;',
             TAB + TAB + TAB + TAB + TAB + 'lodjPlacements += lodgenNativeCacheLastPlacements();',
             TAB + TAB + TAB + TAB + '};',
             TAB + TAB + TAB + '}',
             TAB + TAB + '}',
             '',
             TAB + TAB + '{',
             TAB + TAB + TAB + 'QString passErr;',
         ]),
         'produced files block')

# ---- 5. the census and the shared-arrival refusal ---------------------------
t = sub1(t,
         TAB + TAB + 'if ( lodgenNativeActive() ) {' + NL
         + TAB + TAB + TAB + 'QString nrep, nerr;',
         block([
             TAB + TAB + 'if ( lodgenNativeActive() ) {',
             TAB + TAB + TAB + 'if ( !nativeDir.isEmpty() && gLgNativeCache ) {',
             TAB + TAB + TAB + TAB + 'censusOut( QString( "native cache: %1 chunk(s) written to .lodj, "',
             TAB + TAB + TAB + TAB + TAB + TAB + TAB + TAB + '  "%2 replayed from cache (%3 placement(s)), %4 failure(s), "',
             TAB + TAB + TAB + TAB + TAB + TAB + TAB + TAB + '  "%5 arrival(s) lit by more than one chunk" )',
             TAB + TAB + TAB + TAB + TAB + '.arg( lodjWritten ).arg( lodjReplayed ).arg( lodjPlacements )',
             TAB + TAB + TAB + TAB + TAB + '.arg( lodjFailed ).arg( lodgenNativeSharedArrivals() ) );',
             TAB + TAB + TAB + TAB + 'out().flush();',
             TAB + TAB + TAB + '}',
             TAB + TAB + TAB + 'if ( lodjFailed > 0 ) {',
             TAB + TAB + TAB + TAB + 'err() << "error: " << lodjFailed << " native chunk cache failure(s); the pair "',
             TAB + TAB + TAB + TAB + TAB + TAB + ' "this run would write is missing whole chunks. Bake without "',
             TAB + TAB + TAB + TAB + TAB + TAB + ' "--incremental." << Qt::endl;',
             TAB + TAB + TAB + TAB + 'lodgenNativeEnd();',
             TAB + TAB + TAB + TAB + 'return 1;',
             TAB + TAB + TAB + '}',
             TAB + TAB + TAB + '/* THE ONE CASE THE CACHE CANNOT REPRODUCE BIT FOR BIT, refused',
             TAB + TAB + TAB + ' * rather than hoped through. An arrival is keyed `(refForm,',
             TAB + TAB + TAB + ' * scolPart)` ACROSS chunks, so a placement two chunks both light',
             TAB + TAB + TAB + ' * has sums built from both, and `(prev + a1) + a2` is not',
             TAB + TAB + TAB + ' * `prev + (a1 + a2)` in floating point. Zero is the ordinary',
             TAB + TAB + TAB + ' * answer -- lighting is keyed on the chunk\'s own identity index --',
             TAB + TAB + TAB + ' * and anything else means the pair would be NEARLY right, which is',
             TAB + TAB + TAB + ' * the one thing a bake may not be. */',
             TAB + TAB + TAB + 'if ( lodjReplayed > 0 && lodgenNativeSharedArrivals() > 0 ) {',
             TAB + TAB + TAB + TAB + 'err() << "refused: " << lodgenNativeSharedArrivals()',
             TAB + TAB + TAB + TAB + TAB + '  << " placement(s) were lit by more than one chunk, so a rebuilt "',
             TAB + TAB + TAB + TAB + TAB + TAB + ' "chunk and a cached one would have to have their lighting sums "',
             TAB + TAB + TAB + TAB + TAB + TAB + ' "added in an order this run cannot reproduce." << Qt::endl;',
             TAB + TAB + TAB + TAB + 'err() << "  bake without --incremental: a full bake adds them in the one "',
             TAB + TAB + TAB + TAB + TAB + TAB + ' "order there is." << Qt::endl;',
             TAB + TAB + TAB + TAB + 'lodgenNativeEnd();',
             TAB + TAB + TAB + TAB + 'return 1;',
             TAB + TAB + TAB + '}',
             TAB + TAB + TAB + 'QString nrep, nerr;',
         ]),
         'native active block')

if CHECK:
    print('  would write %s (%d bytes)' % (rel, len(t.encode('utf-8'))))
else:
    with io.open(os.path.join(ROOT, rel), 'wb') as fh:
        fh.write(t.encode('utf-8'))
    print('  wrote %s (%d bytes)' % (rel, len(t.encode('utf-8'))))
