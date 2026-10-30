/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellcambounds.h"

#include "esmdata.h"
#include "wwswitch.h"

#include <QByteArray>
#include <QDebug>
#include <QList>
#include <QMutex>
#include <QMutexLocker>

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <optional>
#include <utility>

// lane CAMGATE1: see cellcambounds.h

namespace
{
using Heights = std::array<float, WwCamBounds::kGrid * WwCamBounds::kGrid>;

struct BoundsDoc
{
	std::shared_ptr<EsmWorld> world;	// read-only, as the live document holds it
	WwCamBounds::Box box;
	bool interior = false;
	WwCamBounds::Room room;	// lane CAMGATE2: an interior's placements + kRoomMargin (invalid: not judged)
	quint64 gen = 0;
	bool homeValid = false;
	float home[3] = { 0, 0, 0 };
	float homeYaw = 0.0f;
	std::map<std::pair<int, int>, std::optional<Heights>> land;	// per cell, read once (a miss is kept too)
};

QMutex g_lock;
quint64 g_gen = 0;

std::map<const NifModel *, BoundsDoc> & docs()
{
	static std::map<const NifModel *, BoundsDoc> m;
	return m;
}

std::map<const NifModel *, std::pair<bool, std::array<float, 4>>> & homes()	// a spawn published before the keep
{
	static std::map<const NifModel *, std::pair<bool, std::array<float, 4>>> m;
	return m;
}

const float ( *cellHeights( BoundsDoc & d, int cx, int cy ) )[WwCamBounds::kGrid]
{
	auto it = d.land.find( { cx, cy } );
	if ( it == d.land.end() ) {
		std::optional<Heights> h;
		EsmLand l;
		if ( d.world && d.world->land( cx, cy, l ) && l.valid ) {
			h.emplace();
			for ( int r = 0; r < WwCamBounds::kGrid; r++ )
				for ( int c = 0; c < WwCamBounds::kGrid; c++ )
					( *h )[r * WwCamBounds::kGrid + c] = l.heights[r][c];
		}
		it = d.land.emplace( std::make_pair( cx, cy ), std::move( h ) ).first;
	}
	if ( !it->second )
		return nullptr;
	return reinterpret_cast<const float ( * )[WwCamBounds::kGrid]>( it->second->data() );
}
} // namespace

// lane CAMGATE2: the reds are a comma list (allow, freeze, interior), re-read on every live set
bool cellCamBoundsRedHas( const char * word )
{
	WW_SWITCH_STATIC( QList<QByteArray>, reds, wwEnv( "WW_CAMERA_BOUNDS_RED" ).trimmed().toLower().split( ',' ) );
	for ( const QByteArray & r : reds )
		if ( r.trimmed() == word )
			return true;
	return false;
}

bool cellCamBoundsRed()
{
	return cellCamBoundsRedHas( "allow" );
}

