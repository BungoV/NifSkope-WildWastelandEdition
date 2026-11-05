// Lane TOOLS1101 (REFLPROBECLI1, 2026-11-01): `nifskope -no-gui reflcensus`, how many of a real probe cube's texels
// miss below the horizon (SKYBELOW2 turns those to ground), counted through a bake's soup with no window.
// src/reflcensus.h holds the rule and what it mirrors; tests/reflcensus/run.sh is its gate.
#include "reflcensuscli.h"

#include "features/gi/probebvh.h"
#include "features/gi/probeplace.h"
#include "reflcensus.h"

#include <QFile>
#include <QTextStream>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
const char * kUsage =
	"usage: nifskope -no-gui reflcensus --soup <soup.psp> (--camera x,y,z ... | --auto N) [--interior] [--eye 128]\n"
	"         [--fit] [--red lift|noreach|roof|stand|place] [--out <probes.tsv>]\n"
	"  the probes the reflection pass places round each camera (the camera probe + the 3x3 grid of 2048 boxes),\n"
	"  each one's 64x64 oct tile traced through the soup: hits, misses above (sky) and below the horizon (ground\n"
	"  since SKYBELOW2), below misses with no ground tap hit (albedo 0.5) and below misses that run past the soup.\n"
	"  --auto N: an N x N grid of cameras over the soup's middle 80 %, each at the ground under it + eye (128);\n"
	"  with --interior eye over every floor under a ceiling (red roof: the topmost surface, often the roof).\n"
	"  the misses past the soup split by where the probe stands: outside the soup's xy box, in the void (no surface\n"
	"  in its column, under the lowest, indoors over the topmost) or in the geometry (red stand: no split).\n"
	"  --fit (WW_CELL_REFLPLACE=1): interior grid probes fitted into the soup's rooms (src/reflplace.h: kept, moved,\n"
	"  dropped); red place: the old placement (WW_CELL_REFLPLACE_RED=1).\n";

void line( const QString & s )
{
	std::fprintf( stdout, "%s\n", qPrintable( s ) );
}
}   // namespace

