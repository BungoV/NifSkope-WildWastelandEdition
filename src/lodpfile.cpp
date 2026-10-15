/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

/* The release build is -O3 -march=haswell, where GCC fuses a*b+c into one FMA by default. Patch
 * geometry must come out bit for bit what the independent twin (tests/spells/lodp_decode.py)
 * computes, and the 45-degree crease test sits on exact ties, so this unit does plain IEEE
 * arithmetic (lane LODP1, 2026-10-15: FMA noise of 1e-17 split 5 bevelled meshes). */
#pragma GCC optimize( "fp-contract=off" )

#include "lodpfile.h"
#include "lodofile.h"
#include "io/lodvfile.h"

#include <QFile>

#include <algorithm>
#include <cmath>
#include <cstring>

/* ---- packing ---- */

quint16 lodpPackOct8( const float n[3] )
{
	float ax = std::fabs( n[0] ), ay = std::fabs( n[1] ), az = std::fabs( n[2] );
	float s = ax + ay + az;
	if ( !( s > 0.0f ) )
		s = 1.0f;
	float x = n[0] / s, y = n[1] / s;
	if ( n[2] < 0.0f ) {
		float ox = ( 1.0f - std::fabs( y ) ) * ( x >= 0.0f ? 1.0f : -1.0f );
		float oy = ( 1.0f - std::fabs( x ) ) * ( y >= 0.0f ? 1.0f : -1.0f );
		x = ox;
		y = oy;
	}
	auto q = []( float v ) {
		int i = int( std::lround( ( std::clamp( v, -1.0f, 1.0f ) * 0.5f + 0.5f ) * 255.0f ) );
		return quint16( std::clamp( i, 0, 255 ) );
	};
	return quint16( q( x ) | ( q( y ) << 8 ) );
}

void lodpUnpackOct8( quint16 packed, float n[3] )
{
	float x = float( packed & 0xFF ) / 255.0f * 2.0f - 1.0f;
	float y = float( packed >> 8 ) / 255.0f * 2.0f - 1.0f;
	float z = 1.0f - std::fabs( x ) - std::fabs( y );
	if ( z < 0.0f ) {
		float ox = ( 1.0f - std::fabs( y ) ) * ( x >= 0.0f ? 1.0f : -1.0f );
		float oy = ( 1.0f - std::fabs( x ) ) * ( y >= 0.0f ? 1.0f : -1.0f );
		x = ox;
		y = oy;
	}
	float l = std::sqrt( x * x + y * y + z * z );
	n[0] = x / l;
	n[1] = y / l;
	n[2] = z / l;
}

quint16 lodpHalfFromFloat( float f )
{
	quint32 b;
	std::memcpy( &b, &f, 4 );
	quint32 sign = ( b >> 16 ) & 0x8000U;
	quint32 ex = ( b >> 23 ) & 0xFF;
	quint32 man = b & 0x7FFFFFU;
	if ( ex == 0xFF )
		return quint16( sign | 0x7C00U | ( man ? 0x200U : 0U ) );
	int e = int( ex ) - 127 + 15;
	if ( e >= 31 )
		return quint16( sign | 0x7C00U );
	if ( e <= 0 ) {
		if ( e < -10 )
			return quint16( sign );
		man |= 0x800000U;
		int shift = 14 - e;
		quint32 h = man >> shift;
		quint32 rem = man & ( ( 1U << shift ) - 1 );
		quint32 half = 1U << ( shift - 1 );
		if ( rem > half || ( rem == half && ( h & 1U ) ) )
			h++;
		return quint16( sign | h );
	}
	quint32 h = ( quint32( e ) << 10 ) | ( man >> 13 );
	quint32 rem = man & 0x1FFFU;
	if ( rem > 0x1000U || ( rem == 0x1000U && ( h & 1U ) ) )
		h++;
	return quint16( sign | h );
}

float lodpFloatFromHalf( quint16 h )
{
	quint32 sign = quint32( h & 0x8000U ) << 16;
	quint32 ex = ( h >> 10 ) & 0x1F;
	quint32 man = h & 0x3FFU;
	quint32 b;
	if ( ex == 0 ) {
		if ( man == 0 ) {
			b = sign;
		} else {
			int e = -1;
			do {
				e++;
				man <<= 1;
			} while ( !( man & 0x400U ) );
			b = sign | ( quint32( 127 - 15 - e ) << 23 ) | ( ( man & 0x3FFU ) << 13 );
		}
	} else if ( ex == 31 ) {
		b = sign | 0x7F800000U | ( man << 13 );
	} else {
		b = sign | ( ( ex - 15 + 127 ) << 23 ) | ( man << 13 );
	}
	float f;
	std::memcpy( &f, &b, 4 );
	return f;
}

