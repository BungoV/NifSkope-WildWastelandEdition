/* lane WATERLOOK1 step 8 (WATERCOLOR1, src/gl/cellwater.h): the water's own colour, in place of the WATR's depth ramp.
 * Only fo4_waterlook.frag compiles this (WW_WATER_CLEAR, after water_depth.glsl); the Water clear colour row.
 * Per body class (sea / river / lake, the baked .lodl body table; placed water = lake), coefficients in 1/m at
 * 650 / 550 / 450 nm.
 *   light in     just below the surface: the sun (light 0) through the surface by its refracted cosine mu_s, and
 *                the water's sky (half the horizon, half the upper row) as diffuse light, mean cosine 0.8
 *   bed          the drawn bed (already lit in air) dimmed by the light's way down to it, then by the way up the
 *                view ray: bed x Td(vert) x exp( -k along ); no bed drawn = the class's bed albedo lit the same way
 *   in-scatter   single scattering along the view ray, closed form per light: sigma E (1 - exp( -(k + K mu_v) L ))
 *                / (k + K mu_v), K = k / mu the light's own attenuation with depth, mu_v = vert / along; sigma = 0.75
 *                bb so deep water seen straight down returns Gordon's reflectance 0.33 bb / k of the light
 * Lane FIX1031B (WATERLOOK1 overhaul, LATE ITEMs 3/5/6, notes/fix1031b/water/RESEARCH.md section 3): BOSTON's water,
 * not a clear ocean. k per class from the Secchi depth (Poole and Atkins 1929: Kd = 1.7 / Zsd at 550 nm; MWRA: the
 * harbour is turbid, Secchi under 1.8 m poor, clearer toward the bay, set by suspended solids): sea 2.5 m, lake 2.0 m,
 * river 1.2 m; non-water absorption exponential in wavelength at the non-algal particles' slope 0.011 / nm (Babin 2003;
 * Jerlov coastal 3C's algae + CDOM shape, 5.7 x at 450 nm, tinted sand saturated khaki), particle backscatter ~ 1 /
 * lambda; the deep reflectance's LUMINANCE from the same physics (R(550) 0.040 sea / river, 0.035 lake), its HUE
 * from the cell's own WATR deep colour, so a modder's record still drives the colour. So the bed shows only through
 * the first 1-2 m, then the water is the record's grey-green, lit by the frame's sun and sky; never turquoise or lime.
 * The bed where none is drawn is WET sand / silt (the dry 0.55 sand of before read beige: the teeth crop).
 * Reds WW_CELL_WATER_CLEAR_RED: noscatter (no in-scatter), nodown (the bed not dimmed on the way down), fast (every
 * coefficient x 5), old (WATERCOLOR1's clear-ocean law and bed, the hard baked/drawn bed switch, no Fresnel coupling:
 * the clear colour exactly as it shipped behind its row before FIX1031B). */

uniform bool waterClearOn;	// the Water clear colour row
uniform int waterClearRed;	// 1 noscatter, 2 nodown, 3 fast, 4 old

const vec3 kWwClearBw = vec3( 0.0004, 0.00095, 0.00225 );	// half of pure seawater's scattering (Morel 1974), 1/m
// red old (WATERCOLOR1), per class 0 sea, 1 river, 2 lake: the extra absorption, the particles' backscatter, the bed
const vec3 kWwClearA[3] = vec3[3]( vec3( 0.002, 0.004, 0.012 ), vec3( 0.06, 0.20, 0.70 ), vec3( 0.02, 0.08, 0.28 ) );
const vec3 kWwClearBb[3] = vec3[3]( vec3( 0.0006 ), vec3( 0.020, 0.018, 0.016 ), vec3( 0.004 ) );
const vec3 kWwClearBed[3] = vec3[3]( vec3( 0.55, 0.48, 0.36 ), vec3( 0.20, 0.18, 0.13 ), vec3( 0.22, 0.20, 0.15 ) );
// FIX1031B: Boston (notes/fix1031b/water/tools/boston_coef.py 0.011 real): k = a_w + a + bb (1/m), the physics' deep R.
// MEASURED values (notes/fix1031b/water/REALVALUES.md): sea k(550) = the harbour's measured Kd(PAR) 0.631 /m (MWRA
// 2018-06: mean 1 % light depth 7.3 m, 1996-2017; Secchi-equivalent 2.7 m, inside Luis et al. 2019's in-situ 1-5 m at
// station 142); river Secchi 1.0 m (EPA Clean Charles 2002 mean, Daly Field - BU Bridge); lake 2.0 m (no cited value).
// Rule: k(550) = Kd(PAR) where measured, else Poole-Atkins Kd = 1.7 / Secchi. Red WW_CELL_WATER_CLEAR_RED=secchi (5)
// = the overhaul's first constants (sea 2.5 m, river 1.2 m).
const vec3 kWwBostonK[3] = vec3[3]( vec3( 0.570, 0.631, 1.599 ), vec3( 0.992, 1.700, 4.580 ), vec3( 0.650, 0.850, 2.233 ) );
const vec3 kWwBostonR[3] = vec3[3]( vec3( 0.0372, 0.0400, 0.0195 ), vec3( 0.0578, 0.0400, 0.0182 ),
	vec3( 0.0385, 0.0350, 0.0164 ) );
