#version 410 core

/* lane PERF1: the GI chain (cellGiSampleFull) once per 2 x 2 pixels, at the pixel 2h of lane AO1's shared opaque
 * g-buffer (view normal, linear depth in game units; 1e6 empty). Out: the sample and its key (world position, packed
 * normal; w < 0 no surface), which the surfaces' cellGiSample upsamples (cell_lights.glsl). */

#define WW_CELL_FX 1
#include "lookdev_fog.glsl"
#include "ww_sunshadow.glsl"
#include "cell_lights.glsl"

uniform sampler2D giGbuf;
uniform ivec2 giFull;		// the g-buffer's size (the viewport's)
uniform vec2 giProj;		// the projection's P00, P11

layout ( location = 0 ) out vec4 giOut;
layout ( location = 1 ) out vec4 giKey;

void main()
{
	ivec2 f = min( 2 * ivec2( gl_FragCoord.xy ), giFull - 1 );
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
	giOut = cellGiSampleFull( P, N );
	giKey = vec4( P, cellGiKeyPack( N ) );
}
