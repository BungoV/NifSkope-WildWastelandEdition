// NIFXLIGHT1 unit gate: a fixture's .nifx lights applied to a cell's lights (src/gl/cellnifxlights.cpp), the
// conversion against hand-worked answers, the 64-unit takeover and its carried state, and the fog.scale rungs.
// Usage: nifxapply <frames dir> <nifx dir> [--red words] [--dump file] [--two <frames dir> <nifx dir>]; exit 0 = every
// check passed. --two (lane NIFXLIGHT2): NifxLight2Lamp's placed scale, NiLight dimmers and bulb-tier rules.
// --three <frames dir> <nifx dir> (lane NIFXLIGHT3): dimmer tracks, bulb flicker, a .nifx made after the build.
// <frames dir>/<Model>.frames: one node a line, "name|niLight 0/1|m00 m01 m02 m03 m10 .. m23" (the mesh's world
// frames, from `nifskope-cli world`), then optionally "|dimmer|sequences" (lane NIFXLIGHT2, a NiLight's On-state
// dimmer and its dimmer sequences), then "|interp cycle scaled backwards start stop frequency phase|t:v:fwd:bwd,..."
// (lane NIFXLIGHT3, its dimmer track); <nifx dir>/<Model>.nifx: the test lights.

#include "cellnifxlights.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

int fails = 0, checks = 0;

void check( bool ok, const QString & what )
{
	checks++;
	if ( !ok )
		fails++;
	std::printf( "%s %s\n", ok ? "PASS" : "FAIL", what.toUtf8().constData() );
}

bool near( double a, double b, double tol = 1e-3 ) { return std::fabs( a - b ) <= tol * std::max( 1.0, std::fabs( b ) ); }

std::shared_ptr<WwNifxModel> loadModel( const QString & framesDir, const QString & nifxDir, const QString & model )
{
	auto m = std::make_shared<WwNifxModel>();
	m->model = model;
	m->sidecar = nifxDir + QLatin1Char( '/' ) + QString( model ).replace( QLatin1String( ".nif" ), QLatin1String( ".nifx" ) );
	QFile f( framesDir + QLatin1Char( '/' ) + QString( model ).replace( QLatin1String( ".nif" ), QLatin1String( ".frames" ) ) );
	if ( !f.open( QIODevice::ReadOnly | QIODevice::Text ) )
		return m;
	QTextStream in( &f );
	while ( !in.atEnd() ) {
		const QStringList p = in.readLine().split( QLatin1Char( '|' ) );
		if ( p.size() < 3 )
			continue;
		WwNifxNode n;
		n.name = p[0];
		n.niLight = p[1] == QLatin1String( "1" );
		if ( p.size() > 3 )
			n.dimmer = p[3].toFloat();
		if ( p.size() > 4 )
			n.sequences = p[4].toInt();
		if ( p.size() > 6 ) {	// lane NIFXLIGHT3
			auto k = std::make_shared<WwNifxDimTrack>();
			const QStringList h = p[5].split( QLatin1Char( ' ' ), Qt::SkipEmptyParts );
			if ( h.size() >= 8 ) {
				k->interp = h[0].toInt();
				k->cycle = h[1].toInt();
				k->scaled = h[2] == QLatin1String( "1" );
				k->backwards = h[3] == QLatin1String( "1" );
				k->start = h[4].toFloat();
				k->stop = h[5].toFloat();
				k->frequency = h[6].toFloat();
				k->phase = h[7].toFloat();
			}
			for ( const QString & key : p[6].split( QLatin1Char( ',' ), Qt::SkipEmptyParts ) ) {
				const QStringList q = key.split( QLatin1Char( ':' ) );
				k->t.push_back( q.value( 0 ).toFloat() );
				k->v.push_back( q.value( 1 ).toFloat() );
				k->fwd.push_back( q.value( 2 ).toFloat() );
				k->bwd.push_back( q.value( 3 ).toFloat() );
			}
			k->sequence = QStringLiteral( "Loop" );
			n.track = k;
		}
		const QStringList v = p[2].split( QLatin1Char( ' ' ), Qt::SkipEmptyParts );
		for ( int i = 0; i < 12 && i < v.size(); i++ )
			n.m[i / 4][i % 4] = v[i].toFloat();
		m->nodes.append( n );
	}
	return m;
}

const WwNifxNode * nodeOf( const WwNifxModel & m, const std::string & name )
{
	for ( const WwNifxNode & n : m.nodes )
		if ( n.name.compare( QString::fromStdString( name ), Qt::CaseInsensitive ) == 0 )
			return &n;
	return nullptr;
}

