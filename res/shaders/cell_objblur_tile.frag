#version 410 core

/* lane POSTFX1: per-object motion blur, passes 2 and 3 of 4 (McGuire et al. 2012, "A Reconstruction Filter for
 * Plausible Motion Blur": tile max, then neighbour max). tileMode 0: the longest vector of the tile x tile pixels
 * under this tile; tileMode 1: the longest of the 3 x 3 tiles around this one. srcTex holds pixel vectors (RG16F). */

uniform sampler2D srcTex;
uniform int tile;
uniform int tileMode;

out vec2 vmax;

void main()
{
	ivec2 t = ivec2( gl_FragCoord.xy );
	ivec2 sz = textureSize( srcTex, 0 );
	vec2 best = vec2( 0.0 );
	float bl = 0.0;
	if ( tileMode == 0 ) {
		ivec2 o = t * tile;
		for ( int y = 0; y < tile; y++ )
			for ( int x = 0; x < tile; x++ ) {
				ivec2 q = o + ivec2( x, y );
				if ( q.x >= sz.x || q.y >= sz.y )
					continue;
				vec2 v = texelFetch( srcTex, q, 0 ).xy;
				float l = dot( v, v );
				if ( l > bl ) { bl = l; best = v; }
			}
	} else {
		for ( int y = -1; y <= 1; y++ )
			for ( int x = -1; x <= 1; x++ ) {
				ivec2 q = clamp( t + ivec2( x, y ), ivec2( 0 ), sz - 1 );
				vec2 v = texelFetch( srcTex, q, 0 ).xy;
				float l = dot( v, v );
				if ( l > bl ) { bl = l; best = v; }
			}
	}
	vmax = best;
}
