#version 410 core

/* lane DOFLENS1: the thin-lens depth of field's first pass (src/gl/cellpost.cpp). Quarter size: each texel q takes the
 * mean of the 4x4 full-size texels from 4q (four bilinear taps at the 2x2 block corners; Inf or NaN counts as 0) and
 * the signed circle of confusion (full px, game_doflens.glsl) of the NEAREST of four depths, so a near edge spreads.
 * t0 the frame, t1 the depth. */

uniform sampler2D t0;
uniform sampler2D t1;

#include "game_doflens.glsl"

out vec4 fragColor;

void main()
{
	ivec2 q = ivec2( gl_FragCoord.xy );
	vec2 fs = vec2( textureSize( t0, 0 ) );
	ivec2 hi = textureSize( t1, 0 ) - 1;
	float s1 = lensFocus( t1 );
	vec3 acc = vec3( 0.0 );
	float zn = 1e30;
	bool sky = true;
	for ( int i = 0; i < 4; i++ ) {
		ivec2 o = ivec2( ( i & 1 ) * 2 + 1, ( i >> 1 ) * 2 + 1 );
		vec2 pc = vec2( q * 4 + o );
		vec3 c = texture( t0, pc / fs ).rgb;
		bvec3 bad = equal( floatBitsToUint( c ) & 0x7f800000u, uvec3( 0x7f800000u ) );
		acc += mix( c, vec3( 0.0 ), bad ) * 0.25;
		float z = lensDist( texelFetch( t1, min( q * 4 + o, hi ), 0 ).r );
		if ( z >= 0.0 ) {
			sky = false;
			zn = min( zn, z );
		}
	}
	fragColor = vec4( acc, lensCoc( sky ? -1.0 : zn, s1 ) );
}
