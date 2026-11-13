// HLSL source of features/water/water_clear.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	bool waterClearOn;	// the Water clear colour row
	int waterClearRed;	// 1 noscatter, 2 nodown, 3 fast, 4 old
#else
/* lane WATERLOOK1 step 8 (WATERCOLOR1, src/features/water/cellwater.h): the water's own colour, in place of the WATR's depth ramp.
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



static const float3 kWwClearBw = float3( 0.0004, 0.00095, 0.00225 );	// half of pure seawater's scattering (Morel 1974), 1/m
// red old (WATERCOLOR1), per class 0 sea, 1 river, 2 lake: the extra absorption, the particles' backscatter, the bed
static const float3 kWwClearA[3] = { float3( 0.002, 0.004, 0.012 ), float3( 0.06, 0.20, 0.70 ), float3( 0.02, 0.08, 0.28 ) };
static const float3 kWwClearBb[3] = { (float3)( 0.0006 ), float3( 0.020, 0.018, 0.016 ), (float3)( 0.004 ) };
static const float3 kWwClearBed[3] = { float3( 0.55, 0.48, 0.36 ), float3( 0.20, 0.18, 0.13 ), float3( 0.22, 0.20, 0.15 ) };
// FIX1031B: Boston (notes/fix1031b/water/tools/boston_coef.py 0.011 real): k = a_w + a + bb (1/m), the physics' deep R.
// MEASURED values (notes/fix1031b/water/REALVALUES.md): sea k(550) = the harbour's measured Kd(PAR) 0.631 /m (MWRA
// 2018-06: mean 1 % light depth 7.3 m, 1996-2017; Secchi-equivalent 2.7 m, inside Luis et al. 2019's in-situ 1-5 m at
// station 142); river Secchi 1.0 m (EPA Clean Charles 2002 mean, Daly Field - BU Bridge); lake 2.0 m (no cited value).
// Rule: k(550) = Kd(PAR) where measured, else Poole-Atkins Kd = 1.7 / Secchi. Red WW_CELL_WATER_CLEAR_RED=secchi (5)
// = the overhaul's first constants (sea 2.5 m, river 1.2 m).
static const float3 kWwBostonK[3] = { float3( 0.570, 0.631, 1.599 ), float3( 0.992, 1.700, 4.580 ), float3( 0.650, 0.850, 2.233 ) };
static const float3 kWwBostonR[3] = { float3( 0.0372, 0.0400, 0.0195 ), float3( 0.0578, 0.0400, 0.0182 ),
	float3( 0.0385, 0.0350, 0.0164 ) };
static const float3 kWwBostonK5[3] = { float3( 0.589, 0.680, 1.737 ), float3( 0.880, 1.417, 3.790 ), float3( 0.650, 0.850, 2.233 ) };
static const float3 kWwBostonR5[3] = { float3( 0.0388, 0.0400, 0.0194 ), float3( 0.0543, 0.0400, 0.0184 ),
	float3( 0.0385, 0.0350, 0.0164 ) };
// wet sand (sea; wet sand reads about half its dry albedo) and darker river / lake silt
static const float3 kWwBostonBed[3] = { float3( 0.20, 0.18, 0.14 ), float3( 0.10, 0.085, 0.06 ), float3( 0.12, 0.10, 0.075 ) };

struct WwClearLight
{
	int cls;	// 0 sea, 1 river, 2 lake
	float3 sun;	// the sun's colour (linear), 0 when below the horizon
	float sunCos;	// the sun's height over the surface (cos from the zenith, in air)
	float3 sky;	// the sky's diffuse light
	float viewMu;	// |V.z| of the view ray (used where the path is not known)
};
static WwClearLight wwClearIn = { 2, (float3)( 0.0 ), 0.0, (float3)( 0.0 ), 1.0 };
static float3 wwClearDeep = (float3)( 0.0 );	// FIX1031B: the cell's WATR deep colour (linear); 0 = none: the physics' own hue

void wwWaterClearSetup( float3 P, float3 V, float3 sun, float3 Ldir, float3 skyLo, float3 skyHi )
{
	float c = wwWaterInfo( P.xy ).w;
	wwClearIn.cls = c < -0.5 ? 2 : clamp( int( c + 0.5 ), 0, 2 );
	wwClearIn.sunCos = clamp( Ldir.z, 0.0, 1.0 );
	wwClearIn.sun = sun;
	wwClearIn.sky = 0.5 * ( skyLo + skyHi );
	wwClearIn.viewMu = max( abs( V.z ), 1e-3 );
}

// FIX1031B: the deep reflectance, the physics' luminance in the hue of the record's deep colour
float3 wwClearDeepR( int cls )
{
	const float3 lum = float3( 0.2126, 0.7152, 0.0722 );
	float y = dot( wwClearDeep, lum );
	float3 r = waterClearRed == 5 ? kWwBostonR5[cls] : kWwBostonR[cls];
	return y > 1e-6 ? wwClearDeep * ( dot( r, lum ) / y ) : r;
}

float3 wwClearK( int cls, out float3 bb )
{
	float3 k;
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
float3 wwWaterClear( float3 c, float along, float vert )
{
	const float muD = 0.8;
	float3 bb;
	float3 k = wwClearK( wwClearIn.cls, bb );
	float s = wwClearIn.sunCos;
	float muS = sqrt( 1.0 - ( 1.0 - s * s ) / ( 1.333 * 1.333 ) );	// the sun's cosine under the surface
	float3 Es = wwClearIn.sun * s * 0.98, Ed = wwClearIn.sky * 0.94;	// through the surface (Fresnel ~2% / 6%)
	bool bed = along < 1e8 && vert < 1e8;
	float L = bed ? along * kWwUnitMetres : 1e6;
	float z = bed ? vert * kWwUnitMetres : 1e6;
	float muV = bed ? clamp( vert / max( along, 1e-3 ), 1e-3, 1.0 ) : wwClearIn.viewMu;
	float3 Ks = k / muS, Kd = k / muD;
	float3 inS = (float3)( 0.0 );
	if ( waterClearRed != 1 ) {
		float3 sigma = 0.75 * bb;
		float3 gs = k + Ks * muV, gd = k + Kd * muV;
		inS = sigma * ( Es * ( 1.0 - exp( -gs * L ) ) / gs + Ed * ( 1.0 - exp( -gd * L ) ) / gd );
	}
	if ( !bed )
		return inS;
	float3 E = Es + Ed;
	float3 Td = waterClearRed == 2 ? (float3)( 1.0 )
		: ( Es * exp( -Ks * z ) + Ed * exp( -Kd * z ) ) / max( E, (float3)( 1e-6 ) );
	return c * Td * exp( -k * L ) + inS;
}

// the class's bed albedo lit in air (the bed where the frame draws none)
float3 wwWaterClearBed()
{
	float3 alb = waterClearRed == 4 ? kWwClearBed[wwClearIn.cls] : kWwBostonBed[wwClearIn.cls];
	return alb * ( wwClearIn.sun * wwClearIn.sunCos + wwClearIn.sky );
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/water/water_clear.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookm_ps.hlsl, fo4_waterlookp_ps.hlsl
// Not here (declarations, or no converted entry carried them): wwClearDeepR, wwClearK, wwWaterClearBed

#if defined(WW_FN_wwWaterClearSetup) || defined(WW_FN_ALL)
void wwWaterClearSetup(float3 P, float3 V, float3 sun, float3 Ldir, float3 skyLo, float3 skyHi)
{
    float2 cPart = P.xy;
    float c = wwWaterInfo(cPart).w;
    int wwClearInPick;
    if (c < (-0.5f))
    {
        wwClearInPick = 2;
    }
    else
    {
        wwClearInPick = clamp(int(c + 0.5f), 0, 2);
    }
    wwClearIn.cls = wwClearInPick;
    wwClearIn.sunCos = clamp(Ldir.z, 0.0f, 1.0f);
    wwClearIn.sun = sun;
    wwClearIn.sky = (skyLo + skyHi) * 0.5f;
    wwClearIn.viewMu = max(abs(V.z), 0.001000000047497451305389404296875f);
}
#endif

#if defined(WW_FN_wwWaterClear) || defined(WW_FN_ALL)
float3 wwWaterClear(float3 c, float along, float vert)
{
    int cPart = wwClearIn.cls;
    float3 wwClearInPick;
    float3 kPart = wwClearK(cPart, wwClearInPick);
    float3 bb = wwClearInPick;
    float3 k = kPart;
    float s = wwClearIn.sunCos;
    // the sun's cosine under the surface
    float muS = sqrt(1.0f - ((1.0f - (s * s)) / 1.77688896656036376953125f));
    // through the surface (Fresnel ~2% / 6%)
    float3 Es = (wwClearIn.sun * s) * 0.980000019073486328125f;
    float3 Ed = wwClearIn.sky * 0.939999997615814208984375f;
    bool bed = (along < 100000000.0f) && (vert < 100000000.0f);
    float LPick;
    if (bed)
    {
        LPick = along * 0.01428749971091747283935546875f;
    }
    else
    {
        LPick = 1000000.0f;
    }
    float L = LPick;
    float zPick;
    if (bed)
    {
        zPick = vert * 0.01428749971091747283935546875f;
    }
    else
    {
        zPick = 1000000.0f;
    }
    float z = zPick;
    float muVPick;
    if (bed)
    {
        muVPick = clamp(vert / max(along, 0.001000000047497451305389404296875f), 0.001000000047497451305389404296875f, 1.0f);
    }
    else
    {
        muVPick = wwClearIn.viewMu;
    }
    float muV = muVPick;
    float3 Ks = k / muS.xxx;
    float3 Kd = k / 0.800000011920928955078125f.xxx;
    float3 inS = 0.0f.xxx;
    if (waterClearRed != 1)
    {
        float3 sigma = bb * 0.75f;
        float3 gs = k + (Ks * muV);
        float3 gd = k + (Kd * muV);
        inS = sigma * (((Es * (1.0f.xxx - exp((-gs) * L))) / gs) + ((Ed * (1.0f.xxx - exp((-gd) * L))) / gd));
    }
    if (!bed)
    {
        return inS;
    }
    float3 E = Es + Ed;
    float3 TdPick;
    if (waterClearRed == 2)
    {
        TdPick = 1.0f.xxx;
    }
    else
    {
        TdPick = ((Es * exp((-Ks) * z)) + (Ed * exp((-Kd) * z))) / max(E, 9.9999999747524270787835121154785e-07f.xxx);
    }
    float3 Td = TdPick;
    return ((c * Td) * exp((-k) * L)) + inS;
}
#endif
#endif	// WW_HLSL_SRC
