// NIFXFIX2 unit gate (src/gl/cellnifxlights.cpp): the two .nifx light defects of the Memory Den shots (10-27).
// 1. Street: an 800 lm 2700 K omni (every Memory Den test fixture's Bulb) reaches about as far and is about as bright
//    as the vanilla ceiling lamps it replaces (NIFXCAL1 census: radius median 113, fade x Y 1.256 at 800 lm).
// 2. Dimmer: a light that names no NiLight on a mesh whose only NiLight a track plays follows that track, also when
//    the NiLight is dark when still (WallEmergencyLight01FlashOmni: Dimmer 0, no On sequence, autoLoop flashes).
// Usage: nifxfix <frames dir> <nifx dir> [--red words]; exit 0 = every check passed.
// <frames dir>/<Model>.frames: "name|niLight|m00 .. m23|dimmer|sequences|interp cycle scaled backwards start stop
// frequency phase|t:v:fwd:bwd,..." (the tests/nifxapply format). Only API that main had before this lane, so the
// same file builds against origin/main's sources (the red that must fail).

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

bool near( double a, double b, double tol ) { return std::fabs( a - b ) <= tol * std::max( 1.0, std::fabs( b ) ); }

std::shared_ptr<WwNifxModel> loadModel( const QString & framesDir, const QString & nifxDir, const QString & model )
{
	auto m = std::make_shared<WwNifxModel>();
	m->model = QStringLiteral( "SetDressing\\LightFixtures\\" ) + model;
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
		if ( p.size() > 6 ) {
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
			k->sequence = QStringLiteral( "autoLoop" );
			n.track = k;
		}
		const QStringList v = p[2].split( QLatin1Char( ' ' ), Qt::SkipEmptyParts );
		for ( int i = 0; i < 12 && i < v.size(); i++ )
			n.m[i / 4][i % 4] = v[i].toFloat();
		m->nodes.append( n );
	}
	return m;
}

WwNifxFixture fixtureOf( std::shared_ptr<const WwNifxModel> m )
{
	WwNifxFixture f;
	f.model = m;
	for ( int r = 0; r < 3; r++ )
		for ( int c = 0; c < 3; c++ )
			f.rot[r][c] = r == c ? 1.0f : 0.0f;
	f.scale = 1.0f;
	f.ref = 0xF1;
	return f;
}

struct Applied
{
	WwCellLighting L;
	QStringList census;
};

Applied applyOne( std::shared_ptr<const WwNifxModel> m, int red )
{
	Applied a;
	a.L.nifxFixtures = { fixtureOf( m ) };
	QHash<QString, nifxl::Lights> docs;
	QFile in( m->sidecar );
	if ( in.open( QIODevice::ReadOnly ) ) {
		const QByteArray b = in.readAll();
		docs.insert( m->sidecar, nifxl::read( std::string( b.constData(), size_t( b.size() ) ) ) );
	}
	wwNifxLightsApply( a.L, docs, red, &a.census );
	return a;
}

const WwCellLight * byName( const QVector<WwCellLight> & v, const char * name )
{
	for ( const WwCellLight & l : v )
		if ( l.nifxName == QLatin1String( name ) )
			return &l;
	return nullptr;
}

double lum( const WwCellLight & l ) { return 0.2126 * l.color[0] + 0.7152 * l.color[1] + 0.0722 * l.color[2]; }

}	// namespace

