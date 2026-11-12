#pragma once
/* MODULES1 10c: the Fusion Shaders cloud sheets, the host side (no Qt, no GL; the offline gate
 * tests/features/clouds/cloudsheets builds this file alone). Its texture rung, ported:
 *  - every weather cloud layer is a flat SHEET. The layer's dome mesh is fitted over its zenith cap: uv = J g + c with
 *    g = (x / z, y / z) the tangent of the direction, so a sheet at altitude A maps world XY to uv by J / A;
 *  - the highest dome shell sits at the deck's altitude, a lower shell at deck x (its zenith height / the highest);
 *  - only sheets ABOVE the receiver shadow it, at the point where its sun ray meets the sheet (the parallax);
 *  - the sun's disc is a penumbra: taps over an ellipse on the sheet (Vogel disc), radius = distance x tan(sun radius),
 *    stretched along the sun's azimuth by 1 / sun z;
 *  - hex tiling hides the texture's repeat: three reads at hashed offsets, blended around the texture's mean;
 *  - the layers that cast are the densest ones (cap alpha x layer alpha), ties by layer index.
 * The shader half is res/shaders/features/clouds/cloud_shadowmap.frag (csMode 3 with csSheetN > 0); the constants of
 * the mask itself stay in cloudfusionlaw.h. */

#include <algorithm>
#include <cmath>

