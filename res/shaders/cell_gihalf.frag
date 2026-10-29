#version 410 core

/* lane PERF1: the screen GI, once per giStep x giStep pixels (2 half resolution, 1 full), at the pixel giStep x h of
 * lane AO1's shared opaque g-buffer (view normal, linear depth in game units; 1e6 empty). The volume read
 * (cellGiVolume) in the pixel's volume: the shapes' mask (cell_givol.frag), else the room grid. Out: the sample and its
 * key (world position, packed normal; w < 0 no surface), which the surfaces' cellGiSample upsamples (cell_lights.glsl).
 * Red WW_CELL_GI_VIS=march: the old chain (the pass does not run then; it still compiles). */

#define WW_CELL_FX 1
#include "lookdev_fog.glsl"
#include "ww_sunshadow.glsl"
#include "cell_lights.glsl"

uniform sampler2D giGbuf;
uniform ivec2 giFull;		// the g-buffer's size (the viewport's)
uniform vec2 giProj;		// the projection's P00, P11
uniform int giStep;		// 2 half resolution, 1 full
uniform bool giVolOn;		// the mask was drawn this frame
uniform sampler2D giVolMask;	// full size: the pixel's interior volume's room + 1, 0 none

layout ( location = 0 ) out vec4 giOut;
layout ( location = 1 ) out vec4 giKey;

/* lane GISUNBLOTCH1: g-buffer pixel p's view position (main's rule); false: off the screen or no surface */
bool giViewAt( ivec2 p, out vec3 v )
{
	v = vec3( 0.0 );
	if ( any( lessThan( p, ivec2( 0 ) ) ) || any( greaterThanEqual( p, giFull ) ) )
		return false;
	vec4 q = texelFetch( giGbuf, p, 0 );
	if ( q.w >= 1.0e5 || dot( q.xyz, q.xyz ) < 0.5 )
		return false;
	float z = -q.w / length( cellRow[0].xyz );
	vec2 ndc = ( vec2( p ) + 0.5 ) / vec2( giFull ) * 2.0 - 1.0;
	v = vec3( -z * ndc / giProj, z );
	return true;
}

/* lane GISUNBLOTCH1: the face's normal at pixel f (view space, not normalized) from the depth: on each axis the
 * neighbour nearer in depth, so a pixel at an edge takes its own face; 0: no neighbour on an axis */
vec3 giFaceN( ivec2 f, vec3 c )
{
	vec3 a0, a1, b0, b1;
	bool oa0 = giViewAt( f + ivec2( 1, 0 ), a0 ), oa1 = giViewAt( f - ivec2( 1, 0 ), a1 );
	bool ob0 = giViewAt( f + ivec2( 0, 1 ), b0 ), ob1 = giViewAt( f - ivec2( 0, 1 ), b1 );
	if ( !( oa0 || oa1 ) || !( ob0 || ob1 ) )
		return vec3( 0.0 );
	vec3 dx = ( oa0 && ( !oa1 || abs( a0.z - c.z ) <= abs( a1.z - c.z ) ) ) ? a0 - c : c - a1;
	vec3 dy = ( ob0 && ( !ob1 || abs( b0.z - c.z ) <= abs( b1.z - c.z ) ) ) ? b0 - c : c - b1;
	return cross( dx, dy );
}

void main()
{
	ivec2 f = min( giStep * ivec2( gl_FragCoord.xy ), giFull - 1 );
	vec4 g = texelFetch( giGbuf, f, 0 );
	if ( !cellOn || !cellGiOn || g.w >= 1.0e5 || dot( g.xyz, g.xyz ) < 0.5 ) {
		giOut = vec4( 0.0 );
		giKey = vec4( 0.0, 0.0, 0.0, -1.0 );
		return;
	}
	float z = -g.w / length( cellRow[0].xyz );	// back to view units
	vec2 ndc = ( vec2( f ) + 0.5 ) / vec2( giFull ) * 2.0 - 1.0;
	vec3 posView = vec3( -z * ndc / giProj, z );
	vec3 P = cellWorldPos( posView );
	vec3 N = cellWorldDir( g.xyz );
	// lane GISUNBLOTCH1: the face's normal for the volumes' discrete reads (cellGiGeoN), on the shading normal's side
	vec3 nf = giFaceN( f, posView );
	cellGiGeoN = dot( nf, nf ) > 1.0e-20 ? cellWorldDir( nf ) : vec3( 0.0 );
	if ( dot( cellGiGeoN, N ) < 0.0 )
		cellGiGeoN = -cellGiGeoN;
#ifdef WW_CELL_GI_MARCH
	giOut = cellGiSampleFull( P, N );
#else
	float m = giVolOn ? texelFetch( giVolMask, f, 0 ).r : 0.0;
	giOut = cellGiVolume( P, N, m > 0.5 ? ivec2( int( m + 0.5 ) - 1, -1 ) : cellGiVolAt( P, N ) );
#endif
	giKey = vec4( P, cellGiKeyPack( N ) );
}
