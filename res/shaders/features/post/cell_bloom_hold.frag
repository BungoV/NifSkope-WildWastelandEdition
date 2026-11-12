#version 410 core

/* MODULES1 R8-2: the bloom's level 0 held over the temporal jitter's period (src/features/post/cellbloom.cpp).
 * The chain reads the frame before the temporal resolve, so a bright thing under a pixel wide is hit by one jitter
 * and missed by the next: the whole halo blinks. While the camera stands still the level is the mean of its last
 * bloomHoldN frames (up to 8, the jitter's period), which is the same every frame. A moved camera counts from 1. */

uniform sampler2DArray bloomRing;	// level 0 of the last 8 frames, one a layer
uniform int bloomHoldK;				// the newest layer
uniform int bloomHoldN;				// how many layers count, 1..8

out vec4 fragColor;

void main()
{
	ivec2 p = ivec2( gl_FragCoord.xy );
	int n = clamp( bloomHoldN, 1, 8 );
	vec3 c = vec3( 0.0 );
	for ( int i = 0; i < n; i++ )
		c += texelFetch( bloomRing, ivec3( p, ( bloomHoldK - i + 8 ) & 7 ), 0 ).rgb;
	fragColor = vec4( c / float( n ), 1.0 );
}
