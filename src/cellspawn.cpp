/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellspawn.h"
#include "cellground.h"
#include "cellsplat.h"

#include "cellview.h"
#include "esmdata.h"
#include "wwswitch.h"
#include "data/niftypes.h"

#include <QHash>
#include <QMutex>
#include <QSet>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <cstdio>

bool cellIsMarkerModel( const QString & model );   // src/cellview.cpp

namespace
{
constexpr float kCell = 4096.0f;
constexpr int kGrid = 33;
constexpr float kStep = kCell / float( kGrid - 1 );
//! A placed object stands on a point when its bounds cover the point from knee height to the eye.
constexpr float kKnee = 48.0f;

struct Box
{
	float lo[3], hi[3];
	quint32 form = 0;
	QString model;
};

float rad2degf( float r ) { return r * 57.29577951308232f; }

//! The world bounds of every placed object in the 3x3 cells round (cx, cy) that a walker could not stand inside:
//! enabled, not deleted, with a model that is not a marker, and an OBND on its base.
QVector<Box> blockers( const EsmWorld & world, int cx, int cy )
{
	QVector<Box> out;
	QVector<EsmRefr> refs;
	for ( int y = cy - 1; y <= cy + 1; y++ )
		for ( int x = cx - 1; x <= cx + 1; x++ )
			if ( world.hasCell( x, y ) )
				refs += world.refrs( x, y );
	refs += world.persistentRefrsIn( float( cx - 1 ) * kCell, float( cy - 1 ) * kCell,
		float( cx + 2 ) * kCell, float( cy + 2 ) * kCell );
	QSet<quint32> seen;
	for ( const EsmRefr & r : refs ) {
		if ( r.deleted || r.initiallyDisabled || !r.base || seen.contains( r.formID ) )
			continue;
		seen.insert( r.formID );
		const EsmLodBase & b = world.lodBase( r.base );
		if ( b.model.isEmpty() || cellIsMarkerModel( b.model ) )
			continue;
		// Sky cards and effects (fog, light shafts) are not solid: Concord's centre sat under a 13176-unit cloud card.
		const QString m = b.model.toLower().replace( QLatin1Char( '/' ), QLatin1Char( '\\' ) );
		if ( m.startsWith( QLatin1String( "sky\\" ) ) || m.contains( QLatin1String( "\\sky\\" ) )
			|| m.startsWith( QLatin1String( "effects\\" ) ) || m.contains( QLatin1String( "\\effects\\" ) ) )
			continue;
		float lo[3], hi[3];
		if ( !world.objectBounds( r.base, lo, hi ) )
			continue;
		Matrix rm;
		rm.fromEuler( -r.rot[0], -r.rot[1], -r.rot[2] );   // as the cell view places it
		const float s = r.scale > 0.0f ? r.scale : 1.0f;
		Box w;
		w.form = r.formID;
		w.model = b.model;
		for ( int k = 0; k < 3; k++ ) {
			w.lo[k] = 1.0e30f;
			w.hi[k] = -1.0e30f;
		}
		for ( int c = 0; c < 8; c++ ) {
			const Vector3 v( ( c & 1 ) ? hi[0] : lo[0], ( c & 2 ) ? hi[1] : lo[1], ( c & 4 ) ? hi[2] : lo[2] );
			const Vector3 p = Vector3( r.pos[0], r.pos[1], r.pos[2] ) + rm * ( v * s );
			for ( int k = 0; k < 3; k++ ) {
				w.lo[k] = std::min( w.lo[k], p[k] );
				w.hi[k] = std::max( w.hi[k], p[k] );
			}
		}
		// Nothing a walker meets is wider than a cell; a box that is was a card or a volume.
		if ( w.hi[0] - w.lo[0] > kCell || w.hi[1] - w.lo[1] > kCell )
			continue;
		out.append( w );
	}
	return out;
}

int standingOn( const QVector<Box> & boxes, float x, float y, float feet, QStringList * who = nullptr )
{
	int n = 0;
	for ( const Box & b : boxes )
		if ( x >= b.lo[0] && x <= b.hi[0] && y >= b.lo[1] && y <= b.hi[1]
			&& b.hi[2] >= feet + kKnee && b.lo[2] <= feet + cellEyeHeight ) {
			n++;
			if ( who && who->size() < 4 )
				who->append( QStringLiteral( "%1 %2 %3x%4x%5" ).arg( b.form, 8, 16, QLatin1Char( '0' ) )
					.arg( b.model.section( QLatin1Char( '\\' ), -1 ) ).arg( double( b.hi[0] - b.lo[0] ), 0, 'f', 0 )
					.arg( double( b.hi[1] - b.lo[1] ), 0, 'f', 0 ).arg( double( b.hi[2] - b.lo[2] ), 0, 'f', 0 ) );
		}
	return n;
}

CellSpawn exterior( const EsmWorld & world, int cx, int cy )
{
	CellSpawn s;
	EsmLand land;
	if ( !world.land( cx, cy, land ) ) {
		s.rule = QStringLiteral( "exterior %1,%2 has no LAND: the old framing" ).arg( cx ).arg( cy );
		return s;
	}
	float wh = 0.0f;
	s.water = world.cellWater( cx, cy, wh );
	s.waterHeight = wh;
	const QVector<Box> boxes = blockers( world, cx, cy );
	const float ox = float( cx ) * kCell, oy = float( cy ) * kCell;
	auto surface = [&]( int row, int col ) {
		const float g = land.heights[row][col];
		return ( s.water && wh > g ) ? wh : g;
	};
	// the centre is vertex [16][16]; the rest by distance from it, nearest first
	QVector<QPair<int, int>> order;
	for ( int row = 0; row < kGrid; row++ )
		for ( int col = 0; col < kGrid; col++ )
			order.append( { row, col } );
	std::stable_sort( order.begin(), order.end(), []( const QPair<int, int> & a, const QPair<int, int> & b ) {
		const int da = ( a.first - 16 ) * ( a.first - 16 ) + ( a.second - 16 ) * ( a.second - 16 );
		const int db = ( b.first - 16 ) * ( b.first - 16 ) + ( b.second - 16 ) * ( b.second - 16 );
		return da < db;
	} );
	const float centreSurface = surface( 16, 16 );
	QStringList who;
	s.blockers = standingOn( boxes, ox + 16 * kStep, oy + 16 * kStep, centreSurface, &who );
	s.centreObjects = who.join( QStringLiteral( "; " ) );
	int pickRow = 16, pickCol = 16;
	if ( s.blockers ) {
		bool found = false;
		for ( const auto & rc : order ) {
			if ( !standingOn( boxes, ox + rc.second * kStep, oy + rc.first * kStep, surface( rc.first, rc.second ) ) ) {
				pickRow = rc.first;
				pickCol = rc.second;
				found = true;
				break;
			}
		}
		if ( !found )
			s.rule = QStringLiteral( "exterior centre (every terrain point of the cell is covered; kept)" );
	}
	const float g = land.heights[pickRow][pickCol];
	const float feet = surface( pickRow, pickCol );
	s.eye[0] = ox + pickCol * kStep;
	s.eye[1] = oy + pickRow * kStep;
	s.eye[2] = feet + cellEyeHeight;
	s.ground = feet;
	s.yawDeg = 0.0f;
	s.moved = std::hypot( float( pickCol - 16 ), float( pickRow - 16 ) ) * kStep;
	if ( s.rule.isEmpty() ) {
		const QString on = feet > g ? QStringLiteral( "on the water" ) : QStringLiteral( "on the LAND" );
		s.rule = ( pickRow == 16 && pickCol == 16 )
			? QStringLiteral( "exterior centre, %1" ).arg( on )
			: QStringLiteral( "exterior nearest open terrain point, %1 (%2 objects stand on the centre)" )
				.arg( on ).arg( s.blockers );
	}
	s.valid = true;
	return s;
}

CellSpawn interior( const EsmWorld & world )
{
	CellSpawn s;
	const QVector<EsmRefr> refs = world.interiorRefrs();
	for ( const EsmRefr & r : refs ) {
		if ( r.deleted || !r.base )
			continue;
		if ( world.lodBase( r.base ).edid.compare( QLatin1String( "COCMarkerHeading" ), Qt::CaseInsensitive ) != 0 )
			continue;
		for ( int k = 0; k < 3; k++ )
			s.eye[k] = r.pos[k];
		s.ground = r.pos[2];
		s.eye[2] += cellEyeHeight;
		s.yawDeg = rad2degf( r.rot[2] );
		s.rule = QStringLiteral( "interior COC marker %1" ).arg( r.formID, 8, 16, QLatin1Char( '0' ) );
		s.valid = true;
		return s;
	}
	for ( const EsmRefr & r : refs ) {
		if ( r.deleted || !r.teleport )
			continue;
		float pos[3], rot[3];
		if ( !world.teleportArrival( r.teleport, pos, rot ) )
			continue;
		for ( int k = 0; k < 3; k++ )
			s.eye[k] = pos[k];
		s.ground = pos[2];
		s.eye[2] += cellEyeHeight;
		s.yawDeg = rad2degf( rot[2] );
		s.rule = QStringLiteral( "interior arrival of load door %1 (the XTEL of %2)" )
			.arg( r.formID, 8, 16, QLatin1Char( '0' ) ).arg( r.teleport, 8, 16, QLatin1Char( '0' ) );
		s.valid = true;
		return s;
	}
	s.rule = QStringLiteral( "interior has no COC marker and no load door: the old framing" );
	return s;
}

QMutex g_lock;
QHash<const void *, CellSpawn> g_spawns;

bool openWorld( const CellSceneSpec & spec, EsmWorld & world, QString * err )
{
	if ( spec.interior )
		return world.loadInterior( spec.plugins, spec.interiorCell, err );
	const QVector<QPair<quint32, QString>> worlds = EsmWorld::listWorldspaces( spec.plugins, err );
	for ( const QPair<quint32, QString> & w : worlds )
		if ( w.second.compare( spec.world, Qt::CaseInsensitive ) == 0 )
			return world.load( spec.plugins, w.first, err );
	if ( err && err->isEmpty() )
		*err = QStringLiteral( "no worldspace called \"%1\"" ).arg( spec.world );
	return false;
}

//! The checker reads the ground under the eye a second way: the LAND of the cell holding the eye, bilinear.
bool groundUnder( const EsmWorld & world, float x, float y, float & g )
{
	const int cx = int( std::floor( x / kCell ) ), cy = int( std::floor( y / kCell ) );
	EsmLand land;
	if ( !world.land( cx, cy, land ) )
		return false;
	const float fx = ( x - float( cx ) * kCell ) / kStep, fy = ( y - float( cy ) * kCell ) / kStep;
	const int c0 = std::clamp( int( std::floor( fx ) ), 0, kGrid - 2 ), r0 = std::clamp( int( std::floor( fy ) ), 0, kGrid - 2 );
	const float tx = fx - float( c0 ), ty = fy - float( r0 );
	const float a = land.heights[r0][c0] * ( 1 - tx ) + land.heights[r0][c0 + 1] * tx;
	const float b = land.heights[r0 + 1][c0] * ( 1 - tx ) + land.heights[r0 + 1][c0 + 1] * tx;
	g = a * ( 1 - ty ) + b * ty;
	return true;
}
}