int reflCensusCli( const QStringList & args )
{
	QString soupPath, outPath;
	std::vector<std::array<double, 3>> cams;
	int autoN = 0, red = reflcensus::RedNone;
	double eye = 128.0;
	bool interior = false, fitOn = false;
	for ( int i = 0; i < args.size(); i++ ) {
		const QString & a = args[i];
		const QString nx = i + 1 < args.size() ? args[i + 1] : QString();
		if ( a == QLatin1String( "--soup" ) ) { soupPath = nx; i++; }
		else if ( a == QLatin1String( "--out" ) ) { outPath = nx; i++; }
		else if ( a == QLatin1String( "--auto" ) ) { autoN = nx.toInt(); i++; }
		else if ( a == QLatin1String( "--eye" ) ) { eye = nx.toDouble(); i++; }
		else if ( a == QLatin1String( "--interior" ) ) interior = true;
		else if ( a == QLatin1String( "--fit" ) ) fitOn = true;	// lane REFLPLACE1
		else if ( a == QLatin1String( "--red" ) ) {
			for ( const QString & r : nx.split( ',' ) ) {
				if ( r == QLatin1String( "lift" ) ) red |= reflcensus::RedLift;
				else if ( r == QLatin1String( "noreach" ) ) red |= reflcensus::RedNoReach;
				else if ( r == QLatin1String( "roof" ) ) red |= reflcensus::RedRoof;	// lane INTGI1
				else if ( r == QLatin1String( "stand" ) ) red |= reflcensus::RedStand;
				else if ( r == QLatin1String( "place" ) ) red |= reflcensus::RedPlace;	// lane REFLPLACE1
				else { std::fprintf( stderr, "reflcensus: REFUSED: unknown red '%s' (lift, noreach, roof, stand, place)\n", qPrintable( r ) ); return 2; }
			}
			i++;
		}
		else if ( a == QLatin1String( "--camera" ) ) {
			const QStringList v = nx.split( ',' );
			if ( v.size() != 3 ) { std::fprintf( stderr, "reflcensus: REFUSED: --camera wants x,y,z, got '%s'\n", qPrintable( nx ) ); return 2; }
			cams.push_back( { v[0].toDouble(), v[1].toDouble(), v[2].toDouble() } );
			i++;
		}
		else if ( a != QLatin1String( "reflcensus" ) ) { std::fprintf( stderr, "reflcensus: REFUSED: unknown argument '%s'\n%s", qPrintable( a ), kUsage ); return 2; }
	}
	if ( soupPath.isEmpty() || ( cams.empty() && autoN <= 0 ) ) {
		std::fprintf( stderr, "reflcensus: REFUSED: %s\n%s", soupPath.isEmpty() ? "no --soup" : "no --camera and no --auto", kUsage );
		return 2;
	}
	ProbeSoup soup;
	QString error;
	if ( !probeSoupRead( soupPath, &soup, &error ) ) {
		std::fprintf( stderr, "reflcensus: REFUSED: cannot read the soup %s: %s\n", qPrintable( soupPath ), qPrintable( error ) );
		return 1;
	}
	const size_t nTri = soup.tris.size() / 9;
	if ( nTri == 0 ) {
		std::fprintf( stderr, "reflcensus: REFUSED: the soup %s has no triangles (a count over it would be all sky)\n", qPrintable( soupPath ) );
		return 1;
	}
	double mn[3] = { 1e300, 1e300, 1e300 }, mx[3] = { -1e300, -1e300, -1e300 };
	for ( size_t i = 0; i < soup.tris.size(); i += 3 )
		for ( int k = 0; k < 3; k++ ) {
			mn[k] = std::min( mn[k], double( soup.tris[i + size_t( k )] ) );
			mx[k] = std::max( mx[k], double( soup.tris[i + size_t( k )] ) );
		}
	// the bake's tracer over the soup's opaque triangles (the soup keeps glass apart; the count leaves it out and says
	// so), local to the soup's middle; the alpha masks number these triangles
	probebvh::Bvh bvh;
	const double O[2] = { std::floor( ( mn[0] + mx[0] ) * 0.5 ), std::floor( ( mn[1] + mx[1] ) * 0.5 ) };
	bvh.t.reserve( soup.tris.size() );
	for ( size_t i = 0; i < soup.tris.size(); i += 3 ) {
		bvh.t.push_back( float( double( soup.tris[i + 0] ) - O[0] ) );
		bvh.t.push_back( float( double( soup.tris[i + 1] ) - O[1] ) );
		bvh.t.push_back( soup.tris[i + 2] );
	}
	bvh.build();
	bvh.mask = &soup.amask;
	const double soupXY[4] = { mn[0], mn[1], mx[0], mx[1] };
	// lane REFLPLACE1: the footprint the pass fits its interior grid probes into (the same function, the same soup)
	reflplace::Footprint fp;
	if ( fitOn )
		reflplace::build( soup.tris.data(), nTri, fp );

	line( QStringLiteral( "reflcensus: soup %1: %2 triangles (%3 glass not traced, %4 alpha masks), box %5,%6,%7 .. %8,%9,%10" )
		.arg( soupPath ).arg( nTri ).arg( soup.glass.size() / 9 ).arg( soup.amask.tris.size() )
		.arg( mn[0], 0, 'f', 0 ).arg( mn[1], 0, 'f', 0 ).arg( mn[2], 0, 'f', 0 ).arg( mx[0], 0, 'f', 0 ).arg( mx[1], 0, 'f', 0 ).arg( mx[2], 0, 'f', 0 ) );
	line( QStringLiteral( "reflcensus: rule: tile %1x%1, near 4, reach max(2*|box corner|,4096) planar, below = d.z < %2%3; %4" )
		.arg( reflcensus::kFace ).arg( ( red & reflcensus::RedLift ) ? QStringLiteral( "0.1 (RED lift)" ) : QStringLiteral( "0" ) )
		.arg( ( red & reflcensus::RedNoReach ) ? QStringLiteral( ", RED noreach (no far plane)" ) : QString() )
		.arg( interior ? QStringLiteral( "interior (camera box 1024,1024,512; below misses draw 0, not ground)" )
			: QStringLiteral( "exterior (camera box 4096,4096,2048; below misses draw the ground)" ) ) );
	line( QStringLiteral( "reflcensus: not modelled: the pass's sphere cull and its sun-shadow caster set (the soup is the bake's statics); "
		"the rooms placement (old PRT GI interiors): the grid is placed, as RTXGI cells get" ) );

	if ( autoN > 0 ) {   // cameras on the ground under an N x N grid over the soup's middle 80 %
		// lane INTGI1 (REFLPROBECLI2): indoors eye over every floor under a ceiling (reflcensus.h floorsOf), not on
		// the roof; red `roof` (and every exterior) the topmost surface as before
		const bool rooms = interior && !( red & reflcensus::RedRoof );
		int noGround = 0;
		std::vector<double> zs, camZ;
		for ( int j = 0; j < autoN; j++ )
			for ( int i = 0; i < autoN; i++ ) {
				const double x = mn[0] + ( mx[0] - mn[0] ) * ( 0.1 + 0.8 * ( i + 0.5 ) / autoN );
				const double y = mn[1] + ( mx[1] - mn[1] ) * ( 0.1 + 0.8 * ( j + 0.5 ) / autoN );
				if ( rooms ) {
					reflcensus::columnHits( bvh, x - O[0], y - O[1], mx[2] + 16.0, mx[2] - mn[2] + 32.0, zs );
					camZ.clear();
					reflcensus::floorsOf( zs, eye, red, camZ );
					noGround += camZ.empty() ? 1 : 0;
					for ( double z : camZ )
						cams.push_back( { x, y, z } );
					continue;
				}
				const double o[3] = { x - O[0], y - O[1], mx[2] + 16.0 }, d[3] = { 0.0, 0.0, -1.0 };
				double th = 0.0;
				if ( !bvh.ray( o, d, mx[2] - mn[2] + 32.0, &th ) ) {
					noGround++;
					continue;
				}
				cams.push_back( { x, y, mx[2] + 16.0 - th + eye } );
			}
		if ( rooms )
			line( QStringLiteral( "reflcensus: auto %1x%1 rooms: %2 cameras at eye %3 over each floor under a ceiling (headroom eye + %4), "
				"%5 columns refused (no such floor)" ).arg( autoN ).arg( int( cams.size() ) ).arg( eye, 0, 'f', 0 )
				.arg( reflcensus::kHeadroom, 0, 'f', 0 ).arg( noGround ) );
		else
			line( QStringLiteral( "reflcensus: auto %1x%1: %2 cameras on the ground + %3, %4 refused (no ground under them)%5" )
				.arg( autoN ).arg( int( cams.size() ) ).arg( eye, 0, 'f', 0 ).arg( noGround )
				.arg( interior ? QStringLiteral( " (RED roof: the topmost surface indoors)" ) : QString() ) );
		if ( cams.empty() ) {
			std::fprintf( stderr, "reflcensus: REFUSED: no auto camera found ground\n" );
			return 1;
		}
	}

	QFile tsv( outPath );
	QTextStream ts( &tsv );
	if ( !outPath.isEmpty() ) {
		if ( !tsv.open( QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text ) ) {
			std::fprintf( stderr, "reflcensus: REFUSED: cannot write %s\n", qPrintable( outPath ) );
			return 1;
		}
		ts << "camera\tcx\tcy\tcz\tprobe\tkind\tx\ty\tz\treach\ttexels\thits\tabove\tbelow\tbelowTapless\tbelowEdge\ttaps\tstand\tfit\n";
	}
	reflcensus::Count all;
	long probes = 0, taplessProbes = 0, camBelow = 0, camTexels = 0;
	long edgeBy[3] = { 0, 0, 0 }, probesBy[3] = { 0, 0, 0 };	// by reflcensus::Stand
	static const char * const kStand[3] = { "geometry", "outside", "void" };
	static const char * const kFit[4] = { "-", "kept", "moved", "dropped" };
	long fitN[3] = { 0, 0, 0 };	// lane REFLPLACE1: interior grid probes kept, moved, dropped
	std::vector<reflcensus::Probe> P;
	for ( size_t ci = 0; ci < cams.size(); ci++ ) {
		reflcensus::place( cams[ci].data(), interior, P, fitOn ? &fp : nullptr, red );
		long placedGrid = 0;
		for ( const reflcensus::Probe & q : P ) {
			placedGrid += q.kind == 1 ? 1 : 0;
			if ( q.fit >= 0 )
				fitN[q.fit]++;
		}
		fitN[reflplace::Dropped] += fitOn && interior && !( red & reflcensus::RedPlace ) ? 9 - placedGrid : 0;
		reflcensus::Count cc;
		for ( size_t pi = 0; pi < P.size(); pi++ ) {
			const reflcensus::Count c = reflcensus::census( bvh, O, P[pi], red, soupXY );
			const int st = reflcensus::standOf( bvh, O, P[pi], soupXY, mx[2] + 16.0, mx[2] - mn[2] + 32.0, interior, red );
			edgeBy[st] += c.belowEdge;
			probesBy[st]++;
			probes++;
			taplessProbes += c.tapsHit == 0 ? 1 : 0;
			cc.texels += c.texels; cc.hits += c.hits; cc.misses += c.misses; cc.above += c.above; cc.below += c.below;
			cc.belowTapless += c.belowTapless; cc.belowEdge += c.belowEdge;
			if ( P[pi].kind == 0 ) {
				camBelow += c.below;
				camTexels += c.texels;
			}
			if ( !outPath.isEmpty() )
				ts << ci << '\t' << cams[ci][0] << '\t' << cams[ci][1] << '\t' << cams[ci][2] << '\t' << pi << '\t'
				   << ( P[pi].kind == 0 ? "camera" : "grid" ) << '\t' << P[pi].pos[0] << '\t' << P[pi].pos[1] << '\t' << P[pi].pos[2]
				   << '\t' << c.reach << '\t' << c.texels << '\t' << c.hits << '\t' << c.above << '\t' << c.below << '\t'
				   << c.belowTapless << '\t' << c.belowEdge << '\t' << c.tapsHit << '\t' << kStand[st] << '\t' << kFit[P[pi].fit + 1] << '\n';
		}
		line( QStringLiteral( "camera %1 %2,%3,%4: %5 probes, texels %6: hits %7, sky misses %8, below misses %9 (%10 %), "
			"tapless %11, past the soup %12" )
			.arg( ci ).arg( cams[ci][0], 0, 'f', 0 ).arg( cams[ci][1], 0, 'f', 0 ).arg( cams[ci][2], 0, 'f', 0 ).arg( int( P.size() ) )
			.arg( cc.texels ).arg( cc.hits ).arg( cc.above ).arg( cc.below ).arg( 100.0 * cc.below / std::max( 1L, cc.texels ), 0, 'f', 2 )
			.arg( cc.belowTapless ).arg( cc.belowEdge ) );
		all.texels += cc.texels; all.hits += cc.hits; all.misses += cc.misses; all.above += cc.above; all.below += cc.below;
		all.belowTapless += cc.belowTapless; all.belowEdge += cc.belowEdge;
	}
	auto pc = [&]( long n ) { return QString::number( 100.0 * n / std::max( 1L, all.texels ), 'f', 2 ); };
	line( QStringLiteral( "reflcensus total: %1 cameras, %2 probes, %3 texels: hits %4 (%5 %), sky misses %6 (%7 %), below-horizon misses %8 (%9 %; "
		"camera probes %10 of %11), of them tapless %12 (%13 probes with no ground tap) and past the soup %14" )
		.arg( int( cams.size() ) ).arg( probes ).arg( all.texels ).arg( all.hits ).arg( pc( all.hits ) ).arg( all.above ).arg( pc( all.above ) )
		.arg( all.below ).arg( pc( all.below ) ).arg( camBelow ).arg( camTexels ).arg( all.belowTapless ).arg( taplessProbes ).arg( all.belowEdge ) );
	line( QStringLiteral( "reflcensus past the soup by where the probe stands: outside the soup box %1 (%2 probes), in the void %3 (%4), "
		"in the geometry %5 (%6)%7" )
		.arg( edgeBy[reflcensus::StandOutside] ).arg( probesBy[reflcensus::StandOutside] ).arg( edgeBy[reflcensus::StandVoid] )
		.arg( probesBy[reflcensus::StandVoid] ).arg( edgeBy[reflcensus::StandGeometry] ).arg( probesBy[reflcensus::StandGeometry] )
		.arg( ( red & reflcensus::RedStand ) ? QStringLiteral( " (RED stand: no split)" ) : QString() ) );
	if ( fitOn )   // lane REFLPLACE1
		line( !interior ? QStringLiteral( "reflcensus fit: exterior, nothing fitted (the pass fits interior grid probes only)" )
			: ( red & reflcensus::RedPlace ) ? QStringLiteral( "reflcensus fit: RED place: the old interior grid placement" )
			: QStringLiteral( "reflcensus fit: footprint %1 x %2 columns of %3 u over %4 triangles; interior grid probes kept %5, moved %6, dropped %7" )
				.arg( fp.nx ).arg( fp.ny ).arg( fp.cell, 0, 'f', 0 ).arg( qulonglong( fp.tris ) )
				.arg( fitN[reflplace::Kept] ).arg( fitN[reflplace::Moved] ).arg( fitN[reflplace::Dropped] ) );
	if ( all.hits + all.above + all.below != all.texels ) {
		std::fprintf( stderr, "reflcensus: REFUSED: hits + above + below != texels\n" );
		return 1;
	}
	return 0;
}
