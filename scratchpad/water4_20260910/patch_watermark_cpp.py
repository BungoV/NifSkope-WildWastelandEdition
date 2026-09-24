# -*- coding: utf-8 -*-
"""patch_watermark_cpp.py -- src/watermark.cpp: the potential-flow solve
replaces the harmonic fill; the dye plane; the flow gates in the selftest.
Every anchor must match exactly once; CR count unchanged (0)."""
import os
import sys
sys.path.insert(0, 'scratchpad/water4_20260910')
from splice import splice   # noqa

HERE = 'scratchpad/water4_20260910'
C = 'src/watermark.cpp'
new_solve = open(os.path.join(HERE, 'new_solve.cpp'), encoding='utf-8').read()
new_gates = open(os.path.join(HERE, 'new_gates.cpp'), encoding='utf-8').read()

# ---- 1. replace the old fill + solve() wholesale ---------------------------
b = open(C, 'rb').read()
s = b.decode('utf-8')
a0 = '/*! The constrained harmonic fill, per marked body.\n'
a1 = '/*! The word the WRITER puts on every texel of a body nobody marked.\n'
assert s.count(a0) == 1 and s.count(a1) == 1
i0, i1 = s.index(a0), s.index(a1)
assert i0 < i1
s = s[:i0] + new_solve + '\n' + s[i1:]
open(C, 'wb').write(s.encode('utf-8'))
print('%s: solve() replaced (%d -> %d bytes)' % (C, len(b), len(s.encode('utf-8'))))

