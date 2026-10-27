/* lane WATERCAUSTICS1 (src/gl/cellwater.h): the sun's caustics on the bed seen through the water.
 * Only the combined water twin compiles this (WW_WATER_CAUSTICS: fo4_waterlook); the default
 * fo4_water program is the one it always was. Applied in the water shader to the refracted scene texel (the bed),
 * the way Crest and Unity HDRP water apply theirs: only water pixels pay, and the bed is lit by the scene already.
 *   waves     four directional ripples of our own (22-57 u, 0.5-1 u high), deep-water dispersion w = sqrt( g k ),
 *             phase on the world clock (WW_CELL_WORLDANIM_TIME pins it): h, its slope and its Hessian H analytic;
 *             w rounded to whole turns per 64 s and k to whole turns per 8192 u, so the clock and the position wrap
 *             seamlessly and the float32 phase stays small anywhere on the map
 *   focus     flux through the refracted sun ray tube (GPU Gems ch.2 / Yuksel's height-field caustics, small-slope
 *             form): J = I - L c H, c = 1 - 1 / 1.333, L the sun's path down to the bed; I = 1 / |det J|, the det
 *             softened by the sun disc and depth ( e = 0.05 + 0.002 L, scaled so flat water reads 1 ), capped at 8
 *   where     the surface point the bed point's sun ray crossed (S.xy - r.xy L, r = refract( -sun, up, 1 / 1.333 ))
 *   strength  the bed's lit colour x ( 1 + share x env x ( I - 1 ) ): share = the sun's part of the light at the bed
 *             (sun x cos x shadow over sun x cos x shadow + the sky colour), env = ( 1 - smoothstep( 0, 700, d ) )
 *             x the luminance of pure water's transmittance down the sun path (wwWaterAbsorb)
 * Engineering choices: own waves (WATERWAVES1 may later feed its surface instead); the Jacobian at the crossing
 * point instead of a ray-cast mesh (no extra pass); the sky colour stands in for the bed's ambient. */

uniform float waterCauTime;	// the world clock mod kWwCauLoop, seconds (0 with the World animation row off)
uniform int waterCauRed;	// 1 nofade (env 1), 2 noshadow (the shadow ignored), 3 still (time 0), 4 flat (det without H)
uniform bool waterCauShadow;	// the sun's cascades are bound this frame (wwSunShadowWanted)
uniform bool waterCauOn;	// lane WATERCOMBINE1: the Water caustics row (fo4_waterlook carries every water look)

const float kWwCauN = 1.333;
const float kWwCauG = 686.6;	// 9.81 m/s^2 in units ( / 0.0142875 )
const float kWwCauLoop = 64.0;	// s: the pattern repeats (Tessendorf's looping ocean); C++ sends the clock mod this
const float kWwCauW0 = 6.2831853 / kWwCauLoop;
const float kWwCauTile = 8192.0;	// u: the pattern tiles (117 m); wave vectors rounded to whole turns per tile
const float kWwCauK0 = 6.2831853 / kWwCauTile;
const vec4 kWwCauWave[4] = vec4[4](	// direction (deg), wavelength (u), amplitude (u), phase
	vec4( 12.0, 22.0, 0.50, 0.0 ), vec4( 77.0, 31.0, 0.60, 1.7 ), vec4( 141.0, 41.0, 0.80, 4.1 ), vec4( 203.0, 57.0, 1.00, 2.6 ) );

float wwCauLum( vec3 c )
{
	return dot( c, vec3( 0.2126, 0.7152, 0.0722 ) );
}

// the Hessian of the ripple height at xy: ( hxx, hyy, hxy )
vec3 wwCauHessian( vec2 xy, float t )
{
	vec3 H = vec3( 0.0 );
	vec2 xw = xy - kWwCauTile * floor( xy / kWwCauTile );	// exact in float32: the phase stays small far from 0,0
	for ( int i = 0; i < 4; i++ ) {
		vec4 w = kWwCauWave[i];
		float a = radians( w.x );
		float k = 6.2831853 / w.y;
		vec2 kv = round( k * vec2( cos( a ), sin( a ) ) / kWwCauK0 ) * kWwCauK0;	// whole turns per tile
		float om = round( sqrt( kWwCauG * k ) / kWwCauW0 ) * kWwCauW0;	// a whole number of turns per loop
		float s = -w.z * sin( dot( kv, xw ) - om * t + w.w );
		H += s * vec3( kv.x * kv.x, kv.y * kv.y, kv.x * kv.y );
	}
	return H;
}

// the depth envelope: 1 at the surface, falling to 0 at 700 u, x pure water's transmittance down the sun path
float wwCauEnvelope( float d, float L )
{
	if ( waterCauRed == 1 )
		return 1.0;
	return ( 1.0 - smoothstep( 0.0, 700.0, d ) ) * wwCauLum( wwWaterAbsorb( L ) );
}

/* the factor on the bed's colour at S (world), d its vertical depth under the surface; Ldir world TO the sun, sun its
 * colour (linear), sky the sky's colour (linear), shadow the sun's visibility at S. 1 = no caustic. dbg: (I, env, share) */
float wwWaterCaustic( vec3 S, float d, vec3 Ldir, vec3 sun, vec3 sky, float shadow, out vec3 dbg )
{
	dbg = vec3( 1.0, 0.0, 0.0 );
	if ( Ldir.z <= 0.0 || d <= 0.0 )
		return 1.0;
	vec3 r = refract( -Ldir, vec3( 0.0, 0.0, 1.0 ), 1.0 / kWwCauN );
	float L = d / max( -r.z, 0.05 );
	vec2 X = S.xy - r.xy * L;
	float t = waterCauRed == 3 ? 0.0 : waterCauTime;
	vec3 H = waterCauRed == 4 ? vec3( 0.0 ) : wwCauHessian( X, t );
	float s = L * ( 1.0 - 1.0 / kWwCauN );
	float det = ( 1.0 - s * H.x ) * ( 1.0 - s * H.y ) - s * s * H.z * H.z;
	float e = 0.05 + 0.002 * L;
	float I = min( sqrt( 1.0 + e * e ) * inversesqrt( det * det + e * e ), 8.0 );	// flat water: exactly 1
	float sh = waterCauRed == 2 ? 1.0 : shadow;
	float Es = wwCauLum( sun ) * Ldir.z * sh;
	float share = Es / max( Es + wwCauLum( sky ), 1e-6 );
	float env = wwCauEnvelope( d, L );
	dbg = vec3( I, env, share );
	return max( 1.0 + share * env * ( I - 1.0 ), 0.0 );
}