int lodpSectorOf( int cell )
{
	return cell >= 0 ? cell / LODP_SECTOR_CELLS : -( ( -cell + LODP_SECTOR_CELLS - 1 ) / LODP_SECTOR_CELLS );
}

int lodpCellIndex( int cellX, int cellY, int sectorX, int sectorY )
{
	return ( cellY - sectorY * LODP_SECTOR_CELLS ) * LODP_SECTOR_CELLS + ( cellX - sectorX * LODP_SECTOR_CELLS );
}

QString lodpFileName( int sectorX, int sectorY )
{
	return QString( "lodp_%1_%2.lodp" ).arg( sectorX ).arg( sectorY );
}

quint64 lodpOptionsHash( float patchUnits, int terrainGrid )
{
	quint64 h = lodoFnv1a64( "lodp-options", 12 );
	quint32 v = LODP_VERSION;
	h = lodoFnv1a64( &v, 4, h );
	h = lodoFnv1a64( &patchUnits, 4, h );
	qint32 g = terrainGrid;
	h = lodoFnv1a64( &g, 4, h );
	double c = LODP_CREASE_COS;
	h = lodoFnv1a64( &c, 8, h );
	return h;
}

/* ---- layout ---- */

namespace {

// header field offsets (docs section 13.1)
enum : int
{
	H_MAGIC = 0x00, H_VERSION = 0x04, H_FLAGS = 0x08, H_HCRC = 0x0C,
	H_WS = 0x10, H_INPUT = 0x18, H_LODO = 0x20, H_OPTIONS = 0x28,
	H_SX = 0x30, H_SY = 0x34, H_PATCH = 0x38, H_AZ = 0x3C, H_GRID = 0x3E, H_RES0 = 0x3F,
	H_NCELL = 0x40, H_NINST = 0x44, H_NCHART = 0x48, H_NSLOT = 0x4C, H_NPATCH = 0x50, H_NLINK = 0x54,
	H_ICRC = 0x58, H_RES1 = 0x5C,
	H_OCELL = 0x60, H_OINST = 0x68, H_OCHART = 0x70, H_OSLOT = 0x78, H_OPATCH = 0x80, H_OLINK = 0x88,
	H_FILE = 0x90, H_RESERVED = 0x98
};

template <typename T> void put( QByteArray & b, int at, T v ) { std::memcpy( b.data() + at, &v, sizeof( T ) ); }
template <typename T> T get( const QByteArray & b, qint64 at ) { T v; std::memcpy( &v, b.constData() + at, sizeof( T ) ); return v; }

struct Layout
{
	quint64 oCell, oInst, oChart, oSlot, oPatch, oLink, end;
};

Layout layoutOf( quint64 ni, quint64 nc, quint64 ns, quint64 np, quint64 nl )
{
	Layout L;
	L.oCell = LODP_HEADER_BYTES;
	L.oInst = L.oCell + quint64( LODP_SECTOR_CELL_COUNT ) * sizeof( LodpCell );
	L.oChart = L.oInst + ni * sizeof( LodpInstance );
	L.oSlot = L.oChart + nc * sizeof( LodpChart );
	L.oPatch = L.oSlot + ns * 4;
	L.oLink = L.oPatch + np * sizeof( LodpPatch );
	L.end = L.oLink + nl * sizeof( LodpLink );
	return L;
}

bool finite( float v ) { return std::isfinite( v ); }

/*! Every rule the contract has, on a decoded sector. The writer runs it on
 *  what it is about to write and the reader on what it read: one list. */
bool checkSector( const LodpSector & s, QString * why )
{
	auto fail = [why]( const QString & m ) { if ( why ) *why = m; return false; };
	if ( s.flags & ~LODP_FLAGS_KNOWN )
		return fail( QString( "header flags 0x%1 carry unknown bits" ).arg( s.flags, 0, 16 ) );
	if ( !( s.patchUnits > 0.0f ) || !finite( s.patchUnits ) )
		return fail( QString( "patch size %1 is not positive" ).arg( s.patchUnits ) );
	const int G = s.terrainGrid;
	if ( G < 1 || G > 64 )
		return fail( QString( "terrain grid %1 is outside 1..64" ).arg( G ) );
	if ( s.cells.size() != size_t( LODP_SECTOR_CELL_COUNT ) )
		return fail( QString( "cell table holds %1 rows, not 256" ).arg( s.cells.size() ) );
	const quint32 np = quint32( s.patches.size() ), nc = quint32( s.charts.size() );
	const quint32 ni = quint32( s.instances.size() ), ns = quint32( s.slotIds.size() );
	quint32 nextPatch = 0, nextChart = 0;
	std::vector<quint32> cellOfChart( nc, LODP_NONE ), cellOfPatch( np, LODP_NONE );
	for ( int ci = 0; ci < LODP_SECTOR_CELL_COUNT; ci++ ) {
		const LodpCell & c = s.cells[ci];
		if ( c.reserved )
			return fail( QString( "cell %1: reserved word is %2, not 0" ).arg( ci ).arg( c.reserved ) );
		if ( c.flags & LODP_CELL_QUEST_LAYER )
			return fail( QString( "cell %1: quest-layer flag is PROPOSED (P1) and refused in v1" ).arg( ci ) );
		if ( c.flags & ~LODP_CELL_FLAGS_KNOWN )
			return fail( QString( "cell %1: flags 0x%2 carry unknown bits" ).arg( ci ).arg( c.flags, 0, 16 ) );
		if ( !finite( c.zBase ) )
			return fail( QString( "cell %1: zBase is not finite" ).arg( ci ) );
		if ( c.firstPatch != nextPatch )
			return fail( QString( "cell %1: first patch %2, but the cells before it end at %3 (patches are in cell order)" )
				.arg( ci ).arg( c.firstPatch ).arg( nextPatch ) );
		if ( c.firstChart != nextChart )
			return fail( QString( "cell %1: first chart %2, but the cells before it end at %3 (charts are in cell order)" )
				.arg( ci ).arg( c.firstChart ).arg( nextChart ) );
		if ( quint64( c.firstPatch ) + c.patchCount > np )
			return fail( QString( "cell %1: patch range %2+%3 runs past %4 patches" ).arg( ci ).arg( c.firstPatch ).arg( c.patchCount ).arg( np ) );
		if ( quint64( c.firstChart ) + c.chartCount > nc )
			return fail( QString( "cell %1: chart range %2+%3 runs past %4 charts" ).arg( ci ).arg( c.firstChart ).arg( c.chartCount ).arg( nc ) );
		quint32 terr = ( c.flags & LODP_CELL_TERRAIN ) ? quint32( G * G ) : 0U;
		if ( c.patchCount < terr )
			return fail( QString( "cell %1: terrain flag set but only %2 patches (the grid is %3)" ).arg( ci ).arg( c.patchCount ).arg( terr ) );
		for ( quint32 k = 0; k < c.patchCount; k++ ) {
			const LodpPatch & p = s.patches[c.firstPatch + k];
			cellOfPatch[c.firstPatch + k] = quint32( ci );
			bool isTerr = p.kind == LODP_KIND_TERRAIN;
			if ( ( k < terr ) != isTerr )
				return fail( QString( "cell %1 patch %2: kind %3 where the terrain grid says %4" )
					.arg( ci ).arg( c.firstPatch + k ).arg( p.kind ).arg( k < terr ? "terrain" : "not terrain" ) );
			if ( isTerr && p.lodSource != k )
				return fail( QString( "cell %1 patch %2: terrain lodSource %3, not its grid index %4" ).arg( ci ).arg( c.firstPatch + k ).arg( p.lodSource ).arg( k ) );
			if ( !isTerr && ( p.lodSource < c.firstChart || p.lodSource >= c.firstChart + c.chartCount ) )
				return fail( QString( "cell %1 patch %2: lodSource chart %3 is not one of the cell's charts %4+%5" )
					.arg( ci ).arg( c.firstPatch + k ).arg( p.lodSource ).arg( c.firstChart ).arg( c.chartCount ) );
		}
		for ( quint32 k = 0; k < c.chartCount; k++ )
			cellOfChart[c.firstChart + k] = quint32( ci );
		nextPatch += c.patchCount;
		nextChart += c.chartCount;
	}
	if ( nextPatch != np )
		return fail( QString( "the cells hold %1 patches, the table %2" ).arg( nextPatch ).arg( np ) );
	if ( nextChart != nc )
		return fail( QString( "the cells hold %1 charts, the table %2" ).arg( nextChart ).arg( nc ) );

	// instances
	std::vector<quint32> instOfChart( nc, LODP_NONE );
	for ( quint32 i = 0; i < ni; i++ ) {
		const LodpInstance & r = s.instances[i];
		if ( r.reserved )
			return fail( QString( "instance %1: reserved word is %2, not 0" ).arg( i ).arg( r.reserved ) );
		if ( i && !( s.instances[i - 1].lodiIndex < r.lodiIndex ) )
			return fail( QString( "instance table is not sorted by lodiIndex ascending at row %1" ).arg( i ) );
		if ( r.chartCount == 0 )
			return fail( QString( "instance %1 owns no chart" ).arg( i ) );
		if ( quint64( r.firstChart ) + r.chartCount > nc )
			return fail( QString( "instance %1: chart range %2+%3 runs past %4 charts" ).arg( i ).arg( r.firstChart ).arg( r.chartCount ).arg( nc ) );
		quint32 cell0 = cellOfChart[r.firstChart];
		for ( quint32 k = 0; k < r.chartCount; k++ ) {
			quint32 ch = r.firstChart + k;
			if ( instOfChart[ch] != LODP_NONE )
				return fail( QString( "chart %1 is claimed by instances %2 and %3" ).arg( ch ).arg( instOfChart[ch] ).arg( i ) );
			instOfChart[ch] = i;
			if ( cellOfChart[ch] != cell0 )
				return fail( QString( "instance %1: its charts span cells %2 and %3" ).arg( i ).arg( cell0 ).arg( cellOfChart[ch] ) );
			const LodpChart & c = s.charts[ch];
			bool last = k + 1 == r.chartCount;
			if ( c.kind == LODP_CHART_KIND_TREE ) {
				if ( !last )
					return fail( QString( "instance %1: tree chart %2 is not the instance's last chart" ).arg( i ).arg( ch ) );
				if ( c.meshChart != LODP_CHART_FOLIAGE )
					return fail( QString( "chart %1: a tree chart must carry id 0x%2, not 0x%3" ).arg( ch ).arg( LODP_CHART_FOLIAGE, 0, 16 ).arg( c.meshChart, 0, 16 ) );
			} else if ( c.meshChart != k ) {
				return fail( QString( "chart %1: mesh chart id %2 is not its place %3 in instance %4 (the draw side indexes firstChart + id)" )
					.arg( ch ).arg( c.meshChart ).arg( k ).arg( i ) );
			}
		}
	}
	for ( quint32 ch = 0; ch < nc; ch++ )
		if ( instOfChart[ch] == LODP_NONE )
			return fail( QString( "chart %1 belongs to no instance" ).arg( ch ) );

	// charts and slots
	quint32 nextSlot = 0;
	std::vector<quint32> slotRefs( np, 0 );
	for ( quint32 ch = 0; ch < nc; ch++ ) {
		const LodpChart & c = s.charts[ch];
		if ( c.reserved )
			return fail( QString( "chart %1: reserved word is %2, not 0" ).arg( ch ).arg( c.reserved ) );
		if ( c.instance != instOfChart[ch] )
			return fail( QString( "chart %1: names instance %2, but instance %3 owns it" ).arg( ch ).arg( c.instance ).arg( instOfChart[ch] ) );
		if ( c.kind != LODP_CHART_KIND_SURFACE && c.kind != LODP_CHART_KIND_TREE )
			return fail( QString( "chart %1: kind %2 is neither surface nor tree" ).arg( ch ).arg( c.kind ) );
		if ( c.kind == LODP_CHART_KIND_SURFACE && c.meshChart > LODP_CHART_MAX )
			return fail( QString( "chart %1: surface chart id %2 is above %3" ).arg( ch ).arg( c.meshChart ).arg( LODP_CHART_MAX ) );
		if ( !c.nu || !c.nv || !c.nw )
			return fail( QString( "chart %1: slot grid %2 x %3 x %4 is empty" ).arg( ch ).arg( c.nu ).arg( c.nv ).arg( c.nw ) );
		if ( c.kind == LODP_CHART_KIND_TREE && ( c.nu != 1 || c.nv != 1 || c.nw != 1 ) )
			return fail( QString( "chart %1: a tree chart is one slot, not %2 x %3 x %4" ).arg( ch ).arg( c.nu ).arg( c.nv ).arg( c.nw ) );
		if ( !( c.step > 0.0f ) || !finite( c.step ) )
			return fail( QString( "chart %1: step %2 is not positive" ).arg( ch ).arg( c.step ) );
		float lu = 0, ln = 0, dun = 0;
		for ( int k = 0; k < 3; k++ ) {
			if ( !finite( c.origin[k] ) || !finite( c.axisU[k] ) || !finite( c.axisN[k] ) )
				return fail( QString( "chart %1: frame is not finite" ).arg( ch ) );
			lu += c.axisU[k] * c.axisU[k];
			ln += c.axisN[k] * c.axisN[k];
			dun += c.axisU[k] * c.axisN[k];
		}
		if ( std::fabs( lu - 1.0f ) > 1e-4f || std::fabs( ln - 1.0f ) > 1e-4f || std::fabs( dun ) > 1e-4f )
			return fail( QString( "chart %1: axes are not orthonormal (|U|^2 %2, |N|^2 %3, U.N %4)" ).arg( ch ).arg( lu ).arg( ln ).arg( dun ) );
		if ( c.firstSlot != nextSlot )
			return fail( QString( "chart %1: first slot %2, but the charts before it end at %3" ).arg( ch ).arg( c.firstSlot ).arg( nextSlot ) );
		quint64 n = quint64( c.nu ) * c.nv * c.nw;
		if ( quint64( c.firstSlot ) + n > ns )
			return fail( QString( "chart %1: slot range %2+%3 runs past %4 slots" ).arg( ch ).arg( c.firstSlot ).arg( n ).arg( ns ) );
		quint32 held = 0;
		for ( quint64 k = 0; k < n; k++ ) {
			quint32 p = s.slotIds[c.firstSlot + k];
			if ( p == LODP_NONE )
				continue;
			if ( p >= np )
				return fail( QString( "chart %1 slot %2: patch %3 is past %4 patches" ).arg( ch ).arg( k ).arg( p ).arg( np ) );
			if ( s.patches[p].lodSource != ch || s.patches[p].kind == LODP_KIND_TERRAIN )
				return fail( QString( "chart %1 slot %2: patch %3 belongs to chart %4, not this one" ).arg( ch ).arg( k ).arg( p ).arg( s.patches[p].lodSource ) );
			slotRefs[p]++;
			held++;
		}
		if ( !held )
			return fail( QString( "chart %1: no slot holds a patch" ).arg( ch ) );
		if ( c.fallbackPatch >= np || s.patches[c.fallbackPatch].lodSource != ch || s.patches[c.fallbackPatch].kind == LODP_KIND_TERRAIN )
			return fail( QString( "chart %1: fallback patch %2 is not one of its own" ).arg( ch ).arg( c.fallbackPatch ) );
		bool tree = c.kind == LODP_CHART_KIND_TREE;
		for ( quint64 k = 0; k < n; k++ ) {
			quint32 p = s.slotIds[c.firstSlot + k];
			if ( p != LODP_NONE && ( s.patches[p].kind == LODP_KIND_TREE ) != tree )
				return fail( QString( "chart %1: patch %2 is kind %3 on a %4 chart" ).arg( ch ).arg( p ).arg( s.patches[p].kind ).arg( tree ? "tree" : "surface" ) );
		}
		nextSlot += quint32( n );
	}
	if ( nextSlot != ns )
		return fail( QString( "the charts hold %1 slots, the table %2" ).arg( nextSlot ).arg( ns ) );

	// patches
	const quint32 nl = quint32( s.links.size() );
	for ( quint32 p = 0; p < np; p++ ) {
		const LodpPatch & q = s.patches[p];
		if ( q.reserved )
			return fail( QString( "patch %1: reserved word is %2, not 0" ).arg( p ).arg( q.reserved ) );
		if ( q.kind == LODP_KIND_WATER )
			return fail( QString( "patch %1: kind water is reserved (section 1: water takes no patches)" ).arg( p ) );
		if ( q.kind > LODP_KIND_TREE )
			return fail( QString( "patch %1: kind %2 is unknown" ).arg( p ).arg( q.kind ) );
		if ( q.flags & ~LODP_PATCH_FLAGS_KNOWN )
			return fail( QString( "patch %1: flags 0x%2 carry unknown bits" ).arg( p ).arg( q.flags, 0, 16 ) );
		if ( ( q.flags & LODP_PATCH_SUBMERGED ) && q.kind != LODP_KIND_TERRAIN )
			return fail( QString( "patch %1: only terrain is flagged submerged" ).arg( p ) );
		float a = lodpFloatFromHalf( q.area );
		if ( !( a > 0.0f ) || !finite( a ) )
			return fail( QString( "patch %1: area %2 is not positive" ).arg( p ).arg( a ) );
		if ( quint64( q.firstLink ) + q.linkCount > nl )
			return fail( QString( "patch %1: link range %2+%3 runs past %4 links" ).arg( p ).arg( q.firstLink ).arg( q.linkCount ).arg( nl ) );
		if ( q.kind != LODP_KIND_TERRAIN && slotRefs[p] != 1 )
			return fail( QString( "patch %1: %2 slots name it, not exactly one" ).arg( p ).arg( slotRefs[p] ) );
	}
	return true;
}

QByteArray serialise( const LodpSector & s, LodpHeader * hOut )
{
	const quint64 ni = s.instances.size(), nc = s.charts.size(), ns = s.slotIds.size(),
		np = s.patches.size(), nl = s.links.size();
	Layout L = layoutOf( ni, nc, ns, np, nl );
	QByteArray b( qsizetype( L.end ), '\0' );
	auto blob = [&b]( quint64 at, const void * p, size_t n ) { if ( n ) std::memcpy( b.data() + at, p, n ); };
	blob( L.oCell, s.cells.data(), s.cells.size() * sizeof( LodpCell ) );
	blob( L.oInst, s.instances.data(), ni * sizeof( LodpInstance ) );
	blob( L.oChart, s.charts.data(), nc * sizeof( LodpChart ) );
	blob( L.oSlot, s.slotIds.data(), ns * 4 );
	blob( L.oPatch, s.patches.data(), np * sizeof( LodpPatch ) );
	blob( L.oLink, s.links.data(), nl * sizeof( LodpLink ) );
	quint32 icrc = lodvCrc32( reinterpret_cast<const unsigned char *>( b.constData() ) + L.oCell, qsizetype( L.end - L.oCell ) );
	put<quint32>( b, H_MAGIC, LODP_MAGIC );
	put<quint32>( b, H_VERSION, LODP_VERSION );
	put<quint32>( b, H_FLAGS, s.flags );
	put<quint64>( b, H_WS, s.worldspaceHash );
	put<quint64>( b, H_INPUT, s.lodInputHash );
	put<quint64>( b, H_LODO, s.lodoIdentity );
	put<quint64>( b, H_OPTIONS, s.optionsHash );
	put<qint32>( b, H_SX, s.sectorX );
	put<qint32>( b, H_SY, s.sectorY );
	put<float>( b, H_PATCH, s.patchUnits );
	put<quint16>( b, H_AZ, quint16( LODP_AZIMUTHS ) );
	put<quint8>( b, H_GRID, s.terrainGrid );
	put<quint32>( b, H_NCELL, quint32( LODP_SECTOR_CELL_COUNT ) );
	put<quint32>( b, H_NINST, quint32( ni ) );
	put<quint32>( b, H_NCHART, quint32( nc ) );
	put<quint32>( b, H_NSLOT, quint32( ns ) );
	put<quint32>( b, H_NPATCH, quint32( np ) );
	put<quint32>( b, H_NLINK, quint32( nl ) );
	put<quint32>( b, H_ICRC, icrc );
	put<quint64>( b, H_OCELL, L.oCell );
	put<quint64>( b, H_OINST, L.oInst );
	put<quint64>( b, H_OCHART, L.oChart );
	put<quint64>( b, H_OSLOT, L.oSlot );
	put<quint64>( b, H_OPATCH, L.oPatch );
	put<quint64>( b, H_OLINK, L.oLink );
	put<quint64>( b, H_FILE, L.end );
	quint32 hcrc = lodvCrc32( reinterpret_cast<const unsigned char *>( b.constData() ) + 0x10, LODP_HEADER_BYTES - 0x10 );
	put<quint32>( b, H_HCRC, hcrc );
	if ( hOut ) {
		hOut->version = LODP_VERSION;
		hOut->flags = s.flags;
		hOut->headerCrc32 = hcrc;
		hOut->indexCrc32 = icrc;
		hOut->fileBytes = L.end;
	}
	return b;
}

bool parse( const QByteArray & b, LodpHeader * hOut, LodpSector * out, QString * why )
{
	auto fail = [why]( const QString & m ) { if ( why ) *why = m; return false; };
	if ( b.size() < qsizetype( LODP_HEADER_BYTES ) )
		return fail( QString( "file is %1 bytes, shorter than the 256-byte header" ).arg( b.size() ) );
	quint32 magic = get<quint32>( b, H_MAGIC );
	if ( magic != LODP_MAGIC ) {
		QByteArray m = b.left( 4 );
		for ( const char * other : { "LODO", "LODI", "LODT", "LDTX", "LODM", "DDS " } )
			if ( m == QByteArray( other ) )
				return fail( QString( "magic says %1, not LODP: this is another LOD file" ).arg( other ) );
		return fail( QString( "magic 0x%1 is not LODP" ).arg( magic, 8, 16, QChar( '0' ) ) );
	}
	quint32 ver = get<quint32>( b, H_VERSION );
	if ( ver != LODP_VERSION )
		return fail( QString( "version %1 is not one this reader knows (1)" ).arg( ver ) );
	quint32 hcrc = lodvCrc32( reinterpret_cast<const unsigned char *>( b.constData() ) + 0x10, LODP_HEADER_BYTES - 0x10 );
	if ( hcrc != get<quint32>( b, H_HCRC ) )
		return fail( QString( "headerCrc32 0x%1 does not match the header (0x%2)" )
			.arg( get<quint32>( b, H_HCRC ), 8, 16, QChar( '0' ) ).arg( hcrc, 8, 16, QChar( '0' ) ) );
	if ( get<quint8>( b, H_RES0 ) )
		return fail( "reserved header byte at 0x3f is not 0" );
	if ( get<quint32>( b, H_RES1 ) )
		return fail( "reserved header word at 0x5c is not 0" );
	for ( int k = H_RESERVED; k < int( LODP_HEADER_BYTES ); k++ )
		if ( b[k] )
			return fail( QString( "reserved header byte at 0x%1 is not 0" ).arg( k, 0, 16 ) );
	if ( get<quint16>( b, H_AZ ) != LODP_AZIMUTHS )
		return fail( QString( "azimuth count %1 is not 16 (section 1)" ).arg( get<quint16>( b, H_AZ ) ) );
	if ( get<quint32>( b, H_NCELL ) != quint32( LODP_SECTOR_CELL_COUNT ) )
		return fail( QString( "cell count %1 is not 256" ).arg( get<quint32>( b, H_NCELL ) ) );
	quint64 ni = get<quint32>( b, H_NINST ), nc = get<quint32>( b, H_NCHART ), ns = get<quint32>( b, H_NSLOT ),
		np = get<quint32>( b, H_NPATCH ), nl = get<quint32>( b, H_NLINK );
	Layout L = layoutOf( ni, nc, ns, np, nl );
	const struct { int at; quint64 want; const char * name; } offs[] = {
		{ H_OCELL, L.oCell, "cell table" }, { H_OINST, L.oInst, "instance table" }, { H_OCHART, L.oChart, "chart table" },
		{ H_OSLOT, L.oSlot, "slot table" }, { H_OPATCH, L.oPatch, "patch table" }, { H_OLINK, L.oLink, "link table" },
		{ H_FILE, L.end, "fileBytes" } };
	for ( const auto & o : offs )
		if ( get<quint64>( b, o.at ) != o.want )
			return fail( QString( "%1 at %2, the layout puts it at %3" ).arg( o.name ).arg( get<quint64>( b, o.at ) ).arg( o.want ) );
	if ( quint64( b.size() ) != L.end )
		return fail( QString( "file is %1 bytes, the header says %2" ).arg( b.size() ).arg( L.end ) );
	quint32 icrc = lodvCrc32( reinterpret_cast<const unsigned char *>( b.constData() ) + L.oCell, qsizetype( L.end - L.oCell ) );
	if ( icrc != get<quint32>( b, H_ICRC ) )
		return fail( QString( "indexCrc32 0x%1 does not match the tables (0x%2)" )
			.arg( get<quint32>( b, H_ICRC ), 8, 16, QChar( '0' ) ).arg( icrc, 8, 16, QChar( '0' ) ) );
	LodpSector s;
	s.flags = get<quint32>( b, H_FLAGS );
	s.worldspaceHash = get<quint64>( b, H_WS );
	s.lodInputHash = get<quint64>( b, H_INPUT );
	s.lodoIdentity = get<quint64>( b, H_LODO );
	s.optionsHash = get<quint64>( b, H_OPTIONS );
	s.sectorX = get<qint32>( b, H_SX );
	s.sectorY = get<qint32>( b, H_SY );
	s.patchUnits = get<float>( b, H_PATCH );
	s.terrainGrid = get<quint8>( b, H_GRID );
	auto take = [&b]( auto & v, quint64 at, quint64 n ) {
		v.resize( n );
		if ( n )
			std::memcpy( v.data(), b.constData() + at, n * sizeof( v[0] ) );
	};
	take( s.cells, L.oCell, LODP_SECTOR_CELL_COUNT );
	take( s.instances, L.oInst, ni );
	take( s.charts, L.oChart, nc );
	take( s.slotIds, L.oSlot, ns );
	take( s.patches, L.oPatch, np );
	take( s.links, L.oLink, nl );
	if ( !checkSector( s, why ) )
		return false;
	if ( s.optionsHash != lodpOptionsHash( s.patchUnits, s.terrainGrid ) )
		return fail( "optionsHash does not match the patch size and terrain grid the header states" );
	if ( hOut ) {
		hOut->version = ver;
		hOut->flags = s.flags;
		hOut->headerCrc32 = hcrc;
		hOut->indexCrc32 = icrc;
		hOut->fileBytes = L.end;
	}
	if ( out )
		*out = std::move( s );
	return true;
}

} // namespace