splice(C, [
    # ---- constants and includes ----
    ('constexpr qsizetype kOffStrokeL = 0xF0;\n',
     'constexpr qsizetype kOffDye     = 0xF4;   //!< the reserved word: the dye plane (WATER4)\n'
     'constexpr quint32 kSectDye = LODL_SECT_DYE;\n', 'after'),
    ('#include <algorithm>\n#include <cmath>\n#include <vector>\n',
     '#include <QList>\n#include <QPointF>\n#include <functional>\n#include <limits>\n', 'after'),
    # ---- the Field ----
    ('\tbool zero = false;             //!< locked to still water\n',
     '''\tbool windowed = false;         //!< a cut around the constraints, not the whole bbox
\tbool dyeOnly = false;          //!< a receiver's field: dye is written, flow words are not
\tWaterFlowGrid grid;            //!< the compacted system over `mask`
\tstd::vector<double> phi;       //!< the potential, per grid cell
\tstd::vector<quint32> dye;      //!< the dye word per texel, 0 = none
''', 'after'),
    # ---- flowWordOf: a dye-only field writes no flow ----
    ('\t\tconst Field * F = it.value();\n\t\tif ( F->zero )\n\t\t\treturn 0;\n\t\tif ( F->holds( px, py ) ) {\n',
     '\t\tconst Field * F = it.value();\n\t\tif ( F->dyeOnly )\n\t\t\treturn automaticWord( id );\n\t\tif ( F->zero )\n\t\t\treturn 0;\n\t\tif ( F->holds( px, py ) ) {\n', 'replace'),
    # ---- the stroke codec: a DyePin carries RGBA after its points ----
    ('\t\t\ts.pts.append( pt );\n\t\t}\n\t\tmarks.append( s );\n',
     '''\t\t\ts.pts.append( pt );
\t\t}
\t\tif ( s.kind == WaterStroke::DyePin && 20 + qsizetype( n ) * 8 + 4 <= qsizetype( recBytes ) )
\t\t\tfor ( int c = 0; c < 4; c++ )
\t\t\t\ts.colour[c] = quint8( p[20 + n * 8 + c] );
\t\tmarks.append( s );
''', 'replace'),
    ('\t\tconst quint32 recBytes = quint32( 20 + m.pts.size() * 8 );\n',
     '\t\tconst quint32 recBytes = quint32( 20 + m.pts.size() * 8\n'
     '\t\t\t+ ( m.kind == WaterStroke::DyePin ? 4 : 0 ) );\n', 'replace'),
    ('\t\tfor ( const WaterStrokePoint & p : m.pts ) {\n\t\t\tr.f32( p.x );\n\t\t\tr.f32( p.y );\n\t\t}\n\t\ts.b.append( r.b );\n',
     '''\t\tfor ( const WaterStrokePoint & p : m.pts ) {
\t\t\tr.f32( p.x );
\t\t\tr.f32( p.y );
\t\t}
\t\tif ( m.kind == WaterStroke::DyePin )
\t\t\tfor ( int c = 0; c < 4; c++ )
\t\t\t\tr.u8( m.colour[c] );
\t\ts.b.append( r.b );
''', 'replace'),
    # ---- readTail: the dye offset ----
    ('\tstrokeLen = rd32( p + kOffStrokeL );\n',
     '\toDye = ( sect & kSectDye ) ? quint64( rd32( p + kOffDye ) ) : quint64( 0 );\n'
     '\tif ( ( sect & kSectDye ) && !oDye )\n'
     '\t\treturn fail( QStringLiteral( "section dye is declared present but its offset is empty" ) );\n', 'after'),
    ('\tif ( !( oBody < oStroke && oStroke < oId && oId < oFlow\n\t\t&& ( !oShore || ( oFlow < oShore && oShore < sizeOnDisk ) ) ) )\n',
     '\tif ( oDye && !( oDye > oFlow && oDye > oShore && oDye < sizeOnDisk ) )\n'
     '\t\treturn fail( QStringLiteral( "the dye plane at 0x%1 is not after the other planes; this "\n'
     '\t\t\t"tool rewrites the tail in the writer\'s own order and refuses another" )\n'
     '\t\t\t.arg( oDye, 0, 16 ) );\n', 'before'),
    # ---- open(): read the dye plane ----
    ('\tif ( oShore && !readPlane( *reader, oShore, 1, shorePlane, error ) )\n\t\treturn false;\n\topened = true;\n',
     '\tif ( oShore && !readPlane( *reader, oShore, 1, shorePlane, error ) )\n\t\treturn false;\n'
     '\tdyePlane = Plane();\n'
     '\tif ( oDye && !readPlane( *reader, oDye, 4, dyePlane, error ) )\n\t\treturn false;\n\topened = true;\n', 'replace'),
    # ---- packDyePlane, beside packFlowPlane ----
    ('bool WaterMarkDoc::tableRepackMatches() const\n',
     '''/*! The dye plane: the same packer, 4 bytes a sample, at the flow plane's
 *  rate, from dyeWordOf() -- the function the panel paints with. */
QByteArray WaterMarkDoc::packDyePlane( quint64 base, QString * error ) const
{
\tif ( !opened || !idPlane.ok ) {
\t\tif ( error )
\t\t\t*error = QStringLiteral( "no landscape file is open" );
\t\treturn QByteArray();
\t}
\tconst int spc = lodl->samplesPerCell();
\tconst int fr = int( flowRate );
\tconst int idStep = spc / int( idRate );
\tconst int flowStep = spc / fr;
\tQFile idf( filePath );
\tif ( !idf.open( QIODevice::ReadOnly ) ) {
\t\tif ( error )
\t\t\t*error = QStringLiteral( "could not re-open %1 for the body plane" ).arg( filePath );
\t\treturn QByteArray();
\t}
\tQByteArray idRaw;
\tquint32 idUni = 0;
\tbool idIsUni = false;
\tint cachedTx = -1, cachedTy = -1;
\tbool bad = false;
\tauto idAt = [&]( int px, int py ) -> quint16 {
\t\tconst int e = idPlane.tileEdge;
\t\tconst int tx = px / e, ty = py / e;
\t\tif ( px < 0 || py < 0 || tx >= idPlane.tilesX || ty >= idPlane.tilesY )
\t\t\treturn 0;
\t\tif ( tx != cachedTx || ty != cachedTy ) {
\t\t\tif ( !tileOf( idf, idPlane, tx, ty, idRaw, idUni, idIsUni ) ) {
\t\t\t\tbad = true;
\t\t\t\treturn 0;
\t\t\t}
\t\t\tcachedTx = tx;
\t\t\tcachedTy = ty;
\t\t}
\t\tif ( idIsUni )
\t\t\treturn quint16( idUni );
\t\treturn rd16( idRaw.constData() + ( qsizetype( py % e ) * e + ( px % e ) ) * 2 );
\t};
\tqint64 uniform = 0;
\tQByteArray plane = packPlane( idPlane.tilesX, idPlane.tilesY, fr, 4, base,
\t\t[&]( int tx, int ty, quint8 * dst ) {
\t\t\tfor ( int j = 0; j < fr; j++ ) {
\t\t\t\tconst qint64 gy = qint64( ty ) * spc + qint64( j ) * flowStep;
\t\t\t\tfor ( int i = 0; i < fr; i++ ) {
\t\t\t\t\tconst qint64 gx = qint64( tx ) * spc + qint64( i ) * flowStep;
\t\t\t\t\tconst int px = int( gx / idStep ), py = int( gy / idStep );
\t\t\t\t\tconst quint16 id = idAt( px, py );
\t\t\t\t\tconst quint32 word = id ? dyeWordOf( px, py, id ) : quint32( 0 );
\t\t\t\t\tfor ( int c = 0; c < 4; c++ )
\t\t\t\t\t\tdst[( j * fr + i ) * 4 + c] = quint8( ( word >> ( 8 * c ) ) & 0xFF );
\t\t\t\t}
\t\t\t}
\t\t}, &uniform );
\tif ( bad ) {
\t\tif ( error )
\t\t\t*error = QStringLiteral( "a tile of the body-ID plane would not inflate" );
\t\treturn QByteArray();
\t}
\treturn plane;
}

''', 'before'),
    # ---- save(): the dye plane after the shore plane ----
    ('\tpatch64( hdr, kOffSize, pos );\n\tpatch32( hdr, kOffSect, sect );\n',
     '''\t/* the dye plane: written only when the store carries a dye mark, so a
\t * file nobody dyed -- and a file whose dye was undone -- keeps the
\t * writer's own bytes; referenced from the reserved word at 0xF4 */
\tif ( hasDye() ) {
\t\tif ( pos > quint64( 0xFFFFFFFFu ) )
\t\t\treturn fail( QStringLiteral( "the dye plane would sit past 4 GB, which the version-3 "
\t\t\t\t"header's 32-bit word cannot address" ) );
\t\tQString perr;
\t\tconst QByteArray plane = packDyePlane( pos, &perr );
\t\tif ( plane.isEmpty() )
\t\t\treturn fail( perr );
\t\tif ( out.write( plane ) != plane.size() )
\t\t\treturn fail( QStringLiteral( "could not write the dye plane" ) );
\t\tpatch32( hdr, kOffDye, quint32( pos ) );
\t\tsect |= kSectDye;
\t\tpos += quint64( plane.size() );
\t} else {
\t\tpatch32( hdr, kOffDye, 0 );
\t\tsect &= ~kSectDye;
\t}
''', 'before'),
    # ---- the selftest: the flow gates, F5/F8, the dye round trip ----
    ('bool lodtWaterMarkSelfTest( const QString & path, QString * text, QString * error )\n',
     new_gates, 'before'),
    ('\tcheck( QStringLiteral( "the file was read whole for the undo gate (%1 bytes)" )\n\t\t.arg( beforeAll.size() ), beforeAll.size() > 0 );\n',
     '''
\t/* ---- 1b. THE FLOW GATES (lane WATER4) -----------------------------------
\t * Synthetic masks through the solver core alone, before any body of this
\t * file is touched: a red gate here is the METHOD, not the file. */
\tif ( qEnvironmentVariableIsSet( "WW_WATER_FLOW_TEST" ) || true ) {
\t\tsay( QStringLiteral( "-- the flow gates on synthetic masks --" ) );
\t\twaterFlowGates( check, say );
\t}
''', 'after'),
    ('\t\t\tcheck( QStringLiteral( "\\"rivers end up at sea\\": the marked plane\'s mean direction "\n\t\t\t\t"points at the mouth (cos = %1, needs > 0)" ).arg( dot, 0, \'f\', 3 ), dot > 0.0 );\n',
     '''\t\t\tcheck( QStringLiteral( "F5 the mean direction still points at the mouth (cos = %1, needs "
\t\t\t\t"> 0.9; R = %2)" ).arg( dot, 0, 'f', 3 ).arg( sm / std::max( 1.0, double( wetRiver ) ), 0, 'f', 3 ),
\t\t\t\tdot > 0.9 );
''', 'after'),
    ('\t/* ---- 5b. P4: the stroke survives a re-derivation at another rate ------\n',
     '''\t/* ---- 5a. F5 and F8 (lane WATER4): no held discs, and the cost --------
\t * The instrument is the pre-registered one: seam-bounded constant-
\t * direction patches of >= 64 texels, the 99th percentile of the angle
\t * difference between adjacent wet texels, and the seam fraction -- the
\t * same numbers scratchpad/water4_20260910/disc_metric.py reads back out
\t * of the file, where the harmonic fill measured 39 patches, p99 40.78
\t * degrees and 2.97 percent on this same body and stroke. */
\t{
\t\tconst FlowStructure fs = flowStructure( doc, rb );
\t\tsay( QStringLiteral( "F5 structure over body %1: %2 wet texels, %3 adjacent pairs, p99 %4 "
\t\t\t"degrees, seam fraction %5 percent, %6 seam-bounded patches (largest r_eq %7 texels)" )
\t\t\t.arg( river ).arg( fs.wet ).arg( fs.pairs ).arg( fs.p99, 0, 'f', 2 )
\t\t\t.arg( fs.seamFraction * 100.0, 0, 'f', 3 ).arg( fs.patches )
\t\t\t.arg( fs.patchMaxRadius, 0, 'f', 1 ) );
\t\tcheck( QStringLiteral( "F5 no seam-bounded constant-direction patches (measured %1; the "
\t\t\t"disc fill had 39)" ).arg( fs.patches ), fs.wet > 0 && fs.patches == 0 );
\t\tcheck( QStringLiteral( "F5 the 99th percentile of the adjacent angle difference is below 5 "
\t\t\t"degrees (%1; the disc fill had 40.78)" ).arg( fs.p99, 0, 'f', 2 ), fs.pairs > 0 && fs.p99 < 5.0 );
\t\tcheck( QStringLiteral( "F5 seams are below 0.5 percent of adjacent pairs (%1 percent; the "
\t\t\t"disc fill had 2.97)" ).arg( fs.seamFraction * 100.0, 0, 'f', 3 ),
\t\t\tfs.pairs > 0 && fs.seamFraction < 0.005 );
\t\tWaterMarkSolve st2;
\t\tdoc.solve( &st2, &err );
\t\tsay( QStringLiteral( "F8 the solve: %1 s, %2 iterations, residual %3, stroke agreement %4" )
\t\t\t.arg( st2.solveSeconds, 0, 'f', 3 ).arg( st2.iterations ).arg( st2.residual, 0, 'g', 3 )
\t\t\t.arg( st2.strokeAgreement, 0, 'f', 3 ) );
\t\tcheck( QStringLiteral( "F8 the solve of body %1 runs under 1.0 s with a residual below 1e-8 "
\t\t\t"(%2 s, %3)" ).arg( river ).arg( st2.solveSeconds, 0, 'f', 3 ).arg( st2.residual, 0, 'g', 3 ),
\t\t\tst2.solveSeconds < 1.0 && st2.residual < 1e-8 );
\t\tcheck( QStringLiteral( "the stroke and the solved flow under it agree (mean cosine %1, "
\t\t\t"needs > 0.5)" ).arg( st2.strokeAgreement, 0, 'f', 3 ), st2.strokeAgreement > 0.5 );
\t}

''', 'before'),
    ('\t/* ---- 7. save, reopen, save again, and undo ----------------------------\n',
     '''\t/* ---- 6b. DYE (lane WATER4) ----------------------------------------------
\t * bungo: "river flowing into an ocean and the river and the ocean may have
\t * slightly different color".  The river's water is carried past its mouth
\t * into the body it drains into as a plume that fades; the plane is written
\t * only while a dye mark exists, which is what lets section 7's undo gate
\t * stay byte-identical with the mark removed. */
\t{
\t\tdoc.setBodyDyeMouth( river, true, 1.0f );
\t\tWaterMarkSolve st;
\t\tdoc.solve( &st, &err );
\t\tsay( QStringLiteral( "dye solve: %1" ).arg( st.note ) );
\t\tcheck( QStringLiteral( "the river's dye reaches the body it drains into (%1 texels on %2 "
\t\t\t"receiving field(s))" ).arg( st.dyeTexels ).arg( st.dyeBodies ), st.dyeTexels > 0 );
\t\tconst double Lt = doc.dyeHalfDistance() / doc.worldPerTexel();
\t\tint mpx = 0, mpy = 0;
\t\tdoc.worldToTexel( mouthX, mouthY, mpx, mpy );
\t\tconst int reach = int( 5.0 * Lt ) + 2;
\t\tdouble nearSum = 0.0;
\t\tqint64 nearN = 0, dyed = 0, wrongSource = 0;
\t\tint farMax = 0;
\t\tfor ( int py = mpy - reach; py <= mpy + reach; py++ )
\t\t\tfor ( int px = mpx - reach; px <= mpx + reach; px++ ) {
\t\t\t\tconst quint32 wd = doc.dyeWordAt( px, py );
\t\t\t\tif ( !wd )
\t\t\t\t\tcontinue;
\t\t\t\tdyed++;
\t\t\t\tif ( int( wd & 0xFFFF ) != river )
\t\t\t\t\twrongSource++;
\t\t\t\tconst int wgt = int( ( wd >> 16 ) & 0xFF );
\t\t\t\tconst double d = std::hypot( double( px - mpx ), double( py - mpy ) );
\t\t\t\tif ( d < 0.5 * Lt ) {
\t\t\t\t\tnearSum += wgt;
\t\t\t\t\tnearN++;
\t\t\t\t}
\t\t\t\tif ( d > 3.0 * Lt )
\t\t\t\t\tfarMax = std::max( farMax, wgt );
\t\t\t}
\t\tsay( QStringLiteral( "dye near the mouth at (%1, %2): %3 dyed texels within %4 texels, mean "
\t\t\t"weight %5 within L/2 (%6 texels), max %7 beyond 3 L" ).arg( mpx ).arg( mpy ).arg( dyed )
\t\t\t.arg( reach ).arg( nearN ? nearSum / nearN : 0.0, 0, 'f', 1 ).arg( nearN ).arg( farMax ) );
\t\tcheck( QStringLiteral( "near the mouth the weight is above 128 (mean %1 over %2 texels)" )
\t\t\t.arg( nearN ? nearSum / nearN : 0.0, 0, 'f', 1 ).arg( nearN ), nearN > 0 && nearSum / nearN > 128.0 );
\t\tcheck( QStringLiteral( "beyond three half-distances the weight has fallen to 1/8 or less "
\t\t\t"(max %1, 1/8 of 255 = 32, slack to 48)" ).arg( farMax ), dyed > 0 && farMax <= 48 );
\t\tcheck( QStringLiteral( "every dyed texel names the river as its source (%1 of %2 do not)" )
\t\t\t.arg( wrongSource ).arg( dyed ), dyed > 0 && wrongSource == 0 );
\t\tif ( !doc.save( &err ) ) {
\t\t\tcheck( QStringLiteral( "the dyed file saves: %1" ).arg( err ), false );
\t\t} else {
\t\t\tLodtFile lf;
\t\t\tQString lerr;
\t\t\tconst bool opened = lf.open( path, &lerr );
\t\t\tcheck( QStringLiteral( "the dyed file re-opens through the .lodl reader with the dye "
\t\t\t\t"section (bit %1, %2 samples a cell = the flow rate %3): %4" )
\t\t\t\t.arg( ( lf.sectionFlags() & LODL_SECT_DYE ) ? 1 : 0 ).arg( lf.dyePlaneSamples() )
\t\t\t\t.arg( doc.flowSamples() ).arg( lerr ),
\t\t\t\topened && ( lf.sectionFlags() & LODL_SECT_DYE ) && lf.dyePlaneSamples() == doc.flowSamples() );
\t\t\tint agree = 0, total = 0;
\t\t\tconst int bodyS = lf.bodyIdSamples() > 0 ? lf.bodyIdSamples() : 1;
\t\t\tconst int dyeS = lf.dyePlaneSamples() > 0 ? lf.dyePlaneSamples() : 1;
\t\t\tfor ( int py = mpy - reach; py <= mpy + reach; py += 7 )
\t\t\t\tfor ( int px = mpx - reach; px <= mpx + reach; px += 7 ) {
\t\t\t\t\tconst quint32 ours = doc.dyeWordAt( px, py );
\t\t\t\t\tconst quint32 theirs = lf.dyeWordAt( px * dyeS / bodyS, py * dyeS / bodyS );
\t\t\t\t\ttotal++;
\t\t\t\t\tif ( ours == theirs )
\t\t\t\t\t\tagree++;
\t\t\t\t}
\t\t\tcheck( QStringLiteral( "the dye words read back out of the file agree with the document "
\t\t\t\t"(%1 of %2 sampled texels)" ).arg( agree ).arg( total ), total > 0 && agree == total );
\t\t}
\t}

''', 'before'),
])
