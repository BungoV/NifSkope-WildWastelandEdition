// Lane VKCOMPUTE1 (2026-11-12): src/features/gi/rtddgi/border.comp in HLSL (the first DDGI kernel converted; hlsl_plan.md).
// The border copy. One invocation a (probe, border texel): 28 irradiance texels (8x8 tile) then 60 distance texels
// (16x16 tile), each a copy of the interior texel the octahedral fold puts beside it (rtddgimath.h borderSource).
// Texel 0 of an updated probe also counts the update (age).
// Gate: tests/hlslmatch (the GLSL and this kernel on the same buffers, byte for byte); BORDER_RED drops the age count.
#include "ddgi_common.hlsli"

[numthreads( 64, 1, 1 )]
void main( uint3 gid : SV_DispatchThreadID )
{
	const uint per = uint( 4 * DDGI_IRR_S - 4 + 4 * DDGI_DIST_S - 4 );   // 28 + 60
	const uint id = ddgiLinearId( gid );
	if ( id >= uint( P.dims.w ) * per )
		return;
	const uint p = id / per;
	const int k = int( id % per );
	const int st = ddgiProbeState( p );
	if ( st == DDGI_OFF || ( st == DDGI_ASLEEP && P.flags.y != 0 ) || ddgiSkipped( p ) )
		return;   // a covered coarse probe was not blended: no border, its age stays
	if ( k < 4 * DDGI_IRR_S - 4 ) {
		const int2 xy = ddgiBorderXY( k, DDGI_IRR_S );
		const int2 s = ddgiBorderSource( xy.x, xy.y, DDGI_IRR_S );
		const uint b = p * uint( DDGI_IRR_S * DDGI_IRR_S );
		irr[b + uint( xy.y * DDGI_IRR_S + xy.x )] = irr[b + uint( s.y * DDGI_IRR_S + s.x )];
#ifndef BORDER_RED
		if ( k == 0 )
			age[p] = age[p] + 1u;
#endif
	} else {
		const int2 xy = ddgiBorderXY( k - ( 4 * DDGI_IRR_S - 4 ), DDGI_DIST_S );
		const int2 s = ddgiBorderSource( xy.x, xy.y, DDGI_DIST_S );
		const uint b = p * uint( DDGI_DIST_S * DDGI_DIST_S );
		dist[b + uint( xy.y * DDGI_DIST_S + xy.x )] = dist[b + uint( s.y * DDGI_DIST_S + s.x )];
	}
}