int main( int argc, char ** argv )
{
	QCoreApplication app( argc, argv );
	const QStringList a = app.arguments();
	if ( a.size() < 3 ) {
		std::printf( "usage: nifxfix <frames dir> <nifx dir> [--red words]\n" );
		return 2;
	}
	const int ri = int( a.indexOf( QStringLiteral( "--red" ) ) );
	const int red = ri > 0 && ri + 1 < a.size() ? wwNifxRedOf( a[ri + 1].toUtf8() ) : 0;
	const auto flash = loadModel( a[1], a[2], QStringLiteral( "WallEmergencyLight01FlashOmni.nif" ) );
	const WwNifxNode * o0 = nullptr;
	for ( const WwNifxNode & n : flash->nodes )
		if ( n.niLight )
			o0 = &n;
	check( flash->nodes.size() == 5 && o0 && o0->track && o0->dimmer == 0.0f && QFile::exists( flash->sidecar ),
		QStringLiteral( "fixture: flash mesh %1 nodes, its NiLight still dimmer %2 with a %3-key track" )
		.arg( flash->nodes.size() ).arg( o0 ? o0->dimmer : -1.0f ).arg( o0 && o0->track ? int( o0->track->t.size() ) : 0 ) );

	// --- 1. the street: reach and brightness of the Memory Den Bulb against the vanilla lamps it replaces
	const Applied s = applyOne( flash, red );
	const WwCellLight * bulb = byName( s.L.lights, "Bulb" );
	const double r = bulb ? bulb->radius : -1.0;
	check( bulb && r >= 113.0 * 0.75 && r <= 113.0 * 1.33,
		QStringLiteral( "street: 800 lm omni radius %1 (vanilla ceiling lamps median 113; 85..150)" ).arg( r ) );
	// the still colour without the dimmer: a dimmer-free twin of the mesh (no NiLight)
	auto plain = std::make_shared<WwNifxModel>( *flash );
	for ( WwNifxNode & n : plain->nodes )
		n.niLight = false, n.track.reset();
	const Applied p = applyOne( plain, red );
	const WwCellLight * pb = byName( p.L.lights, "Bulb" );
	const double y = pb ? lum( *pb ) : -1.0;
	check( pb && near( y, 1.256, 0.04 ),
		QStringLiteral( "street: 800 lm omni fade x Y %1 (vanilla household bulb 1.256)" ).arg( y, 0, 'f', 4 ) );

	// --- 2. the dimmer: both lights (Bulb normal, Glow bulb tier) follow the only NiLight's flash
	const WwCellLight * glow = byName( s.L.nifxBulbs, "Glow" );
	auto at = []( const WwCellLight * l, double t ) { return l ? double( wwNifxDimFactor( *l, t ) ) : -1.0; };
	check( bulb && bulb->nifxDim && glow && glow->nifxDim,
		QStringLiteral( "dimmer: Bulb and Glow name no NiLight and follow object0 (tracks %1 %2)" )
		.arg( bulb && bulb->nifxDim ).arg( glow && glow->nifxDim ) );
	check( near( at( bulb, 0.5 ), 1.0, 1e-4 ) && near( at( bulb, 1.0 ), 0.0, 1e-4 ) && near( at( bulb, 1.5 ), 1.0, 1e-4 )
		&& near( at( glow, 1.0 ), 0.0, 1e-4 ),
		QStringLiteral( "dimmer: flash factor at 0.5 / 1.0 / 1.5 s = %1 / %2 / %3 (want 1 / 0 / 1)" )
		.arg( at( bulb, 0.5 ) ).arg( at( bulb, 1.0 ) ).arg( at( bulb, 1.5 ) ) );
	check( bulb && pb && near( lum( *bulb ), lum( *pb ), 1e-4 ) && bulb->nifxDimRest == 1.0f,
		QStringLiteral( "dimmer: still Dimmer 0 is lit at full for its track (Y %1 vs %2, rest %3)" )
		.arg( bulb ? lum( *bulb ) : -1.0 ).arg( pb ? lum( *pb ) : -1.0 ).arg( bulb ? bulb->nifxDimRest : -1.0f ) );
	int fN = 0, dN = 0;
	wwNifxFrameList( s.L, true, true, true, &fN, &dN );
	check( dN == 2, QStringLiteral( "dimmer: a World animation frame scales %1 lights (want 2)" ).arg( dN ) );
	const QString all = s.census.join( QLatin1Char( '\n' ) );
	check( all.contains( QStringLiteral( "2 lights follow the mesh's only NiLight (no follows named), 2 dark still dimmers lit for their track" ) ),
		QStringLiteral( "dimmer: census names the 2 default follows" ) );

	// --- no default where the state is not one NiLight's: two tracked NiLights, or one that no track plays
	auto two = std::make_shared<WwNifxModel>( *flash );
	WwNifxNode second = *o0;
	second.name = QStringLiteral( "object1" );
	two->nodes.append( second );
	const Applied t2 = applyOne( two, red );
	auto still = std::make_shared<WwNifxModel>( *flash );
	for ( WwNifxNode & n : still->nodes )
		n.track.reset();
	const Applied t1 = applyOne( still, red );
	const WwCellLight * b2 = byName( t2.L.lights, "Bulb" ), * b1 = byName( t1.L.lights, "Bulb" );
	check( b2 && !b2->nifxDim && near( lum( *b2 ), y, 1e-4 ) && b1 && !b1->nifxDim && near( lum( *b1 ), y, 1e-4 ),
		QStringLiteral( "steady: two NiLights or an untracked one leave the light at full (Y %1, %2)" )
		.arg( b2 ? lum( *b2 ) : -1.0 ).arg( b1 ? lum( *b1 ) : -1.0 ) );

	std::printf( "NIFXFIX %s %d checks, %d failed%s\n", fails ? "FAIL" : "PASS", checks, fails,
		red ? qPrintable( QStringLiteral( " (red %1)" ).arg( red ) ) : "" );
	return fails ? 1 : 0;
}
