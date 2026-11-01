#version 410 core

/* lane PERFFIX1: the vanilla bloom's blur on the GPU. The CPU twin is celllights.cpp wwCellImageSpaceSetBloom (the red
 * WW_CELL_BLOOM_GPU_RED=1, and the numpy gate notes/perffix1/bloom): the game's radius-7 blur, 15 taps
 * exp( -2 x^2 / 49 ) normalized (bloomW, from the CPU so both sides use the same floats), clamped at the edges. Pass 1
 * (bloomVertical 1) reads the measure's quarter-size level (bloomLod 2: rgb = the mean light over the texel, a
 * non-finite value read as 0), takes the bright pass scale x max( 0, c - threshold ) per tap and sums down the
 * column; pass 2 sums its output across the row. */

uniform sampler2D bloomSrc;
uniform int bloomLod;
uniform int bloomVertical;
uniform vec2 bloomPass;	// threshold, scale (the imagespace HDR data's 3rd and 4th floats)
uniform float bloomW[15];

out vec4 fragColor;

float finiteOr0( float v )
{
	return isnan( v ) || isinf( v ) ? 0.0 : v;
}

void main()
{
	ivec2 p = ivec2( gl_FragCoord.xy );
	ivec2 sz = textureSize( bloomSrc, bloomLod );
	vec3 a = vec3( 0.0 );
	for ( int k = -7; k <= 7; k++ ) {
		ivec2 q = bloomVertical == 1 ? ivec2( p.x, clamp( p.y + k, 0, sz.y - 1 ) ) : ivec2( clamp( p.x + k, 0, sz.x - 1 ), p.y );
		vec3 v = texelFetch( bloomSrc, q, bloomLod ).rgb;
		if ( bloomVertical == 1 )
			v = bloomPass.y * max( vec3( 0.0 ), vec3( finiteOr0( v.r ), finiteOr0( v.g ), finiteOr0( v.b ) ) - bloomPass.x );
		a += bloomW[k + 7] * v;
	}
	fragColor = vec4( a, 1.0 );
}