// the expected world point, written out as one matrix product: P = pos + R (scale (N p))
void expectWorld( const WwNifxFixture & f, const WwNifxModel & m, const nifxl::Light & x, double out[3] )
{
	double nm[3][4] = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } };
	if ( const WwNifxNode * n = x.hasNode ? nodeOf( m, x.node ) : nullptr )
		for ( int r = 0; r < 3; r++ )
			for ( int c = 0; c < 4; c++ )
				nm[r][c] = n->m[r][c];
	double M[3][4];
	for ( int r = 0; r < 3; r++ )
		for ( int c = 0; c < 4; c++ ) {
			double s = 0;
			for ( int k = 0; k < 3; k++ )
				s += f.rot[r][k] * nm[k][c];
			M[r][c] = s * f.scale + ( c == 3 ? f.pos[r] : 0.0 );
		}
	for ( int r = 0; r < 3; r++ )
		out[r] = M[r][0] * x.position[0] + M[r][1] * x.position[1] + M[r][2] * x.position[2] + M[r][3];
}

WwNifxFixture fixtureAt( std::shared_ptr<const WwNifxModel> m, double x, double y, double z, double yawDeg, quint32 ref )
{
	WwNifxFixture f;
	f.model = m;
	f.pos[0] = float( x );
	f.pos[1] = float( y );
	f.pos[2] = float( z );
	const double a = yawDeg * 3.14159265358979323846 / 180.0;
	const float R[3][3] = { { float( std::cos( a ) ), float( -std::sin( a ) ), 0 }, { float( std::sin( a ) ), float( std::cos( a ) ), 0 }, { 0, 0, 1 } };
	for ( int r = 0; r < 3; r++ )
		for ( int c = 0; c < 3; c++ )
			f.rot[r][c] = R[r][c];
	f.scale = 1.0f;
	f.ref = ref;
	f.refPlugin = QStringLiteral( "Fallout4.esm" );
	f.refLocal = ref;
	return f;
}

WwCellLight placed( const double p[3], double dx, quint32 ref, int flicker )
{
	WwCellLight l;
	l.pos[0] = float( p[0] + dx );
	l.pos[1] = float( p[1] );
	l.pos[2] = float( p[2] );
	l.radius = 300;
	l.color[0] = l.color[1] = l.color[2] = 1;
	l.ref = ref;
	l.flicker = flicker;
	l.flickerPeriod = flicker ? 0.25f : 0.0f;
	l.flickerIntensity = flicker ? 0.5f : 0.0f;
	l.recxRefPlugin = QStringLiteral( "Fallout4.esm" );
	l.recxRefLocal = ref;
	l.recxBasePlugin = QStringLiteral( "Fallout4.esm" );
	l.recxBaseLocal = ref + 0x1000;
	return l;
}

const WwCellLight * byName( const QVector<WwCellLight> & v, const QString & name, const float near3[3] = nullptr )
{
	for ( const WwCellLight & l : v )
		if ( l.tier >= 0 && l.nifxName == name ) {
			if ( !near3 )
				return &l;
			double d = 0;
			for ( int k = 0; k < 3; k++ )
				d += ( l.pos[k] - near3[k] ) * ( l.pos[k] - near3[k] );
			if ( d < 1.0 )
				return &l;
		}
	return nullptr;
}

const WwCellLight * byRef( const QVector<WwCellLight> & v, quint32 ref )
{
	for ( const WwCellLight & l : v )
		if ( l.tier < 0 && l.ref == ref )
			return &l;
	return nullptr;
}

}	// namespace

