/* lane PERF1: the screen GI's upsample, read by the surfaces' cellGiSample (cell_lights.glsl). PERFFIX2 (2026-11-01):
 * one copy for both GI paths: the old PRT's (its volumes' read once a screen texel) and the ray-traced volume's
 * (cell_gihalf.frag runs cellDdgiRead once a texel of the shared opaque g-buffer; src/gl/celllights.cpp giHalfPass).
 * Included without a trailing comment on the include line (the loader refuses one). */
uniform bool cellGiHalfOn;
uniform sampler2D cellGiHalf;		// rgba: the volume read at the screen texel's pixel
uniform sampler2D cellGiHalfKey;	// xyz: that pixel's world position, w: its packed normal (< 0: no surface)
uniform vec4 cellGiHalfRect;		// the viewport's x, y; the screen target's size
uniform float cellGiHalfScale;		// 0.5 half resolution, 1 full

#if defined( WW_CELL_RTDDGI ) && !defined( WW_CELL_PRT )
/* PERFFIX2: the ray-traced volume's texel is (E x fade, fade), the read cellGiSample makes, plus the sky visibility
 * at three directions of the specular lobe for cellSpecSky: the normal, half way to the mirror direction, the mirror
 * direction (cell_gihalf.frag: one read, cellDdgiTwo / cellDdgiThree). Each rides the low 8 bits of one channel's
 * mantissa (rgb x (1 +- 2^-15) at most; E floored at 1e-20 so no channel is denormal): no new sampler, no target. */
#define CELL_GIHALF_PACKED
float cellGiHalfBits( float e, float v )
{
	uint b = floatBitsToUint( max( e, 1e-20 ) );
	return uintBitsToFloat( ( b & 0xFFFFFF00u ) | uint( floor( clamp( v, 0.0, 1.0 ) * 255.0 + 0.5 ) ) );
}
float cellGiHalfVisOf( float e )
{
	return float( floatBitsToUint( e ) & 0xFFu ) / 255.0;
}
vec3 cellGiHalfVis = vec3( 1.0 );	// the last cellGiHalfAt's visibility at the normal, half way, the mirror direction
/* the lobe's visibility at weight k (cellSpecSky's bend: D = normalize( mix( N, R, k ) ); k 0.5 = half way): the
 * quadratic through the three, exact at k 0, 0.5 and 1 (tests/ddgiscreen: against the lobe's own read) */
float cellGiHalfLobeVis( float k )
{
	vec3 v = cellGiHalfVis;
	return clamp( v.x * ( 1.0 - k ) * ( 1.0 - 2.0 * k ) + 4.0 * v.y * k * ( 1.0 - k ) + v.z * k * ( 2.0 * k - 1.0 ), 0.0, 1.0 );
}
#endif

// the screen GI's upsample: the 4 screen texels round this pixel; false: none is this surface
bool cellGiHalfAt( vec3 P, vec3 N, out vec4 s )
{
	s = vec4( 0.0 );
	vec2 u = ( gl_FragCoord.xy - cellGiHalfRect.xy - 0.5 ) * cellGiHalfScale;	// texel h sits at pixel h / scale
	ivec2 h0 = ivec2( floor( u ) );
	vec2 t = u - vec2( h0 );
	ivec2 hs = ivec2( cellGiHalfRect.zw );
	float tol = 1.0 + 0.01 * distance( P, vec3( cellRow[0].w, cellRow[1].w, cellRow[2].w ) );	// game units
	vec4 acc = vec4( 0.0 );
	float wsum = 0.0;
#ifdef CELL_GIHALF_PACKED
	vec3 vacc = vec3( 0.0 );
#endif
	for ( int k = 0; k < 4; k++ ) {
		ivec2 o = ivec2( k & 1, k >> 1 );
		float wb = ( o.x == 1 ? t.x : 1.0 - t.x ) * ( o.y == 1 ? t.y : 1.0 - t.y );
		if ( wb <= 0.0 )
			continue;
		ivec2 h = clamp( h0 + o, ivec2( 0 ), hs - 1 );
		vec4 key = texelFetch( cellGiHalfKey, h, 0 );
		if ( key.w < 0.0 )
			continue;
		float w = wb * smoothstep( 0.7, 0.95, dot( N, cellGiKeyNormal( key.w ) ) )
		        * max( 1.0 - abs( dot( N, key.xyz - P ) ) / tol, 0.0 );
		if ( w <= 0.0 )
			continue;
#ifdef CELL_GIHALF_PACKED
		vec4 g = texelFetch( cellGiHalf, h, 0 );
		acc += w * g;
		vacc += w * vec3( cellGiHalfVisOf( g.r ), cellGiHalfVisOf( g.g ), cellGiHalfVisOf( g.b ) );
#else
		acc += w * texelFetch( cellGiHalf, h, 0 );
#endif
		wsum += w;
	}
	if ( wsum < 1e-3 )
		return false;
	s = acc / wsum;
#ifdef CELL_GIHALF_PACKED
	cellGiHalfVis = vacc / wsum;
#endif
	return true;
}