QString CellSpawn::line() const
{
	if ( !valid )
		return QStringLiteral( "cell spawn: rule=%1" ).arg( rule );
	return QStringLiteral( "cell spawn: rule=%1 eye=%2,%3,%4 yaw=%5 stands=%6 water=%7 blockers=%8 moved=%9" )
		.arg( rule )
		.arg( double( eye[0] ), 0, 'f', 2 ).arg( double( eye[1] ), 0, 'f', 2 ).arg( double( eye[2] ), 0, 'f', 2 )
		.arg( double( yawDeg ), 0, 'f', 2 ).arg( double( ground ), 0, 'f', 2 )
		.arg( water ? QString::number( double( waterHeight ), 'f', 2 ) : QStringLiteral( "none" ) )
		.arg( blockers ).arg( double( moved ), 0, 'f', 1 );
}

CellSpawn cellSpawnCompute( const EsmWorld & world, const CellSceneSpec & spec )
{
	CellSpawn s = spec.interior ? interior( world ) : exterior( world, spec.cx, spec.cy );
	const QString red = wwEnvStr( "WW_CELL_SPAWN_RED" ).trimmed();
	if ( red == QLatin1String( "old" ) ) {
		s = CellSpawn();
		s.rule = QStringLiteral( "the old framing (WW_CELL_SPAWN_RED=old)" );
	} else if ( red == QLatin1String( "z0" ) && s.valid ) {
		s.eye[2] = 0.0f;
		s.rule += QStringLiteral( " RED eye z 0" );
	}
	return s;
}