int main( int argc, char ** argv )
{
	QCoreApplication app( argc, argv );
	const QStringList a = app.arguments();
	if ( a.size() < 3 ) {
		std::printf( "usage: nifxapply <frames dir> <nifx dir> [--red words] [--dump file]\n" );
		return 2;
	}
	const QString framesDir = a[1], nifxDir = a[2];
	const int ri = int( a.indexOf( QStringLiteral( "--red" ) ) ), di = int( a.indexOf( QStringLiteral( "--dump" ) ) );
	const int red = ri > 0 && ri + 1 < a.size() ? wwNifxRedOf( a[ri + 1].toUtf8() ) : 0;
	const QString dumpTo = di > 0 && di + 1 < a.size() ? a[di + 1] : QString();

	const auto lamp = loadModel( framesDir, nifxDir, QStringLiteral( "DeskLamp01On.nif" ) );
	const auto flash = loadModel( framesDir, nifxDir, QStringLiteral( "WallEmergencyLight01FlashOmni.nif" ) );
	check( lamp->nodes.size() > 1 && flash->nodes.size() > 1, QStringLiteral( "frames read: lamp %1 nodes, flash %2" )
		.arg( lamp->nodes.size() ).arg( flash->nodes.size() ) );

	QHash<QString, nifxl::Lights> docs;
	for ( const auto & m : { lamp, flash } ) {
		QFile f( m->sidecar );
		check( f.open( QIODevice::ReadOnly ), QStringLiteral( "read %1" ).arg( m->sidecar ) );
		const QByteArray b = f.readAll();
		docs.insert( m->sidecar, nifxl::read( std::string( b.constData(), size_t( b.size() ) ) ) );
	}
	const nifxl::Lights & dl = docs[lamp->sidecar];
	const nifxl::Lights & df = docs[flash->sidecar];
	const nifxl::Light * bulb = dl.find( "Bulb" ), * shade = dl.find( "Shade" ), * glow = dl.find( "Glow" ), * blue = dl.find( "Blue" );
	const nifxl::Light * fl = df.find( "Flash" );
	if ( !bulb || !shade || !glow || !blue || !fl ) {
		std::printf( "FAIL the test .nifx lacks Bulb/Shade/Glow/Blue (lamp) or Flash (flash)\nNIFXAPPLY FAIL\n" );
		return 1;
	}

	// three placements: A (lights on), B (the NiLight fixture, its light starts off), C (its placed light skipped dark)
	WwCellLighting L;
	const WwNifxFixture A = fixtureAt( lamp, 1000, 2000, 300, 90, 0xA0 );
	const WwNifxFixture B = fixtureAt( flash, -500, 0, 100, 0, 0xB0 );
	const WwNifxFixture C = fixtureAt( lamp, 4000, 0, 0, 0, 0xC0 );
	L.nifxFixtures = { A, B, C };
	double aBulb[3], aShade[3], aBlue[3], bFlash[3], cBulb[3];
	expectWorld( A, *lamp, *bulb, aBulb );
	expectWorld( A, *lamp, *shade, aShade );
	expectWorld( A, *lamp, *blue, aBlue );
	expectWorld( B, *flash, *fl, bFlash );
	expectWorld( C, *lamp, *bulb, cBulb );
	L.lights = { placed( aBulb, 3, 0xA1, 1 ), placed( aBulb, -20, 0xA2, 0 ), placed( aBulb, 300, 0xA3, 0 ) };
	L.offLights = { placed( bFlash, 2, 0xB1, 2 ) };
	L.nifxOffPlaced = { placed( cBulb, 1, 0xC1, 0 ) };
	const WwCellLighting before = L;

	// off == base: no file read leaves every list as it was
	{
		WwCellLighting U = L;
		QStringList cs;
		wwNifxLightsApply( U, {}, 0, &cs );
		check( U.lights.size() == 3 && U.offLights.size() == 1 && U.nifxOffPlaced.size() == 1 && U.nifxBulbs.isEmpty()
			&& wwNifxLightsDump( U ) == wwNifxLightsDump( before ), QStringLiteral( "no .nifx read: lights as built" ) );
	}

	QStringList census;
	wwNifxLightsApply( L, docs, red, &census );
	for ( const QString & c : census )
		std::printf( "  %s\n", c.toUtf8().constData() );
	const QString dump = wwNifxLightsDump( L );
	if ( !dumpTo.isEmpty() ) {
		QFile o( dumpTo );
		if ( o.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
			o.write( ( census.join( QLatin1Char( '\n' ) ) + QLatin1Char( '\n' ) + dump ).toUtf8() );
	}

	// conversion, worked by hand from nifx/SPEC.md "Converting"
	const float aBulbF[3] = { float( aBulb[0] ), float( aBulb[1] ), float( aBulb[2] ) };
	const WwCellLight * lb = byName( L.lights, QStringLiteral( "Bulb" ), aBulbF );
	check( lb != nullptr, QStringLiteral( "A Bulb drawn at its node (%1 %2 %3)" ).arg( aBulb[0] ).arg( aBulb[1] ).arg( aBulb[2] ) );
	if ( lb ) {
		// 800 lm omni, white: I = 800/4pi, r = 69.99 sqrt(I), bytes 255, fade 1
		check( near( lb->radius, 69.99 * std::sqrt( 800.0 / ( 4 * 3.14159265358979323846 ) ) ) && near( lb->color[0], 1.0 )
			&& near( lb->color[2], 1.0 ) && lb->shadow == 0 && lb->tier == nifxl::Normal,
			QStringLiteral( "A Bulb 800 lm white: radius %1 color %2 shadow %3" ).arg( lb->radius ).arg( lb->color[0] ).arg( lb->shadow ) );
		check( lb->flicker == 1 && near( lb->flickerPeriod, 0.25 ) && lb->ref == 0xA1 && lb->recxBaseLocal == 0xA1 + 0x1000
			&& lb->recxRefLocal == 0xA1, QStringLiteral( "A Bulb carries the nearest placed light (flicker %1 ref %2)" )
			.arg( lb->flicker ).arg( lb->ref, 0, 16 ) );
	}
	const float aShadeF[3] = { float( aShade[0] ), float( aShade[1] ), float( aShade[2] ) };
	const WwCellLight * ls = byName( L.lights, QStringLiteral( "Shade" ), aShadeF );
	check( ls != nullptr, QStringLiteral( "A Shade drawn" ) );
	if ( ls ) {
		const double om = 2 * 3.14159265358979323846 * ( 1 - std::cos( 50.0 * 3.14159265358979323846 / 180.0 ) );
		const nifxl::Converted cv = nifxl::convert( *shade );
		check( ls->spot && ls->shadow == 1 && near( ls->cosOuter, std::cos( 50.0 * 3.14159265358979323846 / 180.0 ) )
			&& near( ls->radius, std::min( 16384.0, 69.99 * std::sqrt( 1200.0 / om ) ) ),
			QStringLiteral( "A Shade hero spot 100 deg: shadow slot %1, radius %2" ).arg( ls->shadow ).arg( ls->radius ) );
		check( near( ls->dir[0], 0 ) && near( ls->dir[1], 0 ) && near( ls->dir[2], -1 ), QStringLiteral( "A Shade points down" ) );
		double Y = 0;
		const double w[3] = { 0.2126, 0.7152, 0.0722 };
		for ( int k = 0; k < 3; k++ )
			Y += w[k] * std::pow( cv.color[k] / 255.0, 2.2 );
		check( near( cv.fade, ( 1200.0 / om ) / ( 800.0 / ( 4 * 3.14159265358979323846 ) ) / Y, 1e-6 )
			&& near( ls->color[0], std::pow( cv.color[0] / 255.0, 2.2 ) * cv.fade, 1e-4 ),
			QStringLiteral( "A Shade 3000 K: bytes %1 %2 %3 fade %4" ).arg( cv.color[0] ).arg( cv.color[1] ).arg( cv.color[2] ).arg( cv.fade ) );
	}
	const float aBlueF[3] = { float( aBlue[0] ), float( aBlue[1] ), float( aBlue[2] ) };
	const WwCellLight * lu = byName( L.lights, QStringLiteral( "Blue" ), aBlueF );
	check( lu && near( lu->byteColor[2], 1.0 ) && near( lu->byteColor[0], 0.0 ) && near( lu->color[2], 1.0 / 0.0722, 1e-4 ),
		QStringLiteral( "A Blue: a tint never dims (fade 1/Y = %1, got %2)" ).arg( 1.0 / 0.0722 ).arg( lu ? lu->color[2] : -1.0f ) );

	// tiers: the bulb tier is never drawn
	check( L.nifxBulbs.size() == 2 && !byName( L.lights, QStringLiteral( "Glow" ) ) && near( L.nifxBulbs[0].nifxFog, 2.5 ),
		QStringLiteral( "Glow (bulb tier) kept apart: %1, fog %2" ).arg( L.nifxBulbs.size() )
		.arg( L.nifxBulbs.isEmpty() ? -1.0f : L.nifxBulbs[0].nifxFog ) );

	// takeover: A's two placed lights within 64 units are off, the one 300 units away stays
	check( !byRef( L.lights, 0xA1 ) && !byRef( L.lights, 0xA2 ) && byRef( L.lights, 0xA3 ),
		QStringLiteral( "placed lights within 64 units switched off, the far one kept" ) );
	// B: the NiLight edited in place, its placed light started off, so the .nifx light starts off with it
	const float bF[3] = { float( bFlash[0] ), float( bFlash[1] ), float( bFlash[2] ) };
	const WwCellLight * lf = byName( L.offLights, QStringLiteral( "Flash" ), bF );
	check( lf && lf->flicker == 2 && lf->ref == 0xB1 && !byRef( L.offLights, 0xB1 ) && !byName( L.lights, QStringLiteral( "Flash" ) ),
		QStringLiteral( "B Flash (NiLight %1) starts off with the pulse of the light it took" ).arg( fl->node.c_str() ) );
	// C: its placed light was skipped (start off, not kept): C's Bulb is dark, C's Shade still draws
	const float cBulbF[3] = { float( cBulb[0] ), float( cBulb[1] ), float( cBulb[2] ) };
	check( !byName( L.lights, QStringLiteral( "Bulb" ), cBulbF ) && !byName( L.offLights, QStringLiteral( "Bulb" ), cBulbF )
		&& L.nifxOffPlaced.isEmpty(), QStringLiteral( "C Bulb dark (took a skipped start-off light)" ) );

	// census
	const QString all = census.join( QLatin1Char( '\n' ) );
	check( all.contains( QStringLiteral( "WallEmergencyLight01FlashOmni.nif: placed 1, ok; 1 lights each (hero 0, normal 1, bulb 0), "
		"took over 1 placed, NiLight edits 1" ) ), QStringLiteral( "census: the NiLight edit counted" ) );
	check( all.contains( QStringLiteral( "follows 0 (on state) unbound 1" ) ), QStringLiteral( "census: an unbound follows warned" ) );
	check( dump.contains( QStringLiteral( "placed lights left: on 1 off 0" ) ), QStringLiteral( "dump: placed lights left on 1 off 0" ) );

	// fog.scale rungs: placed lamp's reference > .nifx light > light record > 1
	const bool redPrec = ( red & WwNifxRedPrecedence ) != 0;
	auto yes = [] { return true; };
	auto no = [] { return false; };
	check( wwNifxFogRung( yes, true, yes, redPrec ) == WwNifxFogRef, QStringLiteral( "fog rung 1: the placed lamp's reference wins" ) );
	check( wwNifxFogRung( no, true, yes, redPrec ) == WwNifxFogMesh, QStringLiteral( "fog rung 2: the .nifx light beats its record" ) );
	check( wwNifxFogRung( no, false, yes, redPrec ) == WwNifxFogRecord, QStringLiteral( "fog rung 3: the light record" ) );
	check( wwNifxFogRung( no, false, no, redPrec ) == WwNifxFogNone, QStringLiteral( "fog rung 4: none, scale 1" ) );
	int recordCalls = 0;
	wwNifxFogRung( yes, true, [&] { recordCalls++; return true; }, redPrec );
	check( recordCalls == 0, QStringLiteral( "fog: a won rung reads no lower file" ) );

	// lane NIFXLIGHT2: one fixture placed at scale 2 with two NiLights (dimmers 0.5 and 0.25), a start-off placed light
	// under its first bulb-tier light
	const int ti = int( a.indexOf( QStringLiteral( "--two" ) ) );
	if ( ti > 0 && ti + 2 < a.size() ) {
		const auto two = loadModel( a[ti + 1], a[ti + 2], QStringLiteral( "NifxLight2Lamp.nif" ) );
		QHash<QString, nifxl::Lights> d2;
		QFile f( two->sidecar );
		check( two->nodes.size() == 3 && f.open( QIODevice::ReadOnly ), QStringLiteral( "two: %1 nodes, read %2" )
			.arg( two->nodes.size() ).arg( two->sidecar ) );
		const QByteArray b = f.readAll();
		d2.insert( two->sidecar, nifxl::read( std::string( b.constData(), size_t( b.size() ) ) ) );
		const nifxl::Lights & dt = d2[two->sidecar];
		const nifxl::Light * xf = dt.find( "Follow" ), * xe = dt.find( "Edit" ), * xu = dt.find( "Unbound" ), * x2 = dt.find( "Bulb2" );
		if ( !xf || !xe || !xu || !x2 || !dt.find( "Bulb3" ) ) {
			std::printf( "FAIL the two .nifx lacks Follow/Edit/Unbound/Bulb2/Bulb3\nNIFXAPPLY FAIL\n" );
			return 1;
		}
		WwCellLighting T;
		WwNifxFixture D = fixtureAt( two, 0, 0, 0, 0, 0xD0 );
		D.scale = 2.0f;
		T.nifxFixtures = { D };
		double pF[3], pE[3], pU[3], p2[3];
		expectWorld( D, *two, *xf, pF );
		expectWorld( D, *two, *xe, pE );
		expectWorld( D, *two, *xu, pU );
		expectWorld( D, *two, *x2, p2 );
		T.offLights = { placed( p2, 1, 0xD1, 0 ) };
		QStringList c2;
		wwNifxLightsApply( T, d2, red, &c2 );
		for ( const QString & c : c2 )
			std::printf( "  %s\n", c.toUtf8().constData() );
		const double r800 = 69.99 * std::sqrt( 800.0 / ( 4 * 3.14159265358979323846 ) );
		const float fF[3] = { float( pF[0] ), float( pF[1] ), float( pF[2] ) };
		const float fE[3] = { float( pE[0] ), float( pE[1] ), float( pE[2] ) };
		const float fU[3] = { float( pU[0] ), float( pU[1] ), float( pU[2] ) };
		const WwCellLight * tf = byName( T.lights, QStringLiteral( "Follow" ), fF );
		const WwCellLight * te = byName( T.lights, QStringLiteral( "Edit" ), fE );
		const WwCellLight * tu = byName( T.lights, QStringLiteral( "Unbound" ), fU );
		check( tf && te && tu, QStringLiteral( "two: Follow, Edit, Unbound drawn where the scale-2 placement puts them" ) );
		check( tf && near( tf->radius, 2 * r800 ) && tu && near( tu->radius, 2 * r800 ),
			QStringLiteral( "two: radius x placed scale 2 (%1, want %2)" ).arg( tf ? tf->radius : -1.0f ).arg( 2 * r800 ) );
		check( tf && near( tf->color[0], 0.5 ) && near( tf->color[2], 0.5 ),
			QStringLiteral( "two: follows LampLight takes its dimmer 0.5 (got %1)" ).arg( tf ? tf->color[0] : -1.0f ) );
		check( te && near( te->color[0], 0.25 ),
			QStringLiteral( "two: a NiLight edited in place keeps its own dimmer 0.25 (got %1)" ).arg( te ? te->color[0] : -1.0f ) );
		check( tu && near( tu->color[0], 1.0 ), QStringLiteral( "two: an unbound follows stays at full strength" ) );
		check( !byName( T.lights, QStringLiteral( "Bulb2" ) ) && !byName( T.lights, QStringLiteral( "Bulb3" ) )
			&& !byName( T.offLights, QStringLiteral( "Bulb2" ) ), QStringLiteral( "two: no bulb-tier light on the surfaces" ) );
		check( T.nifxBulbs.size() == 1 && T.nifxBulbs[0].nifxName == QLatin1String( "Bulb3" ) && T.offLights.isEmpty(),
			QStringLiteral( "two: Bulb2 dark with the start-off light it took, Bulb3 kept for the GI and fog (%1 bulbs)" )
			.arg( T.nifxBulbs.size() ) );
		WwCellLighting G;
		const bool gi = wwNifxGiLights( T, red, G );
		check( gi && G.lights.size() == T.lights.size() + T.nifxBulbs.size() && byName( G.lights, QStringLiteral( "Bulb3" ) )
			&& G.nifxBulbs.size() == T.nifxBulbs.size(), QStringLiteral( "two: the GI's lights take the bulbs (%1 of %2)" )
			.arg( G.lights.size() ).arg( T.lights.size() + T.nifxBulbs.size() ) );
		WwCellLighting none, G0;
		check( !wwNifxGiLights( none, 0, G0 ), QStringLiteral( "two: no bulbs, the GI keeps the lights as they are" ) );
		const QString all2 = c2.join( QLatin1Char( '\n' ) );
		check( all2.contains( QStringLiteral( "2 lights scaled by a NiLight dimmer, 2 dimmer sequences bound" ) )
			&& all2.contains( QStringLiteral( "1 bulb-tier lights dark (took a start-off light)" ) ),
			QStringLiteral( "two: census counts the dimmers, sequences and the dark bulb" ) );
	}

	// lane NIFXLIGHT3: dimmer tracks, bulb flicker, a .nifx made after the build (live == fresh)
	const int hi = int( a.indexOf( QStringLiteral( "--three" ) ) );
	if ( hi > 0 && hi + 2 < a.size() ) {
		const QString fr3 = a[hi + 1], nx3 = a[hi + 2];
		// the track's time rule, worked by hand (NifSkope's ctrlTime + interpolate)
		WwNifxDimTrack lin;
		lin.t = { 0, 1, 2 };
		lin.v = { 0.5f, 1.0f, 0.5f };
		lin.fwd = lin.bwd = { 0, 0, 0 };
		lin.start = 0;
		lin.stop = 2;
		check( near( wwNifxDimAt( lin, 0.5 ), 0.75, 1e-6 ) && near( wwNifxDimAt( lin, 2.5 ), 0.75, 1e-6 )
			&& near( wwNifxDimAt( lin, 3.25 ), 0.875, 1e-6 ), QStringLiteral( "three: linear loop 0.5 -> %1, 2.5 -> %2, 3.25 -> %3" )
			.arg( wwNifxDimAt( lin, 0.5 ) ).arg( wwNifxDimAt( lin, 2.5 ) ).arg( wwNifxDimAt( lin, 3.25 ) ) );
		WwNifxDimTrack her = lin;
		her.t = { 0, 1 };
		her.v = { 0.25f, 1.0f };
		her.fwd = her.bwd = { 0, 0 };
		her.interp = 2;
		her.cycle = 1;
		her.stop = 1;
		check( near( wwNifxDimAt( her, 0.5 ), 0.625, 1e-6 ) && near( wwNifxDimAt( her, 1.25 ), 0.8828125, 1e-6 ),
			QStringLiteral( "three: quadratic ping-pong 0.5 -> %1, 1.25 -> %2 (back at 0.75)" )
			.arg( wwNifxDimAt( her, 0.5 ) ).arg( wwNifxDimAt( her, 1.25 ) ) );
		WwNifxDimTrack clamp = lin, still = lin, back = lin, con = lin;
		clamp.cycle = 2;
		still.scaled = false;
		still.frequency = 3;
		back.backwards = true;
		con.interp = 5;
		check( near( wwNifxDimAt( clamp, 5 ), 0.5, 1e-6 ) && near( wwNifxDimAt( clamp, 1.5 ), 0.75, 1e-6 )
			&& near( wwNifxDimAt( still, 0.5 ), 0.75, 1e-6 ) && near( wwNifxDimAt( back, 0.25 ), 0.625, 1e-6 )
			&& near( wwNifxDimAt( con, 0.4 ), 0.5, 1e-6 ) && near( wwNifxDimAt( con, 0.6 ), 1.0, 1e-6 ),
			QStringLiteral( "three: clamp, unscaled, backwards and constant keys" ) );

		auto load3 = [&]( const QString &, const QString & model, const QString & sidecar ) -> std::shared_ptr<const WwNifxModel> {
			auto m = loadModel( fr3, nx3, QFileInfo( QDir::fromNativeSeparators( model ) ).fileName() );
			if ( m->nodes.isEmpty() )
				return nullptr;
			m->model = model;
			m->sidecar = sidecar;
			return m;
		};
		auto docsOf = [&]( const QVector<WwNifxFixture> & fx ) {
			QHash<QString, nifxl::Lights> d;
			for ( const WwNifxFixture & f : fx )
				if ( f.model && !d.contains( f.model->sidecar ) ) {
					QFile in( f.model->sidecar );
					if ( in.open( QIODevice::ReadOnly ) ) {
						const QByteArray b = in.readAll();
						d.insert( f.model->sidecar, nifxl::read( std::string( b.constData(), size_t( b.size() ) ) ) );
					}
				}
			return d;
		};
		const QString modelPath = QStringLiteral( "SetDressing\\NifxLight3Lamp.nif" );
		const QString srcNifx = nx3 + QStringLiteral( "/NifxLight3Lamp.nifx" );
		QTemporaryDir root( QDir::currentPath() + QStringLiteral( "/nifx3_XXXXXX" ) ), root2( QDir::currentPath() + QStringLiteral( "/nifx3_XXXXXX" ) ),
			data( QDir::currentPath() + QStringLiteral( "/nifx3_XXXXXX" ) );	// beside the run, not in the system temp folder
		const QString rel = wwNifxSidecarRel( modelPath );
		check( rel == QLatin1String( "Meshes/SetDressing/NifxLight3Lamp.nifx" ) && root.isValid() && root2.isValid(),
			QStringLiteral( "three: the .nifx path of %1 is %2" ).arg( modelPath, rel ) );
		const QString side = QDir::cleanPath( root.path() + QLatin1Char( '/' ) + rel );
		// the cell as built: a placed start-off light under Dark, a flickering placed light under Flick
		auto cellOf = [&]( const WwNifxFixture & f ) {
			WwCellLighting C;
			C.nifxFixtures = { f };
			const double pd[3] = { 0, 0, 300 }, pk[3] = { 0, -200, 0 };
			if ( wwNifxKeepStartOff( C.nifxFixtures, red ) )
				C.nifxOffPlaced = { placed( pd, 1, 0xE1, 0 ) };
			C.lights = { placed( pk, 2, 0xE2, 1 ) };
			return C;
		};
		WwNifxFixture F = fixtureAt( nullptr, 0, 0, 0, 0, 0xE0 );
		F.modelPath = modelPath;
		F.dataRoot = data.path();

		// fresh: the .nifx was there when the cell was built
		QDir().mkpath( QFileInfo( side ).path() );
		check( QFile::copy( srcNifx, side ), QStringLiteral( "three: the test .nifx put under a temporary root" ) );
		WwNifxFixture Ff = F;
		Ff.model = load3( F.dataRoot, modelPath, wwNifxSidecarFind( { root.path() }, modelPath ) );
		WwCellLighting fresh = cellOf( Ff );
		QStringList cf;
		wwNifxLightsApply( fresh, docsOf( fresh.nifxFixtures ), red, &cf );
		const QString freshDump = wwNifxLightsDump( fresh );
		const WwCellLight * df = byName( fresh.lights, QStringLiteral( "Follow" ) );
		const WwCellLight * de = byName( fresh.lights, QStringLiteral( "Edit" ) );
		const WwCellLight * ds = byName( fresh.lights, QStringLiteral( "Steady" ) );
		check( df && df->nifxDim && near( df->nifxDimRest, 0.5 ) && near( wwNifxDimFactor( *df, 0.5 ), 1.5, 1e-6 )
			&& df->nifxDim->sequence == QLatin1String( "Loop" ),
			QStringLiteral( "three: follows LampLight carries its track (factor at 0.5 s %1, want 1.5)" )
			.arg( df ? wwNifxDimFactor( *df, 0.5 ) : -1.0f ) );
		check( de && de->nifxDim && near( wwNifxDimFactor( *de, 0.5 ), 2.5, 1e-6 ) && ds && !ds->nifxDim && near( wwNifxDimFactor( *ds, 0.5 ), 1.0 ),
			QStringLiteral( "three: the edited NiLight plays its own track (%1, want 2.5), a plain light none" )
			.arg( de ? wwNifxDimFactor( *de, 0.5 ) : -1.0f ) );
		check( fresh.nifxOffBulbs.size() == 1 && fresh.nifxOffBulbs[0].nifxName == QLatin1String( "Dark" )
			&& fresh.nifxBulbs.size() == 1 && fresh.nifxBulbs[0].nifxName == QLatin1String( "Flick" ) && fresh.nifxBulbs[0].flicker == 1
			&& freshDump.contains( QStringLiteral( "offbulb Dark" ) ),
			QStringLiteral( "three: Dark kept dark (%1), Flick carries the flicker it took" ).arg( fresh.nifxOffBulbs.size() ) );
		const bool bulbFlicker = !( red & WwNifxRedNoBulbFlicker );	// as celllights.cpp frameMode reads it
		int fN = 0, dN = 0;
		const std::vector<int> idx = wwNifxFrameList( fresh, true, bulbFlicker, true, &fN, &dN );
		const int flickAt = int( fresh.lights.size() );
		check( fN == 1 && dN == 2 && std::find( idx.begin(), idx.end(), flickAt ) != idx.end(),
			QStringLiteral( "three: a frame changes the flickering bulb (index %1) and the 2 dimmer tracks (%2 flicker, %3 dim)" )
			.arg( flickAt ).arg( fN ).arg( dN ) );
		int f0 = 0, d0 = 0;
		check( wwNifxFrameList( fresh, false, false, false, &f0, &d0 ).empty() && !f0 && !d0,
			QStringLiteral( "three: nothing live, no light in a frame" ) );

		// live: the cell was built before the .nifx existed
		QFile::remove( side );
		QDir( root.path() ).removeRecursively();
		QDir().mkpath( root.path() );
		WwCellLighting live = cellOf( F );
		WwNifxLive watch;
		wwNifxLiveInit( watch, live.nifxFixtures );
		const int before0 = wwNifxLiveScan( watch, live.nifxFixtures, { root.path() }, 1.5, load3 );
		QDir().mkpath( QFileInfo( side ).path() );
		QFile::copy( srcNifx, side );
		int moved = 0;
		if ( !( red & WwNifxRedNoLiveNew ) )	// as celllights.cpp nifxTick reads it
			moved = wwNifxLiveScan( watch, live.nifxFixtures, { root.path() }, 1.5, load3 );
		wwNifxLightsApply( live, docsOf( live.nifxFixtures ), red, nullptr );
		const QString liveDump = wwNifxLightsDump( live );
		check( before0 == 0 && moved == 1 && live.nifxFixtures[0].model && live.nifxFixtures[0].model->sidecar == side,
			QStringLiteral( "three: the new .nifx seen live (%1 then %2 changed, %3 folders)" ).arg( before0 ).arg( moved ).arg( watch.dirs.size() ) );
		check( liveDump == freshDump, QStringLiteral( "three: live == fresh (dark bulb, tracks, flicker): %1 vs %2 bytes" )
			.arg( liveDump.size() ).arg( freshDump.size() ) );
		if ( liveDump != freshDump )
			std::printf( "--- live\n%s--- fresh\n%s", liveDump.toUtf8().constData(), freshDump.toUtf8().constData() );

		// a second root set live (WW_NIFX_ROOT): every folder at once, the first root wins
		const QString side2 = QDir::cleanPath( root2.path() + QLatin1Char( '/' ) + rel );
		QDir().mkpath( QFileInfo( side2 ).path() );
		QFile::copy( srcNifx, side2 );
		const int moved2 = wwNifxLiveScan( watch, live.nifxFixtures, { root2.path(), root.path() }, 0.0, load3 );
		check( moved2 == 1 && live.nifxFixtures[0].model && live.nifxFixtures[0].model->sidecar == side2,
			QStringLiteral( "three: a root set live takes over (%1)" ).arg( live.nifxFixtures[0].model ? live.nifxFixtures[0].model->sidecar : QString() ) );
		// both removed: the model goes back to none
		QDir( root.path() + QStringLiteral( "/Meshes" ) ).removeRecursively();
		QDir( root2.path() + QStringLiteral( "/Meshes" ) ).removeRecursively();
		int moved3 = 0;
		for ( int k = 0; k < 2; k++ )
			moved3 += wwNifxLiveScan( watch, live.nifxFixtures, { root2.path(), root.path() }, 1.5, load3 );
		check( moved3 == 1 && !live.nifxFixtures[0].model && watch.lost == 1, QStringLiteral( "three: a removed .nifx drops live (lost %1)" ).arg( watch.lost ) );
		// a reopen's fixtures line up with what the scan has seen
		QVector<WwNifxFixture> again = { F };
		WwNifxLive seen = watch;
		wwNifxLiveInit( seen, again );
		check( !again[0].model && seen.dirs.size() == 1, QStringLiteral( "three: a republish keeps the live view" ) );
	}

	std::printf( "NIFXAPPLY %s %d checks, %d failed%s\n", fails ? "FAIL" : "PASS", checks, fails,
		red ? qPrintable( QStringLiteral( " (red %1)" ).arg( red ) ) : "" );
	return fails ? 1 : 0;
}