bool lodpWrite( const QString & path, const LodpSector & s, LodpHeader * headerOut, QString * error )
{
	QString why;
	if ( s.optionsHash != lodpOptionsHash( s.patchUnits, s.terrainGrid ) ) {
		if ( error )
			*error = "lodpWrite: optionsHash does not match the patch size and terrain grid";
		return false;
	}
	if ( !checkSector( s, &why ) ) {
		if ( error )
			*error = "lodpWrite refused: " + why;
		return false;
	}
	QByteArray b = serialise( s, headerOut );
	// the writer reads its own bytes back through the reader's rules before they reach disk
	if ( !parse( b, nullptr, nullptr, &why ) ) {
		if ( error )
			*error = "lodpWrite: own bytes refused: " + why;
		return false;
	}
	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) || f.write( b ) != b.size() ) {
		if ( error )
			*error = "lodpWrite: cannot write " + path;
		return false;
	}
	return true;
}

bool lodpRead( const QString & path, LodpHeader * header, LodpSector * s, QString * error )
{
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) ) {
		if ( error )
			*error = "cannot open " + path;
		return false;
	}
	QByteArray b = f.readAll();
	QString why;
	if ( !parse( b, header, s, &why ) ) {
		if ( error )
			*error = why;
		return false;
	}
	return true;
}

