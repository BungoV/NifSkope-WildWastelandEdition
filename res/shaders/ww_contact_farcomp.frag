#version 410 core

// lane GAPLOOK1 (C7): the far tier's resolve (FO4CS FarComposeCS.hlsl). The coarse mask comes back to the mask's
// resolution through a depth-aware bilinear upsample (a coarse texel counts only if its view depth is within
// `tolerance` x this pixel's own); the blend (GL_MIN) keeps the more occluded of the two tiers. Where no coarse
// texel agrees the output is 1: the near mask is left as it was.

uniform sampler2D gbuf;		// the full-resolution opaque pass (.a linear depth)
uniform sampler2D farDepth;	// the coarse min depth (.a)
uniform sampler2D farMask;	// the coarse march
uniform ivec2 fullSize;
uniform ivec2 coarseSize;
uniform float maskScale;
uniform int divisor;
uniform float tolerance;

out float fragMask;

void main()
{
	fragMask = 1.0;
	vec2 full = gl_FragCoord.xy * maskScale;
	ivec2 fq = clamp( ivec2( floor( full ) ), ivec2( 0 ), fullSize - 1 );
	float fine = texelFetch( gbuf, fq, 0 ).a;
	if ( fine >= 9.0e5 || fine <= 0.0 )
		return;
	vec2 cp = full / float( divisor ) - 0.5;
	ivec2 base = ivec2( floor( cp ) );
	vec2 fr = cp - vec2( base );
	float wsum = 0.0, acc = 0.0;
	for ( int j = 0; j < 2; j++ ) {
		for ( int i = 0; i < 2; i++ ) {
			ivec2 c = base + ivec2( i, j );
			if ( any( lessThan( c, ivec2( 0 ) ) ) || any( greaterThanEqual( c, coarseSize ) ) )
				continue;
			float bw = ( i == 0 ? 1.0 - fr.x : fr.x ) * ( j == 0 ? 1.0 - fr.y : fr.y );
			float cd = texelFetch( farDepth, c, 0 ).a;
			if ( bw <= 0.0 || cd >= 9.0e5 )
				continue;
			float w = bw * clamp( 1.0 - abs( fine - cd ) / fine / tolerance, 0.0, 1.0 );
			if ( w <= 0.0 )
				continue;
			wsum += w;
			acc += w * texelFetch( farMask, c, 0 ).r;
		}
	}
	if ( wsum > 1e-4 )
		fragMask = acc / wsum;
}
