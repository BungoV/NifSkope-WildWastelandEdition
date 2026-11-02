#pragma once
// lane SKINSSS2: the half-resolution skin blur (row "Skin subsurface", option WW_CELL_SKINSSS_HALF=1, off = SKINSSS1's
// full-resolution passes). The C++ twin of cell_skinsss.frag's sssHalf path:
// FO4CS works at half resolution with a 3-tap radius; here the extract takes each 2x2 block's skin texels at the
// block's reference depth (its first texel), the two blurs run on the half image, and the composite reads the half
// result back at full resolution through a depth-aware bilinear (taps across a depth step drop out), so a face never
// takes the light of a hand in front of it. tests/skinsshalf compares it with the full-resolution twin.
// WWSKINSSSHALF_RED: the twin without the half path (the unit's red: its run is the full-resolution one).
#include "gl/wwskinsss.h"

namespace WwSkinSss {

constexpr float kHalfWorkScale = 1.0f;	// FO4CS's own work pixels: kernelRadius( kHalfWorkScale ) = 3 taps
constexpr float kUpFast = 0.98f;	// the composite's plain bilinear when all 4 depth weights reach it (cell_skinsss.frag)

inline bool halfWanted()
{
#ifdef WWSKINSSSHALF_RED
	return false;
#else
	return true;
#endif
}

inline int halfSize( int n )
{
	return ( n + 1 ) / 2;
}

// the extract of one half texel: up to four full texels (light, tag, depth); the first is the reference
inline Px extractHalf( const V3 * light, const float * tag, const float * depth, int n )
{
	float ws = 0.0f, sum[3] = { 0, 0, 0 };
	for ( int i = 0; i < n; i++ ) {
		const float w = clamp01( tag[i] ) * depthWeight( depth[0], depth[i] );
		if ( w <= 0.0f )
			continue;
		const V3 s = saturate( light[i], kSaturation );
		sum[0] += s.r * w;
		sum[1] += s.g * w;
		sum[2] += s.b * w;
		ws += w;
	}
	if ( ws <= 0.0f )
		return Px();
	return { { sum[0] / ws, sum[1] / ws, sum[2] / ws }, ws / float( n ) * clamp01( kAmount ) };
}

// the full-resolution read of the blurred half image at full pixel (x, y): bilinear over the 4 nearest half texels,
// each weighted by its depth match (half texel q's depth = the full depth at 2q) and its coverage; the receiver's
// coverage is its own tag x amount. hw, hh: the half size; depthFull: the full depth, fw wide
inline Px upsample( const Px * half, int hw, int hh, const float * depthFull, int fw, int fh, int x, int y, float tag )
{
	const float a = clamp01( tag ) * clamp01( kAmount );
	if ( a <= 0.0f )
		return Px();
	const float hx = ( float( x ) + 0.5f ) * 0.5f - 0.5f, hy = ( float( y ) + 0.5f ) * 0.5f - 0.5f;
	const int x0 = int( std::floor( hx ) ), y0 = int( std::floor( hy ) );
	const float fx = hx - float( x0 ), fy = hy - float( y0 );
	const float dc = depthFull[size_t( y ) * size_t( fw ) + size_t( x )];
	float dw[4], bw[4];
	const Px * s[4];
	bool fast = true;
	for ( int t = 0; t < 4; t++ ) {
		const int i = t & 1, j = t >> 1;
		const int qx = std::min( std::max( x0 + i, 0 ), hw - 1 ), qy = std::min( std::max( y0 + j, 0 ), hh - 1 );
		s[t] = &half[size_t( qy ) * size_t( hw ) + size_t( qx )];
		const int dx = std::min( 2 * qx, fw - 1 ), dy = std::min( 2 * qy, fh - 1 );
		dw[t] = s[t]->a > 0.0f ? depthWeight( dc, depthFull[size_t( dy ) * size_t( fw ) + size_t( dx )] ) : 0.0f;
		bw[t] = ( i ? fx : 1.0f - fx ) * ( j ? fy : 1.0f - fy );
		fast = fast && dw[t] >= kUpFast;
	}
	float sum[3] = { 0, 0, 0 }, ws = 0.0f;
	for ( int t = 0; t < 4; t++ ) {
		const float w = bw[t] * ( fast ? 1.0f : dw[t] );	// fast: every tap at this depth, the plain bilinear
		sum[0] += s[t]->rgb.r * w;
		sum[1] += s[t]->rgb.g * w;
		sum[2] += s[t]->rgb.b * w;
		ws += w;
	}
	if ( ws <= kMinCoverage )
		return Px();	// no half texel at this depth: the frame keeps its light
	return { { sum[0] / ws, sum[1] / ws, sum[2] / ws }, a };
}

}	// namespace WwSkinSss
