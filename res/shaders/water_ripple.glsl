// lane WATERLOOK1 step 3 (src/gl/cellocean.h): ripples where anything stands in the water (rocks, posts, piers, actors).
// The wind's waves reflect off whatever pierces the surface; next to it the water carries ring trains running
// outward, as strong as the incident slopes there (a wall reflects nearly all of a short wave), fading over a few
// wavelengths. Still water makes none. The distance to the contact comes from the depth difference to the opaque scene
// behind the pixel (the soft edge's own term): vertical depth / its horizontal gradient, exact for a plane-faced
// contact (a post seen at any angle, a sloping rock face), so it needs no object list and covers everything drawn.
// Two trains, 0.3 m and 0.8 m (engineering choices: the short capillary-gravity band the eye reads as ripples), each at
// its own phase speed (w^2 = g k + sigma / rho k^3, the phase from the C++ in double). A train finer than the pixel
// fades out and its slope variance goes to the glare's roughness (LEAN).
uniform vec4 waterOceanRip;	// k of the two trains (rad / u), their phases now (rad, wrapped)

const float kRipDecay = 3.0;	// the rings fade over 3 wavelengths (e-fold)
const float kRipGain = 0.7;	// each train's slope amplitude / the incident slope rms

// d: the distance to the contact (u); dir: away from it (unit, world xy); foot: the pixel's step (u); s0: the
// incident slope rms. Returns ( slope x, slope y, the slope variance x + y the pixel cannot show ).
vec3 wwContactRipple( float d, vec2 dir, float foot, float s0 )
{
	vec3 o = vec3( 0.0 );
	for ( int i = 0; i < 2; i++ ) {
		float k = waterOceanRip[i];
		float lam = 6.2831853 / k;
		float a = kRipGain * s0 * exp( -d / ( kRipDecay * lam ) );
		float keep = 1.0 - smoothstep( 0.25, 0.5, foot / lam );	// full under a quarter wavelength a pixel, none from half
		// h = a / k sin( k d - w t ): its slope along dir (the envelope's own slope, 1 / ( 3 k lam ) of it, left out)
		o.xy += keep * a * cos( k * d - waterOceanRip[2 + i] ) * dir;
		o.z += ( 1.0 - keep * keep ) * 0.5 * a * a;
	}
	return o;
}

// The contact from the scene behind: the face the opaque point S lies on (its tangent plane from the pixel quad) meets
// the water's plane along a line; the distance from the water point P to that line, along the water. Exact for any flat
// face (a pier, a wall, a sloping bank seen at any angle), a little short beside a thin round object (its tangent
// plane). vert = the water's height over S (u, 1e9 none). Returns the distance (u, 1e9 none); dir away from the
// contact (unit, world xy), foot the pixel's step on the water (u), trust (1 a face up to 73 degrees off the ray; 0 a
// face seen edge-on, or a jump in the depth: the quad spans an object's flank and what lies behind). Every pixel of a quad must call it.
float wwContactDist( float vert, vec3 P, vec3 S, vec3 eye, out vec2 dir, out float foot, out float trust )
{
	vec3 sx = dFdx( S ), sy = dFdy( S );
	vec3 Vs = normalize( S - eye );
	float ang = max( length( dFdx( Vs ) ), length( dFdy( Vs ) ) );
	foot = max( length( dFdx( P.xy ) ), length( dFdy( P.xy ) ) );
	// a smooth face moves |S - eye| * angle / cos( incidence ) a pixel; a jump far more
	trust = 1.0 - smoothstep( 4.0, 8.0, max( length( sx ), length( sy ) ) / max( length( S - eye ) * ang, 1e-6 ) );
	vec3 n = cross( sx, sy );
	float nl = length( n );
	n /= max( nl, 1e-12 );
	float e = dot( P - S, n ), h = length( n.xy );
	// a face seen edge-on holds the ray itself, so the line it gives runs past P: an object's flank, not a contact
	trust *= smoothstep( 0.15, 0.3, abs( dot( n, Vs ) ) );
	dir = sign( e ) * n.xy / max( h, 1e-6 );
	return vert > 1e8 || nl < 1e-12 || h < 1e-3 ? 1e9 : abs( e ) / h;
}

// The rings at this pixel (s0 as above); zero away from any contact or across a silhouette.
vec3 wwContactRippleAt( float vert, vec3 P, vec3 S, vec3 eye, float s0 )
{
	vec2 dir;
	float foot, trust;
	float d = wwContactDist( vert, P, S, eye, dir, foot, trust );
	float lamMax = 6.2831853 / min( waterOceanRip.x, waterOceanRip.y );
	if ( s0 <= 0.0 || trust <= 0.0 || d > 4.0 * kRipDecay * lamMax )
		return vec3( 0.0 );
	vec3 o = wwContactRipple( d, dir, foot, s0 );
	return vec3( trust * o.xy, o.z );
}