void cellSpawnPublish( const void * nif, const CellSpawn & s )
{
	QMutexLocker lock( &g_lock );
	g_spawns.insert( nif, s );
}

bool cellSpawnTake( const void * nif, CellSpawn & out )
{
	QMutexLocker lock( &g_lock );
	auto it = g_spawns.find( nif );
	if ( it == g_spawns.end() )
		return false;
	out = *it;
	g_spawns.erase( it );
	return true;
}

int cellSpawnCli( const QStringList & args )
{
	QString line;
	bool check = false;
	for ( const QString & a : args ) {
		if ( a == QLatin1String( "--check" ) )
			check = true;
		else if ( line.isEmpty() )
			line = a;
	}
	CellSceneSpec spec;
	QString err;
	if ( line.isEmpty() || !cellSpecFromLine( line, spec, &err ) ) {
		std::fprintf( stderr, "cellspawn: usage: cellspawn \"plugins|world|x,y|n\" or \"plugins|interior|EDID\" [--check]%s%s\n",
			err.isEmpty() ? "" : ": ", qPrintable( err ) );
		return 2;
	}
	EsmWorld world;
	if ( !openWorld( spec, world, &err ) ) {
		std::fprintf( stderr, "cellspawn: %s\n", qPrintable( err ) );
		return 2;
	}
	const CellSpawn s = cellSpawnCompute( world, spec );
	std::printf( "%s\n", qPrintable( s.line() ) );
	if ( !s.centreObjects.isEmpty() )
		std::printf( "on the centre: %s\n", qPrintable( s.centreObjects ) );
	if ( !check )
		return s.valid ? 0 : 1;
	// the check: eye minus what it stands on = the eye height, within 1 unit, with the ground read a second way
	if ( !s.valid ) {
		std::printf( "check: FAIL no spawn (%s)\n", qPrintable( s.rule ) );
		return 1;
	}
	float under = s.ground;
	QString how = QStringLiteral( "the spawn's own floor" );
	if ( !spec.interior ) {
		float g = 0.0f;
		if ( !groundUnder( world, s.eye[0], s.eye[1], g ) ) {
			std::printf( "check: FAIL no LAND under the eye\n" );
			return 1;
		}
		under = ( s.water && s.waterHeight > g ) ? s.waterHeight : g;
		how = QStringLiteral( "LAND bilinear %1%2" ).arg( double( g ), 0, 'f', 2 )
			.arg( under > g ? QStringLiteral( ", water %1" ).arg( double( s.waterHeight ), 0, 'f', 2 ) : QString() );
	}
	const float d = s.eye[2] - under;
	const bool ok = std::fabs( d - cellEyeHeight ) <= 1.0f;
	std::printf( "check: %s eye z %.2f - surface %.2f (%s) = %.2f, want %.4f +- 1\n", ok ? "PASS" : "FAIL",
		double( s.eye[2] ), double( under ), qPrintable( how ), double( d ), double( cellEyeHeight ) );
	return ok ? 0 : 1;
}

