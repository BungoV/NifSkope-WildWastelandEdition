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
#ifdef WW_CELL_GI_MARCH
	giOut = cellGiSampleFull( P, N );
#else
	float m = giVolOn ? texelFetch( giVolMask, f, 0 ).r : 0.0;
	giOut = cellGiVolume( P, N, m > 0.5 ? ivec2( int( m + 0.5 ) - 1, -1 ) : cellGiVolAt( P, N ) );
#endif
	giKey = vec4( P, cellGiKeyPack( N ) );
}