void cellCamBoundsKeep( const NifModel * nif, const std::shared_ptr<EsmWorld> & world, int cx, int cy, int n,
	bool interior, bool streamed )
{
	// lane CAMGATE2: an interior's room, read before the lock (every REFR not deleted, as the build places them)
	WwCamBounds::Room room;
	int refs = 0;
	QString edid;
	if ( interior && world ) {
		for ( const EsmRefr & r : world->interiorRefrs() ) {
			if ( r.deleted )
				continue;
			room.add( r.pos[0], r.pos[1], r.pos[2] );
			refs++;
		}
		room.grow( WwCamBounds::kRoomMargin );
		edid = world->interior().edid;
	}
	QMutexLocker lock( &g_lock );
	BoundsDoc d;
	d.world = interior ? nullptr : world;
	d.interior = interior;
	d.room = room;
	d.box.cx = cx;
	d.box.cy = cy;
	d.box.half = std::max( n, 1 ) / 2;
	d.box.roam = streamed;
	d.gen = ++g_gen;
	if ( auto h = homes().find( nif ); h != homes().end() ) {
		d.homeValid = h->second.first;
		for ( int i = 0; i < 3; i++ )
			d.home[i] = h->second.second[i];
		d.homeYaw = h->second.second[3];
	}
	bool widened = false;
	if ( d.interior && d.homeValid && !d.room.holds( d.home[0], d.home[1], d.home[2] ) ) {
		d.room.include( d.home[0], d.home[1], d.home[2], WwCamBounds::kRoomMargin );
		widened = d.room.valid;
	}
	if ( interior )
		qInfo().noquote() << QStringLiteral( "camera bounds: interior %1: %2 placements, box %3,%4,%5 .. %6,%7,%8 (+%9)%10" )
			.arg( edid.isEmpty() ? QStringLiteral( "?" ) : edid ).arg( refs )
			.arg( d.room.lo[0], 0, 'f', 0 ).arg( d.room.lo[1], 0, 'f', 0 ).arg( d.room.lo[2], 0, 'f', 0 )
			.arg( d.room.hi[0], 0, 'f', 0 ).arg( d.room.hi[1], 0, 'f', 0 ).arg( d.room.hi[2], 0, 'f', 0 )
			.arg( WwCamBounds::kRoomMargin, 0, 'f', 0 )
			.arg( !d.room.valid ? QStringLiteral( ", no placements: not judged" )
				: widened ? QStringLiteral( ", widened to the spawn" )
				: d.homeValid ? QStringLiteral( ", spawn inside" ) : QStringLiteral( ", no spawn: home = the box centre" ) );
	docs()[nif] = std::move( d );
}

void cellCamBoundsForget( const NifModel * nif )
{
	QMutexLocker lock( &g_lock );
	docs().erase( nif );
	homes().erase( nif );
}

void cellCamBoundsHome( const NifModel * nif, bool valid, const float eye[3], float yawDeg )
{
	QMutexLocker lock( &g_lock );
	homes()[nif] = { valid, { eye[0], eye[1], eye[2], yawDeg } };
	if ( auto it = docs().find( nif ); it != docs().end() ) {
		it->second.homeValid = valid;
		for ( int i = 0; i < 3; i++ )
			it->second.home[i] = eye[i];
		it->second.homeYaw = yawDeg;
		if ( valid && it->second.interior )	// lane CAMGATE2: the home is always in the room
			it->second.room.include( eye[0], eye[1], eye[2], WwCamBounds::kRoomMargin );
	}
}

bool cellCamBoundsActive( const NifModel * nif )
{
	if ( !nif || cellCamBoundsRed() )
		return false;
	const bool interiorRed = cellCamBoundsRedHas( "interior" );
	QMutexLocker lock( &g_lock );
	auto it = docs().find( nif );
	if ( it == docs().end() )
		return false;
	if ( it->second.interior )	// lane CAMGATE2: an interior with placements is judged by its room
		return !interiorRed && it->second.room.valid;
	return bool( it->second.world );
}

quint64 cellCamBoundsGen( const NifModel * nif )
{
	QMutexLocker lock( &g_lock );
	auto it = docs().find( nif );
	return it == docs().end() ? 0 : it->second.gen;
}

