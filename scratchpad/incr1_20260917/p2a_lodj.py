"""INCR1 step 2, patch A+B -- the per-chunk native cache (`.lodj`).

Adds to src/nativeemit.{h,cpp}:
  * `lodgenNativeJournalWriteCache()` -- reduce one chunk job's journal to the
    contribution it makes to the region's arrivals, and write it beside the
    bake record.
  * `lodgenNativeReplayCache()`       -- put that contribution back, in the
    position in the chunk queue where the job itself would have gone.
  * the SHARED-ARRIVAL guard: a placement lit by more than one chunk is the one
    case the reduction cannot reproduce bit for bit, so it is counted and the
    driver refuses on it rather than writing a pair that is nearly right.

Anchors asserted count == 1. Escapes from chr(). LF preserved.
"""
import io
import os
import sys

NL = chr(10)
TAB = chr(9)
Q = chr(34)
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                    '..', '..'))
CHECK = '--check' in sys.argv[1:]


def load(rel):
    with io.open(os.path.join(ROOT, rel), 'rb') as fh:
        b = fh.read()
    assert b.count(chr(13).encode()) == 0, rel + ' is not LF-only'
    return b.decode('utf-8')


def save(rel, text):
    if CHECK:
        print('  would write %s (%d bytes)' % (rel, len(text.encode('utf-8'))))
        return
    with io.open(os.path.join(ROOT, rel), 'wb') as fh:
        fh.write(text.encode('utf-8'))
    print('  wrote %s (%d bytes)' % (rel, len(text.encode('utf-8'))))


def sub1(text, old, new, what):
    n = text.count(old)
    if n != 1:
        raise SystemExit('ANCHOR %s: found %d times, wanted exactly 1' % (what, n))
    return text.replace(old, new, 1)


def block(lines):
    return NL.join(lines)


# =============================================================== nativeemit.h
rel = 'src/nativeemit.h'
t = load(rel)
if 'lodgenNativeReplayCache' in t:
    raise SystemExit(rel + ' already declares the cache')

