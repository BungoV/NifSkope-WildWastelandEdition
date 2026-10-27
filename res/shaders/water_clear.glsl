/* lane WATERLOOK1 step 8 (WATERCOLOR1, src/gl/cellwater.h): the water's own colour, in place of the WATR's depth ramp.
 * Only fo4_waterlook.frag compiles this (WW_WATER_CLEAR, after water_depth.glsl); the Water clear colour row.
 * Per body class (sea / river / lake, the baked .lodl body table; placed water = lake), coefficients in 1/m at
 * 650 / 550 / 450 nm: absorption a = pure water (Pope & Fry 1997) + the class's dissolved and particle share,
 * backscatter bb = half of pure seawater's scattering (Morel 1974) + the class's particles; k = a + bb.
 *   light in     just below the surface: the sun (light 0) through the surface by its refracted cosine mu_s, and
 *                the water's sky (half the horizon, half the upper row) as diffuse light, mean cosine 0.8
 *   bed          the drawn bed (already lit in air) dimmed by the light's way down to it, then by the way up the
 *                view ray: bed x Td(vert) x exp( -k along ); no bed drawn = the class's bed albedo lit the same way
 *   in-scatter   single scattering along the view ray, closed form per light: sigma E (1 - exp( -(k + K mu_v) L ))
 *                / (k + K mu_v), K = k / mu the light's own attenuation with depth, mu_v = vert / along; sigma = 0.75
 *                bb so deep water seen straight down returns Gordon's reflectance 0.33 bb / k of the light
 * So the clear sea is turquoise over sand at 1-3 m (red goes first), blue past about 10 m, the change spread over
 * metres; deep water is a lit blue that follows the sun and the sky, not black. Red WW_CELL_WATER_CLEAR_RED:
 * noscatter (no in-scatter), nodown (the bed not dimmed on the way down), fast (every coefficient x 5). */

uniform bool waterClearOn;	// the Water clear colour row
uniform int waterClearRed;	// 1 noscatter, 2 nodown, 3 fast

const vec3 kWwClearBw = vec3( 0.0004, 0.00095, 0.00225 );	// half of pure seawater's scattering (Morel 1974), 1/m
// per class 0 sea, 1 river, 2 lake: the extra absorption, the particles' backscatter, the bed's albedo where none is drawn
const vec3 kWwClearA[3] = vec3[3]( vec3( 0.002, 0.004, 0.012 ), vec3( 0.06, 0.20, 0.70 ), vec3( 0.02, 0.08, 0.28 ) );
const vec3 kWwClearBb[3] = vec3[3]( vec3( 0.0006 ), vec3( 0.020, 0.018, 0.016 ), vec3( 0.004 ) );
const vec3 kWwClearBed[3] = vec3[3]( vec3( 0.55, 0.48, 0.36 ), vec3( 0.20, 0.18, 0.13 ), vec3( 0.22, 0.20, 0.15 ) );

struct WwClearLight
{
	int cls;	// 0 sea, 1 river, 2 lake
	vec3 sun;	// the sun's colour (linear), 0 when below the horizon
	float sunCos;	// the sun's height over the surface (cos from the zenith, in air)
	vec3 sky;	// the sky's diffuse light
	float viewMu;	// |V.z| of the view ray (used where the path is not known)
};
WwClearLight wwClearIn = WwClearLight( 2, vec3( 0.0 ), 0.0, vec3( 0.0 ), 1.0 );

void wwWaterClearSetup( vec3 P, vec3 V, vec3 sun, vec3 Ldir, vec3 skyLo, vec3 skyHi )
{
	float c = wwWaterInfo( P.xy ).w;
	wwClearIn.cls = c < -0.5 ? 2 : clamp( int( c + 0.5 ), 0, 2 );
	wwClearIn.sunCos = clamp( Ldir.z, 0.0, 1.0 );
	wwClearIn.sun = sun;
	wwClearIn.sky = 0.5 * ( skyLo + skyHi );
	wwClearIn.viewMu = max( abs( V.z ), 1e-3 );
}

vec3 wwClearK( int cls, out vec3 bb )
{
	bb = kWwClearBw + kWwClearBb[cls];
	vec3 k = kWwPureWater + kWwClearA[cls] + bb;
	if ( waterClearRed == 3 ) {
		k *= 5.0;
		bb *= 5.0;
	}
	return k;
}

// the drawn bed's radiance c (lit in air) seen through `along` u of water, `vert` u under the surface (1e9 = no bed)
vec3 wwWaterClear( vec3 c, float along, float vert )
{
	const float muD = 0.8;
	vec3 bb;
	vec3 k = wwClearK( wwClearIn.cls, bb );
	float s = wwClearIn.sunCos;
	float muS = sqrt( 1.0 - ( 1.0 - s * s ) / ( 1.333 * 1.333 ) );	// the sun's cosine under the surface
	vec3 Es = wwClearIn.sun * s * 0.98, Ed = wwClearIn.sky * 0.94;	// through the surface (Fresnel ~2% / 6%)
	bool bed = along < 1e8 && vert < 1e8;
	float L = bed ? along * kWwUnitMetres : 1e6;
	float z = bed ? vert * kWwUnitMetres : 1e6;
	float muV = bed ? clamp( vert / max( along, 1e-3 ), 1e-3, 1.0 ) : wwClearIn.viewMu;
	vec3 Ks = k / muS, Kd = k / muD;
	vec3 inS = vec3( 0.0 );
	if ( waterClearRed != 1 ) {
		vec3 sigma = 0.75 * bb;
		vec3 gs = k + Ks * muV, gd = k + Kd * muV;
		inS = sigma * ( Es * ( 1.0 - exp( -gs * L ) ) / gs + Ed * ( 1.0 - exp( -gd * L ) ) / gd );
	}
	if ( !bed )
		return inS;
	vec3 E = Es + Ed;
	vec3 Td = waterClearRed == 2 ? vec3( 1.0 )
		: ( Es * exp( -Ks * z ) + Ed * exp( -Kd * z ) ) / max( E, vec3( 1e-6 ) );
	return c * Td * exp( -k * L ) + inS;
}

// the class's bed albedo lit in air (the bed where the frame draws none)
vec3 wwWaterClearBed()
{
	return kWwClearBed[wwClearIn.cls] * ( wwClearIn.sun * wwClearIn.sunCos + wwClearIn.sky );
}
