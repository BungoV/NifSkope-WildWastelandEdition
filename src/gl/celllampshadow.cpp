#include "celllampshadow.h"

#include "gl/glshape.h"
#include "wwswitch.h"	// fix TDFIX2: a live set re-reads the switches
#include "gl/celllooks.h"	// lane OPENCELLFIX2: the Shadows master

#include <QSettings>

#include <algorithm>
#include <cmath>

namespace {

struct LsState
{
	bool loaded = false;
	bool pinned = false;
	int tier = -1;
	int red = 0;
};

int tierFromText( const QByteArray & t )
{
	const QByteArray v = t.trimmed().toLower();
	if ( v == "low" || v == "1" )
		return 0;
	if ( v == "medium" || v == "2" )
		return 1;
	if ( v == "high" || v == "3" )
		return 2;
	if ( v == "ultra" || v == "4" )
		return 3;
	return -1;
}

LsState & st()
{
	static LsState s;
	static quint64 liveGen = 0;	// fix TDFIX2: a live set of WW_CELL_LAMPSHADOW* re-reads them
	if ( !s.loaded || wwSwitchMovedFor( liveGen, { "WW_CELL_LAMPSHADOW" } ) ) {
		s.loaded = true;
		liveGen = wwSwitchGen();
		const QByteArray pin = qgetenv( "WW_CELL_LAMPSHADOW" );
		s.pinned = !pin.trimmed().isEmpty();
		s.tier = s.pinned ? tierFromText( pin )
			: std::clamp( QSettings().value( QStringLiteral( "WW/CellLampShadow" ), -1 ).toInt(), -1, 3 );
		const QByteArray red = qgetenv( "WW_CELL_LAMPSHADOW_RED" ).trimmed();
		s.red = red == "live" ? 1 : red == "cube" ? 2 : 0;
	}
	return s;
}

// Moller-Trumbore; t of the hit along d, or -1
float rayTri( const Vector3 & o, const Vector3 & d, const Vector3 & a, const Vector3 & b, const Vector3 & c, Vector3 & n )
{
	const Vector3 e1 = b - a, e2 = c - a;
	const Vector3 p = Vector3::crossproduct( d, e2 );
	const float det = Vector3::dotproduct( e1, p );
	if ( std::fabs( det ) < 1e-8f )
		return -1.0f;
	const float inv = 1.0f / det;
	const Vector3 s = o - a;
	const float u = Vector3::dotproduct( s, p ) * inv;
	if ( u < 0.0f || u > 1.0f )
		return -1.0f;
	const Vector3 q = Vector3::crossproduct( s, e1 );
	const float v = Vector3::dotproduct( d, q ) * inv;
	if ( v < 0.0f || u + v > 1.0f )
		return -1.0f;
	n = Vector3::crossproduct( e1, e2 );
	return Vector3::dotproduct( e2, q ) * inv;
}

}	// namespace

int wwCellLampShadowTier()
{
	const LsState & s = st();
	return s.pinned || wwGroupLets( WwGroup::Shadows ) ? s.tier : -1;	// lane OPENCELLFIX2: the Shadows master
}

void wwCellLampShadowSetTier( int tier )
{
	LsState & s = st();
	if ( s.pinned )
		return;
	s.tier = std::clamp( tier, -1, 3 );
	QSettings().setValue( QStringLiteral( "WW/CellLampShadow" ), s.tier );
}

bool wwCellLampShadowPinned()
{
	return st().pinned;
}

int wwCellLampShadowFace()
{
	static const int face[4] = { 256, 512, 768, 1024 };
	const int t = st().tier;
	return t < 0 ? 512 : face[t];
}

int wwCellLampShadowBudgetMB()
{
	static const int mb[4] = { 32, 128, 256, 384 };
	const int t = st().tier;
	return t < 0 ? 0 : mb[t];
}

int wwCellLampShadowRed()
{
	return st().red;
}

QString wwCellLampShadowTierName( int tier )
{
	static const char * n[5] = { "off", "low", "medium", "high", "ultra" };
	return QString::fromLatin1( n[std::clamp( tier, -1, 3 ) + 1] );
}

WwLampWall wwLampFindWall( const float pos[3], const std::vector<const Shape *> & casters )
{
	constexpr float kReach = 64.0f;		// a wall lamp sits this near its wall, or it is not one
	constexpr float kOff[2] = { 24.0f, 64.0f };	// rays beside the lamp's own backplate
	constexpr float kTol = 1.0f;		// one plane: every hit within a unit of the others
	const Vector3 p( pos[0], pos[1], pos[2] );
	// the casters' world triangles near the lamp, once
	std::vector<Vector3> tri;
	for ( const Shape * sh : casters ) {
		const BoundSphere b = sh->bounds();
		const float reach = b.radius + kReach + kOff[1] * 1.5f;
		if ( ( b.center - p ).squaredLength() > reach * reach )
			continue;
		const Transform & w = sh->worldTrans();
		for ( const Triangle & t : sh->triangles ) {
			if ( t[0] >= sh->verts.size() || t[1] >= sh->verts.size() || t[2] >= sh->verts.size() )
				continue;
			tri.push_back( w * sh->verts.at( t[0] ) );
			tri.push_back( w * sh->verts.at( t[1] ) );
			tri.push_back( w * sh->verts.at( t[2] ) );
		}
	}
	WwLampWall best;
	if ( tri.empty() )
		return best;
	for ( int face = 0; face < 6; face++ ) {
		const int ax = face / 2;
		Vector3 d( 0, 0, 0 );
		d[ax] = ( face & 1 ) ? -1.0f : 1.0f;
		const int a1 = ( ax + 1 ) % 3, a2 = ( ax + 2 ) % 3;
		float lo = 1e30f, hi = -1.0f;
		bool all = true;
		for ( float off : kOff ) {
			for ( int k = 0; k < 4 && all; k++ ) {
				Vector3 o = p;
				o[k < 2 ? a1 : a2] += ( k & 1 ) ? -off : off;
				float nearest = kReach + 1.0f;
				Vector3 nn;
				for ( size_t i = 0; i + 2 < tri.size(); i += 3 ) {
					Vector3 n;
					const float t = rayTri( o, d, tri[i], tri[i + 1], tri[i + 2], n );
					if ( t > 0.0f && t < nearest ) {
						nearest = t;
						nn = n;
					}
				}
				const float nl = nn.length();
				if ( nearest > kReach || nl <= 0.0f || std::fabs( nn[ax] ) / nl < 0.995f ) {
					all = false;
					break;
				}
				lo = std::min( lo, nearest );
				hi = std::max( hi, nearest );
			}
		}
		if ( all && hi - lo <= kTol && ( best.back < 0 || lo < best.dist ) ) {
			best.back = face;
			best.dist = lo;
		}
	}
	return best;
}
