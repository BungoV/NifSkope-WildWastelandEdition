/* lane FIX1101A (water SSR on by default, src/features/reflections/cellssrwater.h): the water ray pass reflects the view ray off the FLAT
 * surface (no normal stack runs there: the pass cost the whole front half of this program, ~1.1 ms at 1080p), and the
 * colour pass bends the read by its own normal: the read moves by the difference of the two reflected directions'
 * vanishing points on the screen (exact for far hits, the banks and the skyline; a near hit is moved too far, so the
 * shift is capped at 8 % of the screen). Red WW_CELL_SSR_WATER_RED=1: the ray from the full normal, no bend. */
// (after water_refl.glsl and cell_lights.glsl: cellWorldDir, projectionMatrix)
vec2 wwWaterSsrBend( vec3 Rw, vec3 Rflat )
{
	vec3 ax = cellWorldDir( vec3( 1.0, 0.0, 0.0 ) ), ay = cellWorldDir( vec3( 0.0, 1.0, 0.0 ) ), az = cellWorldDir( vec3( 0.0, 0.0, 1.0 ) );
	vec3 a = vec3( dot( Rw, ax ), dot( Rw, ay ), dot( Rw, az ) );
	vec3 b = vec3( dot( Rflat, ax ), dot( Rflat, ay ), dot( Rflat, az ) );
	float za = max( -a.z, 0.05 ), zb = max( -b.z, 0.05 );
	// the read is indexed by the WATER pixel, and a mirror flips: to meet a hit moved by (dx, dy) on the screen the read
	// moves (dx, -dy) (the calm image of a far bank mirrors about the horizon). 0.8 x and the cap: a near hit's shift is
	// smaller than its direction's (tests/features/reflections/waterssr/ssr_gate.py: error 0.09 of the stripe contrast vs 0.15 unbent)
	vec2 d = 0.8 * vec2( 0.5 * projectionMatrix[0][0] * ( a.x / za - b.x / zb ), 0.5 * projectionMatrix[1][1] * ( a.y / za - b.y / zb ) );
	float l = length( d );
	return l > 0.08 ? d * ( 0.08 / l ) : d;
}

