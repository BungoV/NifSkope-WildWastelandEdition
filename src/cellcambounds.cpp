/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellcambounds.h"

#include "esmdata.h"
#include "wwswitch.h"

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

bool cellCamBoundsRed()
{
	WW_SWITCH_STATIC( bool, red, wwEnv( "WW_CAMERA_BOUNDS_RED" ).trimmed().toLower() == "allow" );
	return red;
}

void cellCamBoundsKeep( const NifModel * nif, const std::shared_ptr<EsmWorld> & world, int cx, int cy, int n,
	bool interior, bool streamed )
{
	QMutexLocker lock( &g_lock );
	BoundsDoc d;
	d.world = interior ? nullptr : world;
	d.interior = interior;
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
	}
}

bool cellCamBoundsActive( const NifModel * nif )
{
	if ( !nif || cellCamBoundsRed() )
		return false;
	QMutexLocker lock( &g_lock );
	auto it = docs().find( nif );
	return it != docs().end() && !it->second.interior && it->second.world;
}

quint64 cellCamBoundsGen( const NifModel * nif )
{
	QMutexLocker lock( &g_lock );
	auto it = docs().find( nif );
	return it == docs().end() ? 0 : it->second.gen;
}

WwCamBounds::Verdict cellCamBoundsJudge( const NifModel * nif, double x, double y, double z, QString * why, QString * key )
{
	QMutexLocker lock( &g_lock );
	auto it = docs().find( nif );
	if ( it == docs().end() || it->second.interior || !it->second.world )
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
