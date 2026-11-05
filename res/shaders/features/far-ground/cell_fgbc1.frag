#version 410 core

/* lane FARPAGES1: BC1 (DXT1) on the GPU, one fragment a 4x4 block of fgSrc's level fgSrcLevel. Out: x = colour0 |
 * colour1 << 16 (RGB565, colour0 > colour1: the 4-colour mode), y = 2-bit indices, texel (x, y) at bit 2 (4 y + x).
 * Endpoints: the block's box, its diagonal picked by the covariance signs against green, inset by 1/16 (van Waveren). */

uniform sampler2D fgSrc;
uniform int fgSrcLevel;
uniform int fgBc1Red;			// test only: 1 swaps the endpoints without the indices (a broken encoder)

out uvec2 fragBlock;

uint to565( vec3 c )
{
	uvec3 q = uvec3( floor( clamp( c, 0.0, 1.0 ) * vec3( 31.0, 63.0, 31.0 ) + 0.5 ) );
	return ( q.r << 11 ) | ( q.g << 5 ) | q.b;
}

vec3 from565( uint v )
{
	return vec3( float( ( v >> 11 ) & 31u ) / 31.0, float( ( v >> 5 ) & 63u ) / 63.0, float( v & 31u ) / 31.0 );
}

void main()
{
	ivec2 b = ivec2( gl_FragCoord.xy ) * 4;
	vec3 px[16];
	vec3 lo = vec3( 1.0 ), hi = vec3( 0.0 );
	for ( int i = 0; i < 16; i++ ) {
		px[i] = texelFetch( fgSrc, b + ivec2( i & 3, i >> 2 ), fgSrcLevel ).rgb;
		lo = min( lo, px[i] );
		hi = max( hi, px[i] );
	}
	vec3 mid = ( lo + hi ) * 0.5;
	float crg = 0.0, cbg = 0.0;
	for ( int i = 0; i < 16; i++ ) {
		vec3 t = px[i] - mid;
		crg += t.r * t.g;
		cbg += t.b * t.g;
	}
	vec3 inset = ( hi - lo ) / 16.0;
	lo += inset;
	hi -= inset;
	vec3 e0 = hi, e1 = lo;
	if ( crg < 0.0 ) {
		e0.r = lo.r;
		e1.r = hi.r;
	}
	if ( cbg < 0.0 ) {
		e0.b = lo.b;
		e1.b = hi.b;
	}
	uint c0 = to565( e0 ), c1 = to565( e1 );
	if ( c0 < c1 ) {
		uint t = c0;
		c0 = c1;
		c1 = t;
	}
	uint bits = 0u;
	if ( c0 != c1 ) {
		// the 4 colours lie on one line, so the nearest by projection is the nearest (no 4-way search)
		vec3 p0 = from565( c0 ), p1 = from565( c1 );
		vec3 ax = p0 - p1;
		float inv = 3.0 / dot( ax, ax );
		for ( int i = 0; i < 16; i++ ) {
			int q = int( clamp( dot( px[i] - p1, ax ) * inv + 0.5, 0.0, 3.0 ) );	// 0 = p1 .. 3 = p0
			uint idx = q == 0 ? 1u : ( q == 1 ? 3u : ( q == 2 ? 2u : 0u ) );
			bits |= idx << uint( 2 * i );
		}
	}
	if ( fgBc1Red != 0 && c0 != c1 ) {
		uint t = c0;
		c0 = c1;
		c1 = t;
	}
	fragBlock = uvec2( c0 | ( c1 << 16 ), bits );
}
