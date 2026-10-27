/* lane WATERLOOK1 step 14 (WATERSWASH1): the waves' run-up on the land beside baked water (the Wave run-up row).
 * Needs land_shore.glsl, water_info.glsl and water_shore.glsl first. Each wave's bore reaches the still waterline on
 * SURF1's phase (water_surf.glsl at the shore distance 0: omega t plus its along-shore noise, its wave groups), runs
 * up the beach and drains back:
 *   run-up     the 2% height of Stockdon et al. 2006, R2 = 1.1 ( 0.35 b sqrt( H0 L0 ) + 0.5 sqrt( H0 L0 ( 0.563 b^2 +
 *              0.004 ) ) ), H0 the shore's Hs (WATERFOAMDETAIL1: wind sea by fetch, plus the swell on a sea), L0 the
 *              deep wavelength of the period (SURF1's on a sea, the wind sea's on lakes and rivers), b the beach slope;
 *              a wave's own top scales with the root of its group envelope (R ~ sqrt( H ))
 *   motion     the tip rises and falls on a parabola in time (ballistic swash, Shen & Meyer 1963), up and back in one
 *              wave period
 *   look       a water film under the swash, foam at the tip on the uprush that thins as it drains (fresh water keeps
 *              less, as the shore foam), the swash zone up to R2 kept saturated
 * The beach slope is the ground's own (its normal), as in land_wet.glsl. Neutral is all zero. */
uniform bool landSwashOn;	// the Wave run-up row
uniform int landSwashRed;	// 1 still (the tip held at its top), 2 nofoam, 4 regular (no groups, no along-shore noise)
uniform vec4 landSwashK;	// sea period (s, SURF1's), waves a group (SURF1's), foam opacity, foam e-fold (m, horizontal)
uniform float landSwashTime;	// the world clock (s), as the water's

// the land distance (u) and its gradient (per u, pointing inland) from the four texels wwLandShoreSmooth reads
vec3 wwLandShoreGrad( vec2 worldXY )
{
	vec2 p = ( worldXY - landShoreGrid.xy ) / max( landShoreGrid.z, 1.0 ) - 0.5;
	ivec2 t = ivec2( floor( p ) );
	vec2 f = p - vec2( t );
	float a = float( wwLandShoreTexel( t ) ), b = float( wwLandShoreTexel( t + ivec2( 1, 0 ) ) );
	float c = float( wwLandShoreTexel( t + ivec2( 0, 1 ) ) ), d = float( wwLandShoreTexel( t + ivec2( 1, 1 ) ) );
	float q = landShoreOn ? landShoreGrid.w : 32.0;
	vec2 g = vec2( mix( b - a, d - c, f.y ), mix( c - a, d - b, f.x ) ) * q / max( landShoreGrid.z, 1.0 );
	return vec3( mix( mix( a, b, f.x ), mix( c, d, f.x ), f.y ) * q, g );
}

// Stockdon et al. 2006: the 2% run-up height (m) for Hs h0 (m), period T (s), beach slope b
float wwSwashR2( float h0, float T, float b )
{
	float hl = h0 * 9.81 * T * T / 6.2831853;
	return 1.1 * ( 0.35 * b * sqrt( hl ) + 0.5 * sqrt( hl * ( 0.563 * b * b + 0.004 ) ) );
}

/* (film, foam, wet, the wave's top in m) at world point P with the ground's world normal N, clock t (s). Call it in
 * uniform flow or not: no derivatives. */
vec4 wwLandSwash( vec3 P, vec3 N, float t )
{
	if ( !landSwashOn || !landShoreOn || !waterInfoOn )
		return vec4( 0.0 );
	vec3 dg = wwLandShoreGrad( P.xy );
	float d = dg.x;
	float nz = clamp( N.z, 0.05, 1.0 );
	float b = clamp( sqrt( 1.0 - nz * nz ) / nz, 0.005, 1.0 );
	float z = d * b * kShoreM;	// m above the still line
	if ( z > 4.0 )
		return vec4( 0.0 );	// past any run-up (Hs 6 m, 12 s on a 1:1 bank: under 4 m)
	// the water: down the land distance's gradient, one water texel past the line
	vec2 Pw = P.xy - dg.yz / max( length( dg.yz ), 1e-4 ) * ( d + waterInfoGrid.z );
	vec4 wv = wwShoreWave( Pw );
	if ( wv.w < 0.5 )
		return vec4( 0.0 );
	float hs = wv.x * kShoreM;
	bool sea = wwWaterInfo( Pw ).w < 0.5;
	float T = sea ? landSwashK.x : clamp( sqrt( 6.2831853 * hs / ( 0.04 * 9.81 ) ), 1.0, 12.0 );
	float om = 6.2831853 / T;
	float L0 = 9.81 * T * T / 6.2831853 / kShoreM;	// u
	bool regular = ( landSwashRed & 4 ) != 0;
	float phi = om * t + ( regular ? 0.0 : 3.0 * ( wwShoreNoise( Pw / ( 2.0 * L0 ) ) - 0.5 ) );
	float u = fract( phi / 6.2831853 );	// 0 as the bore reaches the line
	float env = 1.0;
	if ( !regular ) {	// SURF1's group envelope at the line, at this wave's arrival
		float psi = om / max( landSwashK.y, 1.0 ) * ( t - u * T ) + 6.2831853 * wwShoreNoise( Pw / ( 3.0 * L0 ) + 0.41 );
		env = 0.35 + 0.65 * ( 0.5 + 0.5 * cos( psi ) );
	}
	float R2 = wwSwashR2( hs, T, b );
	float R = R2 * sqrt( env );
	if ( ( landSwashRed & 1 ) != 0 )
		u = 0.5;
	float zt = R * 4.0 * u * ( 1.0 - u );	// the tip
	float edge = max( 0.1 * b, 1e-3 );	// 0.1 m along the ground
	float film = 1.0 - smoothstep( zt - edge, zt, z );
	float foam = 0.0;
	if ( ( landSwashRed & 2 ) == 0 )
		foam = film * exp( -max( zt - z, 0.0 ) / ( landSwashK.w * b ) ) * mix( 1.0, 0.2, smoothstep( 0.3, 0.7, u ) )
			* wv.y * landSwashK.z * smoothstep( 0.0, 0.05, R );
	float wet = 1.0 - smoothstep( 0.7 * R2, R2, z );
	return vec4( film * smoothstep( 0.0, 0.02, R ), foam, wet * smoothstep( 0.0, 0.02, R2 ), R );
}