namespace WwCloudSheetMath
{

constexpr int kMaxSheets = 8;	//!< sheets the shader walks
constexpr int kMaxTaps = 16;
constexpr float kCapMinElevationDeg = 45.0f;	//!< the zenith cap: vertices at or above this elevation
constexpr int kCapMinVertices = 6;
constexpr float kMaxRelativeFitError = 0.1f;	//!< rms / spread over the cap: above it the shell is not a sheet
constexpr float kTanSunRadius = 0.0046532f;	//!< the sun's angular radius (tan x = x to 7 digits here)
constexpr float kHexCellsPerTile = 2.0f;

enum Reject
{
	RejectNone = 0,
	RejectNoMesh,
	RejectNoCap,
	RejectNotASheet,
	RejectDegenerate
};

inline const char * rejectName( int r )
{
	static const char * n[5] = { "none", "no_mesh", "no_zenith_cap", "not_a_sheet", "degenerate" };
	return n[r < 0 || r > 4 ? 4 : r];
}

struct Fit
{
	bool valid = false;
	int reject = RejectNoMesh;
	int capVertices = 0;
	float zenithHeight = 0.0f;	//!< z of the cap's highest-elevation vertex (the shell's height)
	float capAlpha = 0.0f;	//!< the cap's mean vertex alpha
	float relErr = 0.0f;
	float uvPerTan[4] = {};	//!< row-major J
	float uvAtZenith[2] = {};
};

//! least squares uv = a gx + b gy + c over the zenith cap; pos xyz (3 floats), uv (2), rgba (4, may be null: alpha 1)
inline Fit fit( const float * pos, const float * uv, const float * rgba, int nv )
{
	Fit f;
	if ( !pos || !uv || nv <= 0 )
		return f;
	const float sinCap = std::sin( kCapMinElevationDeg * 3.14159265358979f / 180.0f );
	double sxx = 0, sxy = 0, syy = 0, sx = 0, sy = 0, n = 0;
	double sxu = 0, syu = 0, su = 0, sxv = 0, syv = 0, sv = 0, suu = 0, svv = 0, alphaSum = 0;
	float bestSin = -2.0f;
	for ( int i = 0; i < nv; i++ ) {
		const float x = pos[i * 3], y = pos[i * 3 + 1], z = pos[i * 3 + 2], u = uv[i * 2], v = uv[i * 2 + 1];
		if ( !std::isfinite( x ) || !std::isfinite( y ) || !std::isfinite( z ) || !std::isfinite( u ) || !std::isfinite( v ) )
			continue;
		const float r = std::sqrt( x * x + y * y + z * z );
		if ( !( r > 0.0f ) )
			continue;
		const float s = z / r;
		if ( s < sinCap )
			continue;
		if ( s > bestSin ) {
			bestSin = s;
			f.zenithHeight = z;
		}
		const double gx = double( x ) / z, gy = double( y ) / z;
		sxx += gx * gx;
		sxy += gx * gy;
		syy += gy * gy;
		sx += gx;
		sy += gy;
		n += 1;
		sxu += gx * u;
		syu += gy * u;
		su += u;
		sxv += gx * v;
		syv += gy * v;
		sv += v;
		suu += double( u ) * u;
		svv += double( v ) * v;
		alphaSum += rgba ? std::clamp( rgba[i * 4 + 3], 0.0f, 1.0f ) : 1.0f;
	}
	f.capVertices = int( n );
	if ( f.capVertices < kCapMinVertices ) {
		f.reject = RejectNoCap;
		return f;
	}
	f.capAlpha = float( alphaSum / n );
	const double m[3][3] = { { sxx, sxy, sx }, { sxy, syy, sy }, { sx, sy, n } };
	auto det3 = []( const double a[3][3] ) {
		return a[0][0] * ( a[1][1] * a[2][2] - a[1][2] * a[2][1] ) - a[0][1] * ( a[1][0] * a[2][2] - a[1][2] * a[2][0] )
			+ a[0][2] * ( a[1][0] * a[2][1] - a[1][1] * a[2][0] );
	};
	const double det = det3( m );
	if ( !( std::fabs( det ) > 1e-12 * std::max( 1.0, n * n * n ) ) ) {
		f.reject = RejectDegenerate;
		return f;
	}
	auto solve = [&]( double r0, double r1, double r2, double out[3] ) {
		for ( int col = 0; col < 3; col++ ) {
			double a[3][3];
			for ( int i = 0; i < 3; i++ )
				for ( int j = 0; j < 3; j++ )
					a[i][j] = m[i][j];
			a[0][col] = r0;
			a[1][col] = r1;
			a[2][col] = r2;
			out[col] = det3( a ) / det;
		}
	};
	double cu[3], cv[3];
	solve( sxu, syu, su, cu );
	solve( sxv, syv, sv, cv );
	const double rssU = suu - ( cu[0] * sxu + cu[1] * syu + cu[2] * su );
	const double rssV = svv - ( cv[0] * sxv + cv[1] * syv + cv[2] * sv );
	const double rms = std::sqrt( std::max( 0.0, ( rssU + rssV ) / n ) );
	const double varU = suu / n - ( su / n ) * ( su / n ), varV = svv / n - ( sv / n ) * ( sv / n );
	const double spread = std::sqrt( std::max( 0.0, varU + varV ) );
	if ( !( spread > 1e-9 ) ) {
		f.reject = RejectDegenerate;
		return f;
	}
	f.relErr = float( rms / spread );
	f.uvPerTan[0] = float( cu[0] );
	f.uvPerTan[1] = float( cu[1] );
	f.uvPerTan[2] = float( cv[0] );
	f.uvPerTan[3] = float( cv[1] );
	f.uvAtZenith[0] = float( cu[2] );
	f.uvAtZenith[1] = float( cv[2] );
	if ( f.relErr > kMaxRelativeFitError ) {
		f.reject = RejectNotASheet;
		return f;
	}
	if ( !( std::fabs( cu[0] * cv[1] - cu[1] * cv[0] ) > 1e-12 ) ) {
		f.reject = RejectDegenerate;
		return f;
	}
	f.valid = true;
	f.reject = RejectNone;
	return f;
}

//! a shell's sheet altitude: the highest shell at the deck, a lower one in proportion (0 = no altitude)
inline float altitude( float zenithHeight, float zenithHeightMax, float deck )
{
	if ( !std::isfinite( zenithHeight ) || !std::isfinite( zenithHeightMax ) || !std::isfinite( deck ) || !( zenithHeight > 0.0f )
		|| !( zenithHeightMax > 0.0f ) || !( deck > 0.0f ) )
		return 0.0f;
	return deck * std::min( zenithHeight / zenithHeightMax, 1.0f );
}

//! world XY -> uv of a fitted sheet at an altitude: uv = M xy + offset (M row-major, uv per world unit)
inline bool worldMap( const Fit & f, float alt, float m[4], float offset[2] )
{
	if ( !f.valid || !std::isfinite( alt ) || !( alt > 0.0f ) )
		return false;
	for ( int i = 0; i < 4; i++ )
		m[i] = f.uvPerTan[i] / alt;
	offset[0] = f.uvAtZenith[0];
	offset[1] = f.uvAtZenith[1];
	return true;
}

//! where the receiver's sun ray meets a sheet; false when the sheet is not above the receiver (it casts nothing)
inline bool hit( const float recv[3], const float toSun[3], float alt, float xy[2], float * dist = nullptr )
{
	if ( !( toSun[2] > 0.001f ) || !std::isfinite( alt ) )
		return false;
	const float dz = alt - recv[2];
	if ( !( dz > 0.0f ) )
		return false;
	const float t = dz / toSun[2];
	xy[0] = recv[0] + t * toSun[0];
	xy[1] = recv[1] + t * toSun[1];
	if ( dist )
		*dist = t;
	return true;
}

struct Penumbra
{
	float radiusAcross = 0.0f, radiusAlong = 0.0f;	//!< world units on the sheet
	float azimuth[2] = { 1.0f, 0.0f };	//!< the sun's unit azimuth
};

inline Penumbra penumbra( float distAlongRay, const float toSun[3] )
{
	Penumbra p;
	const float h = std::sqrt( toSun[0] * toSun[0] + toSun[1] * toSun[1] );
	if ( h > 1e-6f ) {
		p.azimuth[0] = toSun[0] / h;
		p.azimuth[1] = toSun[1] / h;
	}
	if ( !( distAlongRay > 0.0f ) || !( toSun[2] > 0.001f ) )
		return p;
	p.radiusAcross = distAlongRay * kTanSunRadius;
	p.radiusAlong = p.radiusAcross / toSun[2];
	return p;
}

//! tap k of n on the unit disc (Vogel: equal areas, the golden angle); one tap is the centre
inline void diskTap( int k, int n, float xy[2] )
{
	xy[0] = xy[1] = 0.0f;
	if ( n <= 1 || k < 0 || k >= n )
		return;
	const float r = std::sqrt( ( float( k ) + 0.5f ) / float( n ) );
	const float a = 2.39996323f * float( k );
	xy[0] = r * std::cos( a );
	xy[1] = r * std::sin( a );
}

inline float fract( float x )
{
	return x - std::floor( x );
}

inline void hexHash( float sx, float sy, float out[2] )
{
	sx = fract( sx * 1271.5151f );
	sy = fract( sy * 3337.8237f );
	const float d = sx * ( sy + 19.19f ) + sy * ( sx + 19.19f );
	sx += d;
	sy += d;
	out[0] = fract( ( sx + sy ) * sy );
	out[1] = fract( ( sx + sy ) * sx );
}

struct HexTaps
{
	float offset[3][2] = {};	//!< the three reads' uv offsets
	float bary[3] = {};	//!< the triangle's weights (they sum to 1)
	float weight[3] = {};	//!< bary / its length (the blend keeps the texture's contrast)
};

inline HexTaps hexTaps( float u, float v, float cells = kHexCellsPerTile )
{
	HexTaps h;
	const float px = u * cells, py = v * cells;
	const float sx = px, sy = -0.57735027f * px + 1.15470054f * py;
	const float cx = std::floor( sx ), cy = std::floor( sy );
	const float fx = sx - cx, fy = sy - cy;
	const float bz = 1.0f - fx - fy;
	float c[3][2];
	if ( bz > 0.0f ) {
		c[0][0] = cx;
		c[0][1] = cy;
		c[1][0] = cx;
		c[1][1] = cy + 1.0f;
		c[2][0] = cx + 1.0f;
		c[2][1] = cy;
		h.bary[0] = bz;
		h.bary[1] = fy;
		h.bary[2] = fx;
	} else {
		c[0][0] = cx + 1.0f;
		c[0][1] = cy + 1.0f;
		c[1][0] = cx + 1.0f;
		c[1][1] = cy;
		c[2][0] = cx;
		c[2][1] = cy + 1.0f;
		h.bary[0] = -bz;
		h.bary[1] = 1.0f - fy;
		h.bary[2] = 1.0f - fx;
	}
	for ( int i = 0; i < 3; i++ )
		hexHash( c[i][0], c[i][1], h.offset[i] );
	const float len = std::sqrt( h.bary[0] * h.bary[0] + h.bary[1] * h.bary[1] + h.bary[2] * h.bary[2] );
	for ( int i = 0; i < 3; i++ )
		h.weight[i] = h.bary[i] / std::max( len, 1e-6f );
	return h;
}

inline float hexBlend( const HexTaps & h, float s0, float s1, float s2, float mean )
{
	const float a = mean + h.weight[0] * ( s0 - mean ) + h.weight[1] * ( s1 - mean ) + h.weight[2] * ( s2 - mean );
	return std::clamp( a, 0.0f, 1.0f );
}

//! a layer's alpha under the density exponent (1 = identity)
inline float layerAlpha( float alpha, float exponent )
{
	const float a = std::clamp( alpha, 0.0f, 1.0f );
	return exponent == 1.0f ? a : std::pow( a, exponent );
}

//! the light kept by the sheets ABOVE a receiver (covers 0..1 at each sheet's hit, altitudes in the receiver's unit)
inline float keptAbove( const float * covers, const float * altitudes, int n, float receiverZ )
{
	float kept = 1.0f;
	for ( int i = 0; i < n; i++ )
		if ( altitudes[i] > receiverZ )
			kept *= 1.0f - std::clamp( covers[i], 0.0f, 1.0f );
	return kept;
}

struct Axes
{
	int layers, taps;
	bool hex;
};

//! step 1..4 (Low, Medium, High, Ultra); Medium is the model's own default
inline Axes preset( int step )
{
	static const Axes a[4] = { { 1, 1, false }, { 2, 4, true }, { 4, 4, true }, { 8, 8, true } };
	return a[std::clamp( step, 1, 4 ) - 1];
}

inline const char * presetName( int step )
{
	static const char * n[4] = { "low", "medium", "high", "ultra" };
	return n[std::clamp( step, 1, 4 ) - 1];
}

struct Candidate
{
	int layer;
	float density;
};

//! the casting sheets: densest first, ties by the lower layer index; returns the count written to out
inline int select( const Candidate * c, int n, int limit, int * out )
{
	if ( !c || !out || limit <= 0 )
		return 0;
	Candidate s[32];
	int kept = 0;
	for ( int i = 0; i < n && kept < 32; i++ )
		if ( std::isfinite( c[i].density ) && c[i].density > 0.0f )
			s[kept++] = c[i];
	std::stable_sort( s, s + kept, []( const Candidate & a, const Candidate & b ) {
		return a.density != b.density ? a.density > b.density : a.layer < b.layer;
	} );
	const int chosen = std::min( kept, std::min( limit, kMaxSheets ) );
	for ( int i = 0; i < chosen; i++ )
		out[i] = s[i].layer;
	return chosen;
}

}	// namespace WwCloudSheetMath