WwCamBounds::Verdict cellCamBoundsJudge( const NifModel * nif, double x, double y, double z, QString * why, QString * key )
{
	const bool interiorRed = cellCamBoundsRedHas( "interior" );
	QMutexLocker lock( &g_lock );
	auto it = docs().find( nif );
	if ( it == docs().end() )
		return WwCamBounds::Verdict::Ok;
	if ( it->second.interior ) {	// lane CAMGATE2: the room of the interior's placements
		const WwCamBounds::Room & r = it->second.room;
		if ( interiorRed || !r.valid )
			return WwCamBounds::Verdict::Ok;
		const WwCamBounds::Verdict v = WwCamBounds::judgeRoom( r, x, y, z );
		if ( v != WwCamBounds::Verdict::Ok ) {
			if ( key )
				*key = QStringLiteral( "%1|room" ).arg( int( v ) );
			if ( why )
				*why = QStringLiteral( "%1: eye %2,%3,%4, the room is %5,%6,%7 .. %8,%9,%10" )
					.arg( QLatin1String( WwCamBounds::verdictName( v ) ) )
					.arg( x, 0, 'f', 0 ).arg( y, 0, 'f', 0 ).arg( z, 0, 'f', 0 )
					.arg( r.lo[0], 0, 'f', 0 ).arg( r.lo[1], 0, 'f', 0 ).arg( r.lo[2], 0, 'f', 0 )
					.arg( r.hi[0], 0, 'f', 0 ).arg( r.hi[1], 0, 'f', 0 ).arg( r.hi[2], 0, 'f', 0 );
		}
		return v;
	}
	if ( !it->second.world )
		return WwCamBounds::Verdict::Ok;
	BoundsDoc & d = it->second;
	double g = 0.0;
	const WwCamBounds::Verdict v = WwCamBounds::judge( d.box,
		[&d]( int cx, int cy ) { return cellHeights( d, cx, cy ); }, x, y, z, &g );
	if ( v != WwCamBounds::Verdict::Ok ) {
		const int cx = WwCamBounds::cellOf( x ), cy = WwCamBounds::cellOf( y );
		if ( key )
			*key = QStringLiteral( "%1|%2,%3" ).arg( int( v ) ).arg( cx ).arg( cy );
		if ( why ) {
			*why = QStringLiteral( "%1: eye %2,%3,%4 in cell %5,%6" ).arg( QLatin1String( WwCamBounds::verdictName( v ) ) )
				.arg( x, 0, 'f', 0 ).arg( y, 0, 'f', 0 ).arg( z, 0, 'f', 0 ).arg( cx ).arg( cy );
			if ( v == WwCamBounds::Verdict::Under )
				*why += QStringLiteral( ", ground %1 + %2" ).arg( g, 0, 'f', 0 ).arg( WwCamBounds::kClear, 0, 'f', 0 );
			else if ( v == WwCamBounds::Verdict::Outside )
				*why += QStringLiteral( ", the loaded %1x%1 is round %2,%3" ).arg( 2 * d.box.half + 1 ).arg( d.box.cx ).arg( d.box.cy );
		}
	}
	return v;
}

bool cellCamBoundsHomeOf( const NifModel * nif, float eye[3], float & yawDeg )
{
	QMutexLocker lock( &g_lock );
	auto it = docs().find( nif );
	if ( it == docs().end() )
		return false;
	BoundsDoc & d = it->second;
	if ( d.homeValid ) {
		for ( int i = 0; i < 3; i++ )
			eye[i] = d.home[i];
		yawDeg = d.homeYaw;
		return true;
	}
	if ( d.interior ) {	// lane CAMGATE2: no spawn in an interior: the room's centre, looking north
		if ( !d.room.valid )
			return false;
		for ( int i = 0; i < 3; i++ )
			eye[i] = float( 0.5 * ( d.room.lo[i] + d.room.hi[i] ) );
		yawDeg = 0.0f;
		return true;
	}
	// no spawn: the centre cell's middle, standing on its ground, looking north
	const double x = ( d.box.cx + 0.5 ) * WwCamBounds::kCellUnits, y = ( d.box.cy + 0.5 ) * WwCamBounds::kCellUnits;
	const auto * H = cellHeights( d, d.box.cx, d.box.cy );
	if ( !H )
		return false;
	eye[0] = float( x );
	eye[1] = float( y );
	eye[2] = float( WwCamBounds::groundIn( H, d.box.cx, d.box.cy, x, y ) + WwCamBounds::kEyeHeight );
	yawDeg = 0.0f;
	return true;
}