QStringList lodpDescribe( const LodpHeader & h, const LodpSector & s )
{
	QStringList o;
	o << QString( "lodp.version %1" ).arg( h.version )
	  << QString( "lodp.flags %1" ).arg( h.flags )
	  << QString( "lodp.bound %1" ).arg( ( h.flags & LODP_FLAG_BOUND ) ? 1 : 0 )
	  << QString( "lodp.sector %1 %2" ).arg( s.sectorX ).arg( s.sectorY )
	  << QString( "lodp.patchUnits %1" ).arg( double( s.patchUnits ) )
	  << QString( "lodp.terrainGrid %1" ).arg( s.terrainGrid )
	  << QString( "lodp.instances %1" ).arg( s.instances.size() )
	  << QString( "lodp.charts %1" ).arg( s.charts.size() )
	  << QString( "lodp.slots %1" ).arg( s.slotIds.size() )
	  << QString( "lodp.patches %1" ).arg( s.patches.size() )
	  << QString( "lodp.links %1" ).arg( s.links.size() )
	  << QString( "lodp.fileBytes %1" ).arg( h.fileBytes )
	  << QString( "lodp.headerCrc32 0x%1" ).arg( h.headerCrc32, 8, 16, QChar( '0' ) )
	  << QString( "lodp.indexCrc32 0x%1" ).arg( h.indexCrc32, 8, 16, QChar( '0' ) )
	  << QString( "lodp.lodoIdentity 0x%1" ).arg( s.lodoIdentity, 16, 16, QChar( '0' ) )
	  << QString( "lodp.lodInputHash 0x%1" ).arg( s.lodInputHash, 16, 16, QChar( '0' ) );
	quint32 kinds[4] = { 0, 0, 0, 0 }, sub = 0;
	for ( const LodpPatch & p : s.patches ) {
		if ( p.kind < 4 )
			kinds[p.kind]++;
		if ( p.flags & LODP_PATCH_SUBMERGED )
			sub++;
	}
	o << QString( "lodp.patches.terrain %1" ).arg( kinds[0] )
	  << QString( "lodp.patches.object %1" ).arg( kinds[1] )
	  << QString( "lodp.patches.tree %1" ).arg( kinds[3] )
	  << QString( "lodp.patches.submerged %1" ).arg( sub );
	return o;
}
