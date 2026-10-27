// lane WATERLOOK1 step 16 (WATERUNDER2, src/gl/cellunderwater.h): the Underwater light row in the camera-underwater
// pass. The water between the eye and what it sees takes the Water clear colour's coefficients for the body's class
// (absorption + backscatter per channel, water_clear.glsl), in place of the WATR's linear fog:
//   transmission  exp( -k L ) along the view (L to the surface where the ray leaves the water)
//   in-scatter    single scattering of the sun and the sky, each dimmed with depth at its own rate K = k / mu: at a
//                 point s along the ray the depth is z = zc - dir.z s, so per light
//                 sigma E exp( -K zc ) ( 1 - exp( -( k - K dir.z ) L ) ) / ( k - K dir.z )   (sigma = 0.75 bb)
//   the scene     was lit in air: dimmed by the light's way down to it (as the clear colour's bed)
// and a meniscus: where the screen crosses the surface (the near plane straddles it) a dark band a few pixels wide.
// Needs water_depth.glsl (kWwPureWater, kWwUnitMetres) and water_clear.glsl (wwClearK) before it.
uniform int uwLight;		// 1 = the Underwater light row is on
uniform int uwLightRed;		// 2 nomeniscus, 4 nocolor (the WATR fog law stays); 1 nosnell is the water program's
uniform vec4 uwSun;		// the sun's linear colour (0 below the horizon), its height (cos from the zenith)
uniform vec3 uwSky;		// the water's diffuse sky light, linear: half the horizon + half the upper row
uniform int uwClass;		// the body's class: 0 sea, 1 river, 2 lake (placed water)
uniform vec2 uwMenK;		// the meniscus: half width (px), darkening at its middle

// ( 1 - exp( -g L ) ) / g, per channel, safe at g -> 0
vec3 wwUlInt( vec3 g, float L )
{
	vec3 gl = g * L;
	return mix( ( 1.0 - exp( -gl ) ) / g, L * ( 1.0 - 0.5 * gl ), lessThan( abs( gl ), vec3( 1e-4 ) ) );
}

// the meniscus' darkening at a pixel whose near-plane point sits dz above the surface; w its half width (u)
float wwUnderMeniscus( float dz, float w )
{
	if ( uwLight == 0 || ( uwLightRed & 2 ) != 0 )
		return 0.0;
	return uwMenK.y * ( 1.0 - smoothstep( 0.0, max( w, 1e-6 ), abs( dz ) ) );
}

// frame = frame * mul + add for the camera under water (pixels whose near point is above the surface: untouched)
void wwUnderLight( vec3 eye, vec3 Pn, vec3 Pw, bool farPlane, float H, out vec3 add, out vec3 mul )
{
	add = vec3( 0.0 );
	mul = vec3( 1.0 );
	if ( Pn.z >= H )
		return;
	vec3 d = Pw - eye;
	float L = length( d );
	vec3 dir = d / max( L, 1e-6 );
	if ( farPlane )
		L = 1.0e9;
	if ( dir.z > 1e-6 )
		L = min( L, ( H - eye.z ) / dir.z );
	vec3 bb;
	vec3 k = wwClearK( uwClass, bb );
	float s = clamp( uwSun.w, 0.0, 1.0 );
	float muS = sqrt( 1.0 - ( 1.0 - s * s ) / ( 1.333 * 1.333 ) );	// the sun's cosine under the surface
	const float muD = 0.8;
	vec3 Es = uwSun.rgb * s * 0.98, Ed = uwSky * 0.94;	// through the surface (Fresnel ~2% / 6%), as the clear colour
	vec3 Ks = k / muS, Kd = k / muD;
	float Lm = min( L, 1.0e7 ) * kWwUnitMetres;
	float zc = max( H - eye.z, 0.0 ) * kWwUnitMetres;	// the eye's depth (m)
	float zv = max( H - ( eye.z + dir.z * min( L, 1.0e7 ) ), 0.0 ) * kWwUnitMetres;	// the seen point's depth (m)
	vec3 sigma = 0.75 * bb;
	add = sigma * ( Es * exp( -Ks * zc ) * wwUlInt( k - Ks * dir.z, Lm ) + Ed * exp( -Kd * zc ) * wwUlInt( k - Kd * dir.z, Lm ) );
	// the scene's light on its way down (a small diffuse floor keeps lamp-lit points at night from going black)
	vec3 Td = ( Es * exp( -Ks * zv ) + ( Ed + 1e-4 ) * exp( -Kd * zv ) ) / ( Es + Ed + 1e-4 );
	mul = Td * exp( -k * Lm );
}