anchor = 'void lodgenNativeJournalDestroy( LodgenNativeJournal * j );'
decl = block([
    anchor,
    '',
    '/* ---- THE PER-CHUNK NATIVE CACHE (`.lodj`, lane INCR1, 2026-09-17) -------',
    ' *',
    ' * WHY IT EXISTS. `--native` was on `--incremental`\'s whole-region refusal',
    ' * list, which meant the incremental rebake refused every command the FO4CS',
    ' * pipeline is made of. The pair is aggregated from `s.arrivals`, and a chunk',
    ' * that is SKIPPED contributes nothing to it, so the skipped chunk\'s',
    ' * contribution has to come from somewhere.',
    ' *',
    ' * WHY NOT OUT OF THE PREVIOUS `.lodi` (the route with no new file). Four',
    ' * things the aggregate needs are provably NOT in that file:',
    ' *   1. `NativePlacement::model` -- the path of the mesh the chunk actually',
    ' *      DREW. Never written: `lodifile.cpp` uses `baseName` only in refusal',
    ' *      text (:291, :294). Without it `models[foldPath(p.model)]` cannot be',
    ' *      looked up, so the occluder box cannot be rebuilt.',
    ' *   2. the model-space occluder box. The file stores the WORLD-space,',
    ' *      0.999-shrunk box with the quantised scale already applied, and only',
    ' *      for the at most LODI_OCCLUDERS_PER_CELL = 4 per cell that survived',
    ' *      the writer\'s cull (`lodifile.cpp:466`). The rest are not in it.',
    ' *   3. `NativePlacement::isTree` -- the aggregate\'s tree list keys on it',
    ' *      and no `.lodi` instance flag carries it.',
    ' *   4. the lighting ACCUMULATORS. The file carries the 8-bit mean, not the',
    ' *      sum and the count, so two chunks\' contributions cannot be recombined.',
    ' * So the clean route is closed, and this is a NEW FILE: a stated divergence',
    ' * for bungo, listed in the bake record\'s `out` rows and counted by the',
    ' * layout census like every other output.',
    ' *',
    ' * WHAT IT HOLDS. Not the raw journal -- lighting is one event per VERTEX,',
    ' * millions a region -- but the chunk\'s REDUCTION: every placement it',
    ' * emitted, in emission order, and one row per lit placement with the exact',
    ' * double sums and counts. Replayed in the chunk\'s own queue position the',
    ' * arrivals come out bit for bit as a full bake\'s, because a chunk\'s events',
    ' * are contiguous and its sums start from zero.',
    ' *',
    ' * THE ONE CASE THAT IS NOT EXACT, and it is guarded rather than hoped: an',
    ' * arrival is keyed `(refForm, scolPart)` ACROSS chunks, so a placement two',
    ' * chunks both light has a sum built from both, and `(prev + a1) + a2` is not',
    ' * `prev + (a1 + a2)` in floating point. Those are COUNTED',
    ' * (`lodgenNativeSharedArrivals()`) and an incremental `--native` run refuses',
    ' * when the count is not zero. */',
    '',
    '//! Reduce one chunk job\'s journal and write it. `path` is created; the',
    '//! caller notes it as a layout file and as one of the chunk\'s outputs.',
    'bool lodgenNativeJournalWriteCache( const LodgenNativeJournal * j, const QString & path,',
    '\tconst QString & wsEdid, int dim, int cx, int cy, QString * error );',
    '',
    '//! Put a cached chunk\'s contribution back, at the point in the queue order',
    '//! where its job would have been replayed.',
    'bool lodgenNativeReplayCache( const QString & path, QString * error );',
    '',
    '//! Placements lit by more than one chunk. Zero is the only value for which a',
    '//! cached replay is bit-exact; the driver refuses otherwise.',
    'int lodgenNativeSharedArrivals();',
    '',
    '//! Placements the last cache write or read carried, for the census.',
    'int lodgenNativeCacheLastPlacements();',
])
t = sub1(t, anchor, decl, 'h journal destroy')
save(rel, t)

# ============================================================= nativeemit.cpp
rel = 'src/nativeemit.cpp'
t = load(rel)
if 'lodgenNativeReplayCache' in t:
    raise SystemExit(rel + ' already carries the cache')

# ---- 0. QFileInfo, for the cache's mkpath -----------------------------------
t = sub1(t, '#include <QFile>' + NL, '#include <QFile>' + NL + '#include <QFileInfo>' + NL,
         'cpp QFile include')

# ---- 1. Arrival gains the chunk that lit it ---------------------------------
t = sub1(t,
         TAB + 'double paoSum = 0.0;' + NL
         + TAB + 'int paoRays = 0;' + NL
         + TAB + 'int paoDim = 0;' + NL
         + '};',
         TAB + 'double paoSum = 0.0;' + NL
         + TAB + 'int paoRays = 0;' + NL
         + TAB + 'int paoDim = 0;' + NL
         + TAB + '/* WHICH CHUNK LIT IT (lane INCR1, 2026-09-17). An arrival is keyed' + NL
         + TAB + ' * `(refForm, scolPart)` across the whole region, so two chunks CAN' + NL
         + TAB + ' * both light one -- and then its sums are built from both, and the' + NL
         + TAB + ' * per-chunk cache cannot reproduce the addition order. Counted here' + NL
         + TAB + ' * so the incremental driver can refuse on it instead of writing a' + NL
         + TAB + ' * pair that is nearly right. */' + NL
         + TAB + 'int litCx = 0, litCy = 0;' + NL
         + TAB + 'bool litSeen = false;' + NL
         + TAB + 'bool shared = false;' + NL
         + '};',
         'Arrival tail')

# ---- 2. the counter in State ------------------------------------------------
t = sub1(t,
         TAB + 'quint64 arrivalsSeen = 0;',
         TAB + 'quint64 arrivalsSeen = 0;' + NL
         + TAB + 'int sharedArrivals = 0;      //!< placements more than one chunk lit',
         'State arrivalsSeen')

