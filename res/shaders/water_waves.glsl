// lane WATERWAVES1 (src/gl/cellwater.h): a slow calm-water swell, normal only, sized by the body it sits on.
// Four sine components (Tessendorf 2001, "Simulating Ocean Water", deep-water dispersion w = sqrt( g k )), read from
// WATERDATA1's baked body table (class + area) and shore plane (water_info.glsl); the game's three noise layers stay.
uniform int waterWavesOn;		// 1 while the Water swell row is on
uniform int waterWavesRed;		// 1 flat (every body the same lake size), 2 noshore (no shore fade), 4 alias (no footprint fade)
uniform vec4 waterWavesClass[3];	// sea, river, lake: body length / wavelength, wavelength min, max (u), total steepness a k
uniform vec4 waterWavesK;		// travel direction x, y (unit), the world clock in s, amplitude factor (the weather's wind)
uniform vec4 waterWavesShore;	// waterline u, fade length / wavelength, area ramp start, end (texels)

// the components: wavelength share, steepness share, turn off the wind (radians: 0, +23, -31, +47 deg), phase
const vec4 kSwellLen = vec4( 1.0, 0.71, 0.53, 0.37 );
const vec4 kSwellShare = vec4( 0.40, 0.28, 0.20, 0.12 );
const vec4 kSwellTurn = vec4( 0.0, 0.401426, -0.541052, 0.820305 );
const vec4 kSwellPhase = vec4( 0.0, 1.7, 4.1, 2.9 );
const float kSwellGravity = 686.6;	// 9.81 m/s^2 in game units (1 u = 0.0142875 m)

/* (slope x, slope y, weight, wavelength) at world xy P, clock t, footprint (world units a pixel). The slopes are
 * -dh/dx, -dh/dy and add to a normal's xy / z. The body and its shore distance come from the 4 texels around P
 * (bilinear like WATERSHORE1's field); a dry or other-body texel counts distance 0, so the swell dies at every shore
 * and at every body-to-body seam. No baked body under P (placed or interior water, the lookup off): weight 0. */
vec4 wwWaterSwell( vec2 P, float t, float footprint )
{
	float tex = waterInfoGrid.z;
	vec2 g = ( P - waterInfoGrid.xy ) / tex - 0.5;
	vec2 t0 = floor( g );
	vec2 f = g - t0;
	vec2 c0 = waterInfoGrid.xy + ( t0 + 0.5 ) * tex;
	uint w0 = wwWaterInfoWord( c0 );
	uint w1 = wwWaterInfoWord( c0 + vec2( tex, 0.0 ) );
	uint w2 = wwWaterInfoWord( c0 + vec2( 0.0, tex ) );
	uint w3 = wwWaterInfoWord( c0 + vec2( tex, tex ) );
	vec4 bw = vec4( ( 1.0 - f.x ) * ( 1.0 - f.y ), f.x * ( 1.0 - f.y ), ( 1.0 - f.x ) * f.y, f.x * f.y );
	uvec4 id = uvec4( w0, w1, w2, w3 ) & 0xFFFFu;
	// the body: the wet corner of the largest weight (first in order on a tie)
	uint body = 0u;
	float best = -1.0;
	for ( int k = 0; k < 4; k++ )
		if ( id[k] != 0u && bw[k] > best ) {
			best = bw[k];
			body = id[k];
		}
	if ( body == 0u )
		return vec4( 0.0 );
	vec4 steps = vec4( uvec4( w0, w1, w2, w3 ) >> 16 & 0xFFu );
	vec4 same = vec4( equal( id, uvec4( body ) ) );
	float dist = dot( bw, same * steps ) * waterInfoGrid.w;
	vec4 row = texelFetch( waterInfoBodies, ivec2( int( body % 256u ), int( body / 256u ) ), 0 );
	int cls = clamp( int( row.g + 0.5 ), 0, 2 );
	float area = row.a;
	if ( ( waterWavesRed & 1 ) != 0 ) {
		cls = 2;
		area = 4096.0;
	}
	vec4 C = waterWavesClass[cls];
	float lam = clamp( sqrt( area ) * tex / C.x, C.y, C.z );
	float wt = ( int( row.b + 0.5 ) & 16 ) != 0 && ( waterWavesRed & 1 ) == 0 ? 0.0
		: smoothstep( 0.0, 1.0, clamp( ( area - waterWavesShore.z ) / ( waterWavesShore.w - waterWavesShore.z ), 0.0, 1.0 ) );
	if ( ( waterWavesRed & 2 ) == 0 )
		wt *= smoothstep( 0.0, 1.0, clamp( ( dist - waterWavesShore.x ) / ( waterWavesShore.y * lam ), 0.0, 1.0 ) );
	if ( wt <= 0.0 )
		return vec4( 0.0, 0.0, 0.0, lam );
	float S = C.w * waterWavesK.w;
	vec2 grad = vec2( 0.0 );
	for ( int k = 0; k < 4; k++ ) {
		float li = lam * kSwellLen[k];
		float kk = 6.2831853 / li;
		float om = sqrt( kSwellGravity * kk );
		float ca = cos( kSwellTurn[k] ), sa = sin( kSwellTurn[k] );
		vec2 d = vec2( ca * waterWavesK.x - sa * waterWavesK.y, sa * waterWavesK.x + ca * waterWavesK.y );
		// below 4 pixels a wavelength the component fades out, gone at 2 (Nyquist): no shimmer far away
		float aa = ( waterWavesRed & 4 ) != 0 ? 1.0 : clamp( ( li / max( footprint, 1e-3 ) - 2.0 ) * 0.5, 0.0, 1.0 );
		float th = kk * dot( d, P ) - om * t + kSwellPhase[k];
		grad += ( S * kSwellShare[k] * aa * cos( th ) ) * d;
	}
	return vec4( -grad * wt, wt, lam );
}
