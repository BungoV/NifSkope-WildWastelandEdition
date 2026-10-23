#version 410 core

// lane GAPLOOK1 (C7): the far tier's depth reduction (FO4CS FarDepthMinCS.hlsl). Each coarse texel keeps the NEAREST
// surface of its divisor x divisor block (a min, so a thin trunk still occludes), with that texel's normal.

uniform sampler2D gbuf;		// RGBA32F: view normal, linear depth (1e6 = none)
uniform ivec2 fullSize;
uniform int divisor;

out vec4 fragOut;

void main()
{
	ivec2 base = ivec2( floor( gl_FragCoord.xy ) ) * divisor;
	vec4 best = vec4( 0.0, 0.0, 1.0, 1e6 );
	for ( int j = 0; j < 8; j++ ) {
		if ( j >= divisor )
			break;
		for ( int i = 0; i < 8; i++ ) {
			if ( i >= divisor )
				break;
			ivec2 q = base + ivec2( i, j );
			if ( any( greaterThanEqual( q, fullSize ) ) )
				continue;
			vec4 t = texelFetch( gbuf, q, 0 );
			if ( t.a < best.a )
				best = t;
		}
	}
	fragOut = best;
}