# ---- 3. lodgenNativeLighting counts the sharing -----------------------------
t = sub1(t,
         TAB + 'a.litDim = dim;' + NL
         + TAB + 'a.aoSum += ao;' + NL
         + TAB + 'a.skySum += sky;' + NL
         + TAB + 'a.groundSum += ground;' + NL
         + TAB + 'a.litVerts++;' + NL
         + '}',
         TAB + '/* THE SHARING COUNT (lane INCR1): the first chunk to light this' + NL
         + TAB + ' * arrival puts its name on it; a second one is counted once. */' + NL
         + TAB + 'if ( !a.litSeen ) {' + NL
         + TAB + TAB + 'a.litSeen = true;' + NL
         + TAB + TAB + 'a.litCx = chunkX;' + NL
         + TAB + TAB + 'a.litCy = chunkY;' + NL
         + TAB + '} else if ( ( a.litCx != chunkX || a.litCy != chunkY ) && !a.shared ) {' + NL
         + TAB + TAB + 'a.shared = true;' + NL
         + TAB + TAB + 's.sharedArrivals++;' + NL
         + TAB + '}' + NL
         + TAB + 'a.litDim = dim;' + NL
         + TAB + 'a.aoSum += ao;' + NL
         + TAB + 'a.skySum += sky;' + NL
         + TAB + 'a.groundSum += ground;' + NL
         + TAB + 'a.litVerts++;' + NL
         + '}',
         'lighting accumulate')

