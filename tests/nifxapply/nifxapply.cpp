// NIFXLIGHT1 unit gate: a fixture's .nifx lights applied to a cell's lights (src/gl/cellnifxlights.cpp), the
// conversion against hand-worked answers, the 64-unit takeover and its carried state, and the fog.scale rungs.
// Usage: nifxapply <frames dir> <nifx dir> [--red words] [--dump file]; exit 0 = every check passed.
// <frames dir>/<Model>.frames: one node a line, "name|niLight 0/1|m00 m01 m02 m03 m10 .. m23" (the mesh's world
// frames, from `nifskope-cli world`); <nifx dir>/<Model>.nifx: the test lights.

#include "cellnifxlights.h"

#include <QCoreApplication>
#include <QFile>
#include <QTextStream>

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
		if ( p.size() != 3 )
			continue;
		WwNifxNode n;
		n.name = p[0];
		n.niLight = p[1] == QLatin1String( "1" );
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

	std::printf( "NIFXAPPLY %s %d checks, %d failed%s\n", fails ? "FAIL" : "PASS", checks, fails,
		red ? qPrintable( QStringLiteral( " (red %1)" ).arg( red ) ) : "" );
	return fails ? 1 : 0;
}
