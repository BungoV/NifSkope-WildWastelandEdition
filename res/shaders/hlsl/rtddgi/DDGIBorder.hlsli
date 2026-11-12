// DDGI volume, the border copy (lane VKCOMPUTE1, 2026-11-12; from src/features/gi/rtddgi/border.comp). Shared, no bindings.
// One invocation a (probe, border texel): 28 irradiance texels (8x8 tile) then 60 distance texels (16x16 tile), each a
// copy of the interior texel the octahedral fold puts beside it (rtddgimath.h borderSource). Texel 0 of an updated
// probe also counts the update (age). Dispatch: ceil(probes x 88 / 64) groups of 64.
// BORDER_RED (the gate's red) drops the age count.
#ifndef DDGI_BORDER_HLSLI
#define DDGI_BORDER_HLSLI

#include "DDGIResources.hlsli"

#define DDGI_BORDER_PER ( 4 * DDGI_IRR_S - 4 + 4 * DDGI_DIST_S - 4 )   // 28 + 60

void DDGI_BorderCS( uint id )
{
	const uint per = uint( DDGI_BORDER_PER );
	if ( id >= uint( DDGI_P.dims.w ) * per )
		return;
	const uint p = id / per;
	const int k = int( id % per );
	const int st = DDGI_ProbeState( p );
	if ( st == DDGI_OFF || ( st == DDGI_ASLEEP && DDGI_P.flags.y != 0 ) || DDGI_Skipped( DDGI_P, p ) )
		return;   // a covered coarse probe was not blended: no border, its age stays
	if ( k < 4 * DDGI_IRR_S - 4 ) {
		const int2 xy = DDGI_BorderXY( k, DDGI_IRR_S );
		const int2 s = DDGI_BorderSource( xy.x, xy.y, DDGI_IRR_S );
		const uint b = p * uint( DDGI_IRR_S * DDGI_IRR_S );
		DDGIIrradiance[b + uint( xy.y * DDGI_IRR_S + xy.x )] = DDGIIrradiance[b + uint( s.y * DDGI_IRR_S + s.x )];
#ifndef BORDER_RED
		if ( k == 0 )
			DDGIAge[p] = DDGIAge[p] + 1u;
#endif
	} else {
		const int2 xy = DDGI_BorderXY( k - ( 4 * DDGI_IRR_S - 4 ), DDGI_DIST_S );
		const int2 s = DDGI_BorderSource( xy.x, xy.y, DDGI_DIST_S );
		const uint b = p * uint( DDGI_DIST_S * DDGI_DIST_S );
		DDGIDistance[b + uint( xy.y * DDGI_DIST_S + xy.x )] = DDGIDistance[b + uint( s.y * DDGI_DIST_S + s.x )];
	}
}

#endif
