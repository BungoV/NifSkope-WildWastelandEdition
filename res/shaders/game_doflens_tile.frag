#version 410 core

/* lane DOFLENS1: the near layer's reach (src/gl/cellpost.cpp), two passes over tiles of 8 x 8 quarter texels (32 full px).
 * dilate 0: t0 = the prep (a = the signed circle in full px); out = the largest near radius (full px) in the tile.
 * dilate 1: t0 = that; out = the largest of the 3 x 3 tiles around. The near circle is at most 64 px (radius 32 =
 * 8 quarter texels), so every near disk that can reach a texel starts in its tile or a neighbour: the gather's near
 * kernel takes this radius instead of the fixed 32 px. */

uniform sampler2D t0;
uniform int dilate;

out vec4 fragColor;

void main()
{
	ivec2 q = ivec2( gl_FragCoord.xy );
	ivec2 sz = textureSize( t0, 0 );
	float m = 0.0;
	if ( dilate == 0 ) {
		for ( int j = 0; j < 8; j++ ) {
			for ( int i = 0; i < 8; i++ ) {
				ivec2 p = min( q * 8 + ivec2( i, j ), sz - 1 );
				m = max( m, -texelFetch( t0, p, 0 ).a * 0.5 );
			}
		}
	} else {
		for ( int j = -1; j <= 1; j++ ) {
			for ( int i = -1; i <= 1; i++ )
				m = max( m, texelFetch( t0, clamp( q + ivec2( i, j ), ivec2( 0 ), sz - 1 ), 0 ).r );
		}
	}
	fragColor = vec4( m, 0.0, 0.0, 1.0 );
}
