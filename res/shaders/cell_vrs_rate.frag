#version 410 core

/* lane VRS1 (src/gl/cellvrs.h): one fragment per shading-rate tile. The method of AMD FidelityFX-VariableShading
 * (MIT, GPUOpen): the largest step between neighbouring pixels of the finished picture across the tile (and its
 * one-pixel border), horizontal and vertical apart; an axis whose steps all stay under the allowance shades coarse
 * along it. The step is per channel (max), so a colour-only edge stays 1x1. Fast camera motion (the temporal AA's
 * vectors, in pixels a frame) raises the allowance, as FidelityFX's motion factor does; a still camera reads 0. */

uniform sampler2D vrsSrc;	// this frame's picture, resolved, display values
uniform sampler2D vrsMv;	// previous minus current, texture units (read only when vrsMvOn)
uniform int vrsMvOn;
uniform vec2 vrsSize;
uniform vec2 vrsTile;
uniform float vrsCut;		// the opaque pass's allowance
uniform float vrsCutSorted;	// the sorted pass's (stricter, never past 2x2)
uniform int vrsMax;		// 2 or 4
uniform float vrsMotion;

layout ( location = 0 ) out uint rateOpaque;
layout ( location = 1 ) out uint rateSorted;

// palette indices (cellvrs.cpp): 0 1x1, 1 1x2, 2 2x1, 3 2x2, 4 2x4, 5 4x2, 6 4x4 (width x height)
uint pick( vec2 d, float cut, int maxRate )
{
	int rx = d.x < cut ? 2 : 1;
	int ry = d.y < cut ? 2 : 1;
	if ( maxRate >= 4 ) {
		if ( d.x < cut * 0.25 && rx == 2 ) rx = 4;
		if ( d.y < cut * 0.25 && ry == 2 ) ry = 4;
	}
	if ( rx == 4 && ry == 1 ) rx = 2;	// no 4x1 / 1x4 in the hardware's set
	if ( ry == 4 && rx == 1 ) ry = 2;
	if ( rx == 1 ) return ry == 1 ? 0u : 1u;
	if ( ry == 1 ) return 2u;
	if ( rx == 2 ) return ry == 2 ? 3u : 4u;
	return ry == 2 ? 5u : 6u;
}

void main()
{
	ivec2 size = ivec2( vrsSize );
	ivec2 tile = ivec2( vrsTile );
	ivec2 o = ivec2( gl_FragCoord.xy ) * tile;
	ivec2 hi = size - 1;
	vec2 d = vec2( 0.0 );
	for ( int y = -1; y < tile.y; y++ ) {
		for ( int x = -1; x < tile.x; x++ ) {
			ivec2 p = clamp( o + ivec2( x, y ), ivec2( 0 ), hi );
			vec3 c = texelFetch( vrsSrc, p, 0 ).rgb;
			vec3 r = texelFetch( vrsSrc, clamp( p + ivec2( 1, 0 ), ivec2( 0 ), hi ), 0 ).rgb;
			vec3 u = texelFetch( vrsSrc, clamp( p + ivec2( 0, 1 ), ivec2( 0 ), hi ), 0 ).rgb;
			vec3 dx = abs( c - r ), dy = abs( c - u );
			d = max( d, vec2( max( dx.r, max( dx.g, dx.b ) ), max( dy.r, max( dy.g, dy.b ) ) ) );
		}
	}
	float k = 1.0;
	if ( vrsMvOn != 0 ) {
		ivec2 m = clamp( o + tile / 2, ivec2( 0 ), hi );
		float v = length( texelFetch( vrsMv, m, 0 ).xy * vrsSize );
		k = clamp( 1.0 + vrsMotion * max( v - 1.0, 0.0 ), 1.0, 4.0 );	// under a pixel a frame: none
	}
	rateOpaque = pick( d, vrsCut * k, vrsMax );
	rateSorted = pick( d, vrsCutSorted * k, 2 );
}