# ---- 4. the cache itself, after the journal replay --------------------------
anchor = ('void lodgenNativeJournalReplay( LodgenNativeJournal * j )')
cache = block([
    '/* ---- THE PER-CHUNK NATIVE CACHE (`.lodj`, lane INCR1, 2026-09-17) -------',
    ' *',
    ' * The format, and why every float is a hex bit pattern: the whole point of',
    ' * the file is that the arrivals it rebuilds are BIT-IDENTICAL to the ones the',
    ' * chunk made, and a decimal round trip is not. It is otherwise the tree\'s own',
    ' * plain-text shape -- UTF-8, LF, `key<TAB>fields` -- so `lodj_read.py` is a',
    ' * dozen lines and the gates never need a binary reader.',
    ' *',
    ' *   lodj<TAB>1<TAB><ws><TAB><dim><TAB><cx><TAB><cy>',
    ' *   placements<TAB><n>',
    ' *   p<TAB>base<TAB>ref<TAB>scolPart<TAB>pos*3<TAB>rot*9<TAB>scale<TAB>slot',
    ' *     <TAB>isTree<TAB>mirrorU<TAB>treeHash<TAB>hasAlpha<TAB>emits',
    ' *     <TAB>objectIndex<TAB>model',
    ' *   lit<TAB><n>',
    ' *   l<TAB>objectIndex<TAB>aoSum<TAB>skySum<TAB>groundSum<TAB>verts',
    ' *   pao<TAB><n>',
    ' *   a<TAB>objectIndex<TAB>paoSum<TAB>rays',
    ' *   end<TAB><placements><TAB><lit><TAB><pao>',
    ' *',
    ' * The rows keep FIRST-APPEARANCE order and each row\'s sums are accumulated in',
    ' * the order the events arrived, which is the order the accumulator itself',
    ' * would have added them in. */',
    '',
    'static int g_cacheLastPlacements = 0;',
    '',
    'static QString f32hex( float v )',
    '{',
    TAB + 'quint32 b = 0;',
    TAB + 'std::memcpy( &b, &v, 4 );',
    TAB + 'return QString::number( b, 16 ).rightJustified( 8, QChar( ' + chr(39) + '0' + chr(39) + ' ) );',
    '}',
    '',
    'static float hexf32( const QString & s )',
    '{',
    TAB + 'const quint32 b = s.toUInt( nullptr, 16 );',
    TAB + 'float v = 0.0f;',
    TAB + 'std::memcpy( &v, &b, 4 );',
    TAB + 'return v;',
    '}',
    '',
    'static QString f64hex( double v )',
    '{',
    TAB + 'quint64 b = 0;',
    TAB + 'std::memcpy( &b, &v, 8 );',
    TAB + 'return QString::number( b, 16 ).rightJustified( 16, QChar( ' + chr(39) + '0' + chr(39) + ' ) );',
    '}',
    '',
    'static double hexf64( const QString & s )',
    '{',
    TAB + 'const quint64 b = s.toULongLong( nullptr, 16 );',
    TAB + 'double v = 0.0;',
    TAB + 'std::memcpy( &v, &b, 8 );',
    TAB + 'return v;',
    '}',
    '',
    'int lodgenNativeSharedArrivals()',
    '{',
    TAB + 'return st().sharedArrivals;',
    '}',
    '',
    'int lodgenNativeCacheLastPlacements()',
    '{',
    TAB + 'return g_cacheLastPlacements;',
    '}',
    '',
    '//! The bulk form of `lodgenNativeLighting`: the chunk\'s whole contribution to',
    '//! one arrival at once, under the same finest-ring-wins rule.',
    'static void lightingBulk( int cx, int cy, int dim, int objectIndex,',
    TAB + 'double ao, double sky, double ground, int verts )',
    '{',
    TAB + 'State & s = st();',
    TAB + 'auto it = s.byObject.find( std::make_tuple( cx, cy, dim, objectIndex ) );',
    TAB + 'if ( it == s.byObject.end() )',
    TAB + TAB + 'return;',
    TAB + 'Arrival & a = s.arrivals[size_t( it->second )];',
    TAB + 'if ( a.litVerts && a.litDim != dim ) {',
    TAB + TAB + 'if ( dim > a.litDim )',
    TAB + TAB + TAB + 'return;',
    TAB + TAB + 'a.aoSum = a.skySum = a.groundSum = 0.0;',
    TAB + TAB + 'a.litVerts = 0;',
    TAB + '}',
    TAB + 'if ( !a.litSeen ) {',
    TAB + TAB + 'a.litSeen = true;',
    TAB + TAB + 'a.litCx = cx;',
    TAB + TAB + 'a.litCy = cy;',
    TAB + '} else if ( ( a.litCx != cx || a.litCy != cy ) && !a.shared ) {',
    TAB + TAB + 'a.shared = true;',
    TAB + TAB + 's.sharedArrivals++;',
    TAB + '}',
    TAB + 'a.litDim = dim;',
    TAB + 'a.aoSum += ao;',
    TAB + 'a.skySum += sky;',
    TAB + 'a.groundSum += ground;',
    TAB + 'a.litVerts += verts;',
    '}',
    '',
    'static void placementAoBulk( int cx, int cy, int dim, int objectIndex, double ao, int rays )',
    '{',
    TAB + 'State & s = st();',
    TAB + 'if ( !s.placementAo )',
    TAB + TAB + 'return;',
    TAB + 'auto it = s.byObject.find( std::make_tuple( cx, cy, dim, objectIndex ) );',
    TAB + 'if ( it == s.byObject.end() )',
    TAB + TAB + 'return;',
    TAB + 'Arrival & a = s.arrivals[size_t( it->second )];',
    TAB + 'if ( a.paoRays && a.paoDim != dim ) {',
    TAB + TAB + 'if ( dim > a.paoDim )',
    TAB + TAB + TAB + 'return;',
    TAB + TAB + 'a.paoSum = 0.0;',
    TAB + TAB + 'a.paoRays = 0;',
    TAB + '}',
    TAB + 'a.paoDim = dim;',
    TAB + 'a.paoSum += ao;',
    TAB + 'a.paoRays += rays;',
    '}',
    '',
    'namespace',
    '{',
    'struct CacheRow',
    '{',
    TAB + 'int objectIndex = 0;',
    TAB + 'double ao = 0.0, sky = 0.0, ground = 0.0;',
    TAB + 'int n = 0;',
    '};',
    '}',
    '',
    'bool lodgenNativeJournalWriteCache( const LodgenNativeJournal * j, const QString & path,',
    TAB + 'const QString & wsEdid, int dim, int cx, int cy, QString * error )',
    '{',
    TAB + 'auto fail = [&]( const QString & m ) { if ( error ) *error = m; return false; };',
    TAB + 'if ( !j )',
    TAB + TAB + 'return fail( QStringLiteral( "no journal for chunk (%1,%2): the chunk pass was not '
    + 'asked to record one" ).arg( cx ).arg( cy ) );',
    '',
    TAB + 'QVector<NativePlacement> ps;',
    TAB + 'QVector<CacheRow> lit, pao;',
    TAB + 'QHash<int, int> litAt, paoAt;',
    TAB + 'for ( const LodgenNativeJournalEvent & e : j->events ) {',
    TAB + TAB + 'if ( e.kind == LNE_PLACEMENT ) {',
    TAB + TAB + TAB + 'if ( e.p.chunkX != cx || e.p.chunkY != cy || e.p.dim != dim )',
    TAB + TAB + TAB + TAB + 'return fail( QStringLiteral( "chunk (%1,%2) dim %3 journal carries a '
    + 'placement from (%4,%5) dim %6" ).arg( cx ).arg( cy ).arg( dim )',
    TAB + TAB + TAB + TAB + TAB + '.arg( e.p.chunkX ).arg( e.p.chunkY ).arg( e.p.dim ) );',
    TAB + TAB + TAB + 'ps.append( e.p );',
    TAB + TAB + TAB + 'continue;',
    TAB + TAB + '}',
    TAB + TAB + 'if ( e.chunkX != cx || e.chunkY != cy || e.dim != dim )',
    TAB + TAB + TAB + 'return fail( QStringLiteral( "chunk (%1,%2) dim %3 journal carries lighting '
    + 'from (%4,%5) dim %6" ).arg( cx ).arg( cy ).arg( dim )',
    TAB + TAB + TAB + TAB + '.arg( e.chunkX ).arg( e.chunkY ).arg( e.dim ) );',
    TAB + TAB + 'QVector<CacheRow> & rows = ( e.kind == LNE_LIGHTING ) ? lit : pao;',
    TAB + TAB + 'QHash<int, int> & at = ( e.kind == LNE_LIGHTING ) ? litAt : paoAt;',
    TAB + TAB + 'auto f = at.constFind( e.objectIndex );',
    TAB + TAB + 'int idx;',
    TAB + TAB + 'if ( f == at.constEnd() ) {',
    TAB + TAB + TAB + 'idx = rows.size();',
    TAB + TAB + TAB + 'CacheRow r;',
    TAB + TAB + TAB + 'r.objectIndex = e.objectIndex;',
    TAB + TAB + TAB + 'rows.append( r );',
    TAB + TAB + TAB + 'at.insert( e.objectIndex, idx );',
    TAB + TAB + '} else {',
    TAB + TAB + TAB + 'idx = f.value();',
    TAB + TAB + '}',
    TAB + TAB + 'CacheRow & r = rows[idx];',
    TAB + TAB + 'r.ao += e.ao;',
    TAB + TAB + 'if ( e.kind == LNE_LIGHTING ) {',
    TAB + TAB + TAB + 'r.sky += e.sky;',
    TAB + TAB + TAB + 'r.ground += e.ground;',
    TAB + TAB + '}',
    TAB + TAB + 'r.n++;',
    TAB + '}',
    '',
    TAB + 'QDir().mkpath( QFileInfo( path ).absolutePath() );',
    TAB + 'QFile f( path );',
    TAB + 'if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )',
    TAB + TAB + 'return fail( QStringLiteral( "cannot write the chunk cache %1" ).arg( path ) );',
    TAB + 'QByteArray o;',
    TAB + 'const QChar T( ' + chr(39) + chr(92) + 't' + chr(39) + ' );',
    TAB + 'auto line = [&]( const QString & s ) { o += s.toUtf8(); o += ' + chr(39) + chr(92)
    + 'n' + chr(39) + '; };',
    TAB + 'line( QStringLiteral( "lodj" ) + T + QStringLiteral( "1" ) + T + wsEdid + T',
    TAB + TAB + '+ QString::number( dim ) + T + QString::number( cx ) + T + QString::number( cy ) );',
    TAB + 'line( QStringLiteral( "placements" ) + T + QString::number( ps.size() ) );',
    TAB + 'for ( const NativePlacement & p : ps ) {',
    TAB + TAB + 'QString s = QStringLiteral( "p" );',
    TAB + TAB + 's += T + QString::number( p.baseForm, 16 );',
    TAB + TAB + 's += T + QString::number( p.refForm, 16 );',
    TAB + TAB + 's += T + QString::number( p.scolPart );',
    TAB + TAB + 'for ( int k = 0; k < 3; k++ )',
    TAB + TAB + TAB + 's += T + f32hex( p.pos[k] );',
    TAB + TAB + 'for ( int k = 0; k < 9; k++ )',
    TAB + TAB + TAB + 's += T + f32hex( p.rot[k] );',
    TAB + TAB + 's += T + f32hex( p.scale );',
    TAB + TAB + 's += T + QString::number( p.slot );',
    TAB + TAB + 's += T + QString::number( p.isTree ? 1 : 0 );',
    TAB + TAB + 's += T + QString::number( p.mirrorU ? 1 : 0 );',
    TAB + TAB + 's += T + QString::number( p.treeHash, 16 );',
    TAB + TAB + 's += T + QString::number( p.hasAlpha ? 1 : 0 );',
    TAB + TAB + 's += T + QString::number( p.emits ? 1 : 0 );',
    TAB + TAB + 's += T + QString::number( p.objectIndex );',
    TAB + TAB + 's += T + p.model;',
    TAB + TAB + 'line( s );',
    TAB + '}',
    TAB + 'line( QStringLiteral( "lit" ) + T + QString::number( lit.size() ) );',
    TAB + 'for ( const CacheRow & r : lit )',
    TAB + TAB + 'line( QStringLiteral( "l" ) + T + QString::number( r.objectIndex ) + T',
    TAB + TAB + TAB + '+ f64hex( r.ao ) + T + f64hex( r.sky ) + T + f64hex( r.ground ) + T',
    TAB + TAB + TAB + '+ QString::number( r.n ) );',
    TAB + 'line( QStringLiteral( "pao" ) + T + QString::number( pao.size() ) );',
    TAB + 'for ( const CacheRow & r : pao )',
    TAB + TAB + 'line( QStringLiteral( "a" ) + T + QString::number( r.objectIndex ) + T',
    TAB + TAB + TAB + '+ f64hex( r.ao ) + T + QString::number( r.n ) );',
    TAB + 'line( QStringLiteral( "end" ) + T + QString::number( ps.size() ) + T',
    TAB + TAB + '+ QString::number( lit.size() ) + T + QString::number( pao.size() ) );',
    TAB + 'if ( f.write( o ) != o.size() || !f.flush() )',
    TAB + TAB + 'return fail( QStringLiteral( "short write on the chunk cache %1" ).arg( path ) );',
    TAB + 'f.close();',
    TAB + 'g_cacheLastPlacements = ps.size();',
    TAB + 'return true;',
    '}',
    '',
    'bool lodgenNativeReplayCache( const QString & path, QString * error )',
    '{',
    TAB + 'auto fail = [&]( const QString & m ) { if ( error ) *error = m; return false; };',
    TAB + 'QFile f( path );',
    TAB + 'if ( !f.open( QIODevice::ReadOnly ) )',
    TAB + TAB + 'return fail( QStringLiteral( "the chunk cache %1 is not there" ).arg( path ) );',
    TAB + 'const QStringList lines = QString::fromUtf8( f.readAll() )',
    TAB + TAB + '.split( QChar( ' + chr(39) + chr(92) + 'n' + chr(39) + ' ), Qt::SkipEmptyParts );',
    TAB + 'f.close();',
    TAB + 'if ( lines.isEmpty() || !lines.at( 0 ).startsWith( QLatin1String( "lodj' + chr(92) + 't1'
    + chr(92) + 't" ) ) )',
    TAB + TAB + 'return fail( QStringLiteral( "%1 is not a version 1 chunk cache" ).arg( path ) );',
    TAB + 'const QStringList h = lines.at( 0 ).split( QChar( ' + chr(39) + chr(92) + 't' + chr(39) + ' ) );',
    TAB + 'if ( h.size() < 6 )',
    TAB + TAB + 'return fail( QStringLiteral( "%1 has a short header line" ).arg( path ) );',
    TAB + 'const int dim = h.at( 3 ).toInt(), cx = h.at( 4 ).toInt(), cy = h.at( 5 ).toInt();',
    TAB + 'int np = 0, nl = 0, na = 0, sawP = 0, sawL = 0, sawA = 0;',
    TAB + 'QVector<NativePlacement> ps;',
    TAB + 'for ( int i = 1; i < lines.size(); i++ ) {',
    TAB + TAB + 'const QStringList fd = lines.at( i ).split( QChar( ' + chr(39) + chr(92) + 't'
    + chr(39) + ' ) );',
    TAB + TAB + 'const QString & k = fd.at( 0 );',
    TAB + TAB + 'if ( k == QLatin1String( "placements" ) ) { np = fd.value( 1 ).toInt(); continue; }',
    TAB + TAB + 'if ( k == QLatin1String( "lit" ) ) { nl = fd.value( 1 ).toInt(); continue; }',
    TAB + TAB + 'if ( k == QLatin1String( "pao" ) ) { na = fd.value( 1 ).toInt(); continue; }',
    TAB + TAB + 'if ( k == QLatin1String( "end" ) ) {',
    TAB + TAB + TAB + 'if ( fd.value( 1 ).toInt() != sawP || fd.value( 2 ).toInt() != sawL',
    TAB + TAB + TAB + TAB + '|| fd.value( 3 ).toInt() != sawA )',
    TAB + TAB + TAB + TAB + 'return fail( QStringLiteral( "%1 is truncated: its end line says %2/%3/%4 '
    + 'and it carries %5/%6/%7" ).arg( path ).arg( fd.value( 1 ) ).arg( fd.value( 2 ) )',
    TAB + TAB + TAB + TAB + TAB + '.arg( fd.value( 3 ) ).arg( sawP ).arg( sawL ).arg( sawA ) );',
    TAB + TAB + TAB + 'continue;',
    TAB + TAB + '}',
    TAB + TAB + 'if ( k == QLatin1String( "p" ) ) {',
    TAB + TAB + TAB + 'if ( fd.size() < 22 )',
    TAB + TAB + TAB + TAB + 'return fail( QStringLiteral( "%1 line %2: a placement row of %3 fields" )',
    TAB + TAB + TAB + TAB + TAB + '.arg( path ).arg( i + 1 ).arg( fd.size() ) );',
    TAB + TAB + TAB + 'NativePlacement p;',
    TAB + TAB + TAB + 'int c = 1;',
    TAB + TAB + TAB + 'p.baseForm = fd.at( c++ ).toUInt( nullptr, 16 );',
    TAB + TAB + TAB + 'p.refForm = fd.at( c++ ).toUInt( nullptr, 16 );',
    TAB + TAB + TAB + 'p.scolPart = fd.at( c++ ).toInt();',
    TAB + TAB + TAB + 'for ( int k2 = 0; k2 < 3; k2++ )',
    TAB + TAB + TAB + TAB + 'p.pos[k2] = hexf32( fd.at( c++ ) );',
    TAB + TAB + TAB + 'for ( int k2 = 0; k2 < 9; k2++ )',
    TAB + TAB + TAB + TAB + 'p.rot[k2] = hexf32( fd.at( c++ ) );',
    TAB + TAB + TAB + 'p.scale = hexf32( fd.at( c++ ) );',
    TAB + TAB + TAB + 'p.slot = fd.at( c++ ).toInt();',
    TAB + TAB + TAB + 'p.isTree = fd.at( c++ ).toInt() != 0;',
    TAB + TAB + TAB + 'p.mirrorU = fd.at( c++ ).toInt() != 0;',
    TAB + TAB + TAB + 'p.treeHash = fd.at( c++ ).toUInt( nullptr, 16 );',
    TAB + TAB + TAB + 'p.hasAlpha = fd.at( c++ ).toInt() != 0;',
    TAB + TAB + TAB + 'p.emits = fd.at( c++ ).toInt() != 0;',
    TAB + TAB + TAB + 'p.objectIndex = fd.at( c++ ).toInt();',
    TAB + TAB + TAB + '/* the model path is the LAST field and may hold anything a path may,',
    TAB + TAB + TAB + ' * so it is what is left of the line rather than one split field. */',
    TAB + TAB + TAB + 'p.model = QStringList( fd.mid( c ) ).join( QChar( ' + chr(39) + chr(92) + 't'
    + chr(39) + ' ) );',
    TAB + TAB + TAB + 'p.chunkX = cx;',
    TAB + TAB + TAB + 'p.chunkY = cy;',
    TAB + TAB + TAB + 'p.dim = dim;',
    TAB + TAB + TAB + 'ps.append( p );',
    TAB + TAB + TAB + 'sawP++;',
    TAB + TAB + TAB + 'continue;',
    TAB + TAB + '}',
    TAB + TAB + 'if ( k == QLatin1String( "l" ) ) { sawL++; continue; }',
    TAB + TAB + 'if ( k == QLatin1String( "a" ) ) { sawA++; continue; }',
    TAB + '}',
    TAB + 'if ( sawP != np || sawL != nl || sawA != na )',
    TAB + TAB + 'return fail( QStringLiteral( "%1 promises %2/%3/%4 rows and carries %5/%6/%7" )',
    TAB + TAB + TAB + '.arg( path ).arg( np ).arg( nl ).arg( na ).arg( sawP ).arg( sawL ).arg( sawA ) );',
    '',
    TAB + '/* THE REPLAY, in the file\'s own order: every placement first, exactly as',
    TAB + ' * the chunk emitted them, then the sums. A row lands on the arrival',
    TAB + ' * `byObject` names, so the order among rows cannot move a value. */',
    TAB + 'for ( const NativePlacement & p : ps )',
    TAB + TAB + 'lodgenNativeAddPlacement( p );',
    TAB + 'for ( int i = 1; i < lines.size(); i++ ) {',
    TAB + TAB + 'const QStringList fd = lines.at( i ).split( QChar( ' + chr(39) + chr(92) + 't'
    + chr(39) + ' ) );',
    TAB + TAB + 'if ( fd.at( 0 ) == QLatin1String( "l" ) && fd.size() >= 6 )',
    TAB + TAB + TAB + 'lightingBulk( cx, cy, dim, fd.at( 1 ).toInt(), hexf64( fd.at( 2 ) ),',
    TAB + TAB + TAB + TAB + 'hexf64( fd.at( 3 ) ), hexf64( fd.at( 4 ) ), fd.at( 5 ).toInt() );',
    TAB + TAB + 'else if ( fd.at( 0 ) == QLatin1String( "a" ) && fd.size() >= 4 )',
    TAB + TAB + TAB + 'placementAoBulk( cx, cy, dim, fd.at( 1 ).toInt(), hexf64( fd.at( 2 ) ),',
    TAB + TAB + TAB + TAB + 'fd.at( 3 ).toInt() );',
    TAB + '}',
    TAB + 'g_cacheLastPlacements = ps.size();',
    TAB + 'return true;',
    '}',
    '',
]) + anchor
t = sub1(t, anchor, cache, 'journal replay anchor')
save(rel, t)
print(NL + 'done')
