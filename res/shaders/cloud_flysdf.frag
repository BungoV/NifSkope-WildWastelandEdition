#version 410 core

/* lane CLOUD4: the fly-through grid's distance field, one separable pass a draw (src/gl/cloudfly.cpp). The Chebyshev
 * distance in cells to the nearest occupied cell, capped at kCap: pass x reads the occupancy, passes y and z the
 * previous pass (d = min over k of max(|k|, src(k))). Cells off the grid count empty. Twin: cloudflymath.h chebyshev. */

uniform sampler3D cfSrc;
uniform int cfAxis;	// 0 x, 1 y, 2 z
uniform int cfLayer;
uniform vec3 cfDim;	// cells x, y, z (whole numbers)

layout ( location = 0 ) out vec4 cfOut;

const int kCap = 15;

void main()
{
	ivec3 c = ivec3( ivec2( gl_FragCoord.xy ), cfLayer );
	ivec3 ax = ivec3( cfAxis == 0 ? 1 : 0, cfAxis == 1 ? 1 : 0, cfAxis == 2 ? 1 : 0 );
	ivec3 dim = ivec3( cfDim + 0.5 );
	int n = cfAxis == 0 ? dim.x : cfAxis == 1 ? dim.y : dim.z;
	int at = cfAxis == 0 ? c.x : cfAxis == 1 ? c.y : c.z;
	int best = kCap;
	for ( int k = -kCap; k <= kCap; k++ ) {
		if ( at + k < 0 || at + k >= n )
			continue;
		float v = texelFetch( cfSrc, c + ax * k, 0 ).r;
		int d = cfAxis == 0 ? ( v > 0.5 ? abs( k ) : kCap ) : max( abs( k ), int( v * 255.0 + 0.5 ) );
		best = min( best, d );
	}
	cfOut = vec4( float( best ) / 255.0 );
}