int cellRefsCli( const QStringList & args )
{
	const QString arg = args.isEmpty() ? QString() : args.first();
	const bool groundPasses = args.contains( QLatin1String( "--ground" ) );	// lane DUNESAND1: the splat's passes
	CellSceneSpec spec;
	QString err;
	const bool file = arg.endsWith( QLatin1String( ".wwcell" ), Qt::CaseInsensitive );
	if ( arg.isEmpty() || !( file ? cellSpecFromFile( arg, spec, &err ) : cellSpecFromLine( arg, spec, &err ) ) ) {
		std::fprintf( stderr, "cellrefs: usage: cellrefs \"plugins|world|x,y|n\" or <file.wwcell>%s%s\n",
			err.isEmpty() ? "" : ": ", qPrintable( err ) );
		return 2;
	}
	const QStringList plugins = spec.plugins.split( QLatin1Char( ',' ), Qt::SkipEmptyParts );
	bool master = false;
	for ( const QString & p : plugins )
		if ( p.trimmed().section( QLatin1Char( '/' ), -1 ).section( QLatin1Char( '\\' ), -1 )
				.compare( QLatin1String( "Fallout4.esm" ), Qt::CaseInsensitive ) == 0 )
			master = true;
	std::printf( "cellrefs: plugins %d, Fallout4.esm %s, world %s\n", int( plugins.size() ), master ? "yes" : "no",
		qPrintable( spec.interior ? QStringLiteral( "interior %1" ).arg( spec.interiorCell ) : spec.world ) );
	if ( !spec.resources.isEmpty() )
		std::printf( "cellrefs: resource roots %d\n", int( spec.resources.size() ) );
	EsmWorld world;
	if ( !openWorld( spec, world, &err ) ) {
		std::printf( "cellrefs: refs 0 (%s)\n", qPrintable( err ) );
		return 2;
	}
	if ( spec.interior ) {
		std::printf( "cellrefs: interior opened\n" );
		return 0;
	}
	int x0, y0, x1, y1;
	spec.rect( x0, y0, x1, y1 );
	QVector<EsmRefr> refs;
	int cells = 0, ground = 0;
	for ( int y = y0; y <= y1; y++ )
		for ( int x = x0; x <= x1; x++ ) {
			EsmLand land;
			if ( world.land( x, y, land ) )
				ground++;
			if ( world.hasCell( x, y ) ) {
				cells++;
				refs += world.refrs( x, y );
			}
		}
	refs += world.persistentRefrsIn( float( x0 ) * kCell, float( y0 ) * kCell, float( x1 + 1 ) * kCell, float( y1 + 1 ) * kCell );
	QSet<quint32> seen;
	int placed = 0;
	for ( const EsmRefr & r : refs ) {
		if ( r.deleted || seen.contains( r.formID ) )
			continue;
		seen.insert( r.formID );
		placed++;
	}
	std::printf( "cellrefs: cells %d of %d, ground %d, refs %d\n", cells, ( x1 - x0 + 1 ) * ( y1 - y0 + 1 ), ground, placed );
	if ( groundPasses ) {
		// lane DUNESAND1: each blended-ground pass with the textures an open binds (diffuse, normal, the TX07 _s map)
		CellSplatBuild sb;
		QString serr;
		if ( !cellBuildSplat( world, x0, y0, x1, y1, 0.0f, 0.0f, CELL_GROUND_TILING, sb, &serr ) ) {
			std::printf( "cellrefs: ground passes FAIL (%s)\n", qPrintable( serr ) );
			return 1;
		}
		for ( const CellSplatBucket & b : sb.buckets )
			std::printf( "cellrefs: pass ltex %08X %s | %s | spec %s\n", b.ltex, qPrintable( b.diffuse ),
				qPrintable( b.normal ), b.spec.isEmpty() ? "-" : qPrintable( b.spec ) );
		std::printf( "cellrefs: ground passes %d, with spec %d\n", int( sb.buckets.size() ), sb.bucketsSpec );
	}
	return placed > 0 ? 0 : 1;
}
