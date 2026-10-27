// lane WATERLOOK1 step 12 (WATERCONTACT1, src/gl/cellwater.h): foam where the water meets anything standing in it
// (rocks, posts, piers, hulls, actors). Each wave that strikes the object breaks against it and leaves a patch of foam
// that drifts outward and decays; the next wave lays a fresh one, so rings of foam pulse out from the contact once a
// wave period. The contact and its distance come from the depth behind the pixel (wwContactDist, water_ripple.glsl),
// so no object list is needed. The strength follows WATERFOAMDETAIL1's energy rule (H^2.5, less in fresh water):
// calm water gives at most a thin lap line. A bank the baked shore field already knows is left to the Shore foam row.
// Needs water_shore.glsl and water_ripple.glsl before it.
uniform bool waterContactOn;
uniform int waterContactRed;	// 1 nodepth (no contact found), 2 flat (full energy on any water), 4 still (no pulse), 8 noshorecut
uniform vec4 waterContactK;	// band width / Hs, foam life (wave periods), wind-sea steepness Hs / L, foam opacity

// the foam density 0..1 at distance d (u) from the contact: hs the significant wave height (u), pers the foam's
// persistence (1 sea, under 1 fresh water), t the clock (s), cxy the contact point (world xy), foot the pixel (u)
float wwContactLaw( float d, float hs, float pers, float t, vec2 cxy, float foot )
{
	float hm = hs * kShoreM;
	float amount = ( waterContactRed & 2 ) != 0 ? 1.0 : clamp( pow( hm / waterShoreK2.w, 2.5 ) * pers, 0.0, 1.0 );
	// the period of a wind sea of this height at its usual steepness (L = Hs / s, deep water: T = sqrt( 2 pi L / g ))
	float T = clamp( sqrt( 6.2831853 * hm / ( waterContactK.z * 9.81 ) ), 1.0, 12.0 );
	float W = clamp( waterContactK.x * hs, 0.3 / kShoreM, 6.0 / kShoreM );	// the foam band's e-fold width (u)
	float v = W / ( waterContactK.y * T );	// the foam drifts out a band width in its life (u/s)
	// born at the contact when a wave strikes (bright), thinning until the next; the strike time drifts along the object
	float ph = ( t - d / v ) / T + 2.0 * wwShoreNoise( cxy / 140.0 );
	float pulse = exp( -4.0 * fract( ph ) );
	float env = 0.35 + 0.65 * ( 0.5 + 0.5 * cos( 6.2831853 * ph / 7.0 ) );	// wave groups of 7
	const float kMean = 0.2454;	// ( 1 - e^-4 ) / 4
	if ( ( waterContactRed & 4 ) != 0 ) {
		pulse = kMean;
		env = 0.675;
	}
	// rings closer than a few pixels show their mean, a band thinner than a pixel is fainter, not wider
	pulse = mix( pulse, kMean, smoothstep( 0.25, 0.5, foot / ( v * T ) ) );
	float ring = amount * exp( -d / W ) * pulse * env * ( W / ( W + foot ) );
	// ripples run up the object about H: a thin translucent lap line (WATERFOAMDETAIL1's, from K3.w metres)
	float lap = ( 1.0 - smoothstep( 0.0, hs + foot, d ) ) * ( hs / ( hs + foot + 1e-4 ) )
		* smoothstep( 0.0, waterShoreK3.w, hm ) * waterShoreK3.z;
	return clamp( max( ring, lap ), 0.0, 1.0 );
}

// the contact foam at this pixel: vert = the water's height over the opaque point S behind it (u, 1e9 none).
// Returns (density 0..1, the contact distance u, trust, the pixel's foot u). Every pixel of a quad must call it.
vec4 wwWaterContact( float vert, vec3 P, vec3 S, vec3 eye, float t )
{
	vec2 dir;
	float foot, trust;
	float d = wwContactDist( ( waterContactRed & 1 ) != 0 ? 1e9 : vert, P, S, eye, dir, foot, trust );
	if ( !waterContactOn || trust <= 0.0 || d > 1e8 )
		return vec4( 0.0, d, trust, foot );
	vec4 wv = wwShoreWave( P.xy );
	if ( wv.w < 0.5 )
		return vec4( 0.0, d, trust, foot );
	float hs = max( wv.x, 1e-3 );
	float cut = 1.0;
	if ( ( waterContactRed & 8 ) == 0 ) {
		// a bank: the baked land lies about as far as the contact (within the bake's error); an object in open water
		// stands well inside the field's distance
		vec2 field = wwShoreField( P.xy );
		if ( field.y > 0.5 )
			cut = smoothstep( 1.5, 2.5, max( field.x - waterShoreK0.x, 0.0 ) / ( d + waterShoreK4.z + 1.0 ) );
	}
	float D = trust * cut * wwContactLaw( d, hs, wv.y, t, P.xy - d * dir, foot );
	return vec4( D, d, trust, foot );
}

// the foam's alpha from its density, broken up by WATERFOAMDETAIL1's texture (as the shore and surf foam)
float wwContactFoamAlpha( float D, vec2 P, float foot, float t )
{
	if ( D < 0.002 )
		return 0.0;
	vec3 det = wwFoamDetail( P, foot, t );
	float s = mix( 0.5, waterShoreK0.w, det.z );
	float a = smoothstep( 1.0 - D - s, 1.0 - D + s, det.x );
	return a * mix( det.y, 1.0, D * D ) * min( 1.0, 1.5 * D ) * waterContactK.w;
}