const vec3 kWwBostonK5[3] = vec3[3]( vec3( 0.589, 0.680, 1.737 ), vec3( 0.880, 1.417, 3.790 ), vec3( 0.650, 0.850, 2.233 ) );
const vec3 kWwBostonR5[3] = vec3[3]( vec3( 0.0388, 0.0400, 0.0194 ), vec3( 0.0543, 0.0400, 0.0184 ),
	vec3( 0.0385, 0.0350, 0.0164 ) );
// wet sand (sea; wet sand reads about half its dry albedo) and darker river / lake silt
const vec3 kWwBostonBed[3] = vec3[3]( vec3( 0.20, 0.18, 0.14 ), vec3( 0.10, 0.085, 0.06 ), vec3( 0.12, 0.10, 0.075 ) );

struct WwClearLight
{
	int cls;	// 0 sea, 1 river, 2 lake
	vec3 sun;	// the sun's colour (linear), 0 when below the horizon
	float sunCos;	// the sun's height over the surface (cos from the zenith, in air)
	vec3 sky;	// the sky's diffuse light
	float viewMu;	// |V.z| of the view ray (used where the path is not known)
};
WwClearLight wwClearIn = WwClearLight( 2, vec3( 0.0 ), 0.0, vec3( 0.0 ), 1.0 );
vec3 wwClearDeep = vec3( 0.0 );	// FIX1031B: the cell's WATR deep colour (linear); 0 = none: the physics' own hue

void wwWaterClearSetup( vec3 P, vec3 V, vec3 sun, vec3 Ldir, vec3 skyLo, vec3 skyHi )
{
	float c = wwWaterInfo( P.xy ).w;
	wwClearIn.cls = c < -0.5 ? 2 : clamp( int( c + 0.5 ), 0, 2 );
	wwClearIn.sunCos = clamp( Ldir.z, 0.0, 1.0 );
	wwClearIn.sun = sun;
	wwClearIn.sky = 0.5 * ( skyLo + skyHi );
	wwClearIn.viewMu = max( abs( V.z ), 1e-3 );
}

// FIX1031B: the deep reflectance, the physics' luminance in the hue of the record's deep colour
vec3 wwClearDeepR( int cls )
{
	const vec3 lum = vec3( 0.2126, 0.7152, 0.0722 );
	float y = dot( wwClearDeep, lum );
	vec3 r = waterClearRed == 5 ? kWwBostonR5[cls] : kWwBostonR[cls];
	return y > 1e-6 ? wwClearDeep * ( dot( r, lum ) / y ) : r;
}

vec3 wwClearK( int cls, out vec3 bb )
{
	vec3 k;
	if ( waterClearRed == 4 ) {
		bb = kWwClearBw + kWwClearBb[cls];
		k = kWwPureWater + kWwClearA[cls] + bb;
	} else {
		k = waterClearRed == 5 ? kWwBostonK5[cls] : kWwBostonK[cls];
		bb = wwClearDeepR( cls ) * k / 0.33;	// Gordon R = 0.33 bb / k, solved for bb
	}
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
	vec3 alb = waterClearRed == 4 ? kWwClearBed[wwClearIn.cls] : kWwBostonBed[wwClearIn.cls];
	return alb * ( wwClearIn.sun * wwClearIn.sunCos + wwClearIn.sky );
}
