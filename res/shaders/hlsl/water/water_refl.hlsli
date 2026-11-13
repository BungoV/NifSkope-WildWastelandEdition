// HLSL source of features/water/water_refl.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	// Roughness: LEAN mapping (Olano and Baker, I3D 2010): each noise layer has a second-moment texture (t.x^2, t.y^2 of
	// the decoded slope, mipmapped and sampled like the layer), so the slope variance the filter averaged away comes back
	// as roughness; the layers the game fades out past 8192 u keep their whole variance. Plus the pixel's own normal
	// footprint (Tokuyoshi and Kaplanyan 2019, "Improved Geometric Specular Antialiasing"). Fresnel: F0 0.0204 (water,
	// IOR 1.333) with Frostbite's rough Schlick (Lagarde and de Rousiers 2014). Sun: GGX with height-correlated Smith.
	bool waterReflOn;		// the Water reflections row
	bool waterGlintOn;		// the Water sun glare row
	int waterReflRed;		// 32 grazing (FARWATER1: the clear gradient at the horizon); 1 record (the WATR's F0 and reflectivity cap), 2 farcolour (the 8192 u colour swap),
	// 4 sharp (no roughness in the reflection), 8 noclouds (the plain sky gradient),
	// 16 cloudedge (the cloud map's hard square edge; lane SKYSEA1)
	int waterGlintRed;		// 1 game (today's sun + fixed-direction sparkle), 2 nolean (no slope variance), 4 noshoulder
	float4 waterGlintK;		// the glare's highlight shoulder (x the sun colour), F0, footprint clamp, sharp-SSR alpha
	float4 waterCloudGeo;		// the map's centre x, y, half extent (game units), valid
	float4 waterCloudZ;		// the low, mid, high bands' base z
	float4 waterCloudCov;		// the bands' coverage outside the map; w unused
	float3 waterCloudTint;
	// lane FIX1101A (water SSR on by default): the ray pass off the flat surface + the bent read (water_ssrbend.glsl)
	bool waterSsrFlat;
#else
// lane WATERREFL1 (src/features/water/cellwater.h): physical water reflections and sun glare (fo4_waterlook.prog only).

Texture2D<float4> waterMoment0 : register(t17, space0); SamplerState _waterMoment0_sampler : register(s17, space0);	// per noise layer: (E[t.x^2], E[t.y^2]) of the decoded slope t = 2 texel - 1
Texture2D<float4> waterMoment1 : register(t18, space0); SamplerState _waterMoment1_sampler : register(s18, space0);
Texture2D<float4> waterMoment2 : register(t19, space0); SamplerState _waterMoment2_sampler : register(s19, space0);
Texture2D<float4> waterCloudMap : register(t20, space0); SamplerState _waterCloudMap_sampler : register(s20, space0);	// lane CLOUD1's map (R low, G mid, A high band coverage); waterCloudGeo.w 0 = no map

static const float kWaterPi = 3.14159265;

float wwLayerVar( Texture2D<float4> m, SamplerState _m_sampler, float2 uv, float2 tap )
{
	float2 t = tap * 2.0 - 1.0;
	float2 e = m.Sample( _m_sampler, uv ).xy;
	return max( e.x - t.x * t.x, 0.0 ) + max( e.y - t.y * t.y, 0.0 );
}

// the whole layer's variance (its last mip: the texture's mean moment; the mean slope of a tiling noise is ~0)
float wwLayerVarAll( Texture2D<float4> m, SamplerState _m_sampler )
{
	float2 e = m.SampleLevel( _m_sampler, (float2)( 0.5 ), 20.0 ).xy;
	return e.x + e.y;
}

/* GGX alpha^2 of the water at this pixel: the record's own lobe (sun spec power, Phong to Blinn x 4, then Beckmann
 * alpha = sqrt( 2 / ( n + 2 ) )), the slope variance the filtered layers lost, the faded layers' whole variance,
 * all through the game's flatten, and the footprint term on the drawn normal N */
float wwWaterAlpha2( float2 uv0, float2 uv1, float2 uv2, float2 tap0, float2 tap1, float2 tap2, float4 Amp, float dfade,
	float flatten, float specPow, float3 N, bool lean )
{
	float a0 = 2.0 / ( 4.0 * max( specPow, 1.0 ) + 2.0 );
	if ( !lean )
		return a0;
	float v = Amp.x * Amp.x * wwLayerVar( waterMoment0, _waterMoment0_sampler, uv0, tap0 )
		+ dfade * dfade * ( Amp.y * Amp.y * wwLayerVar( waterMoment1, _waterMoment1_sampler, uv1, tap1 ) + Amp.z * Amp.z * wwLayerVar( waterMoment2, _waterMoment2_sampler, uv2, tap2 ) );
	float z = 1.0 + ( Amp.y + Amp.z ) * dfade;
	float lost = ( 1.0 - dfade * dfade ) * ( Amp.y * Amp.y * wwLayerVarAll( waterMoment1, _waterMoment1_sampler ) + Amp.z * Amp.z * wwLayerVarAll( waterMoment2, _waterMoment2_sampler ) );
	float zl = 1.0 + Amp.y + Amp.z;
	float slopeVar = ( v / ( z * z ) + lost / ( zl * zl ) ) * flattenWw * flattenWw;
	float2 s = N.xy / max( N.z, 0.05 );
	float2 dx = ddx( s ), dy = ddy( s );
	float foot = min( 0.5 * ( dot( dx, dx ) + dot( dy, dy ) ), waterGlintK.z );
	return a0 + slopeVar + foot;
}

// Fresnel with roughness (Frostbite: the rough surface's grazing reflectance stops short of 1)
float wwWaterFresnel( float NoV, float alpha, float F0 )
{
	float c = 1.0 - clamp( NoV, 0.0, 1.0 );
	float c2 = c * c;
	return F0 + ( max( 1.0 - alpha, F0 ) - F0 ) * c2 * c2 * c;
}

// the sun's glare: GGX x height-correlated Smith x Schlick, the FO4 lighting scale (Lambert = albedo x sun x NoL, so
// the BRDF carries pi), then a highlight shoulder below the tone map's clip: x / ( 1 + x / K ), never brighter
float3 wwWaterGlare( float3 Ns, float3 V, float3 L, float3 sun, float a2, float F0, bool shoulder )
{
	float3 Vo = -V;
	float NoL = dot( Ns, L ), NoV = max( dot( Ns, Vo ), 1e-3 );
	if ( NoL <= 0.0 )
		return (float3)( 0.0 );
	float3 H = normalize( L + Vo );
	float NoH = clamp( dot( Ns, H ), 0.0, 1.0 ), LoH = clamp( dot( L, H ), 0.0, 1.0 );
	float d = NoH * NoH * ( a2 - 1.0 ) + 1.0;
	float D = a2 / ( kWaterPi * d * d );
	float vis = 0.5 / ( NoL * sqrt( NoV * NoV * ( 1.0 - a2 ) + a2 ) + NoV * sqrt( NoL * NoL * ( 1.0 - a2 ) + a2 ) );
	float c = 1.0 - LoH;
	float F = F0 + ( 1.0 - F0 ) * c * c * c * c * c;
	float g = kWaterPi * D * vis * F * NoL;
	if ( shoulder )
		g = g / ( 1.0 + g / waterGlintK.x );
	return sun * g;
}

float wwWaterCloudBand( float3 P, float3 R, float z, float outside, int ch )
{
	float t = ( z - P.z ) / R.z;
	if ( t <= 0.0 )
		return 0.0;
	float2 uv = ( P.xy + t * R.xy - waterCloudGeo.xy ) / ( 2.0 * waterCloudGeo.z ) + 0.5;
	if ( or( any( (uv < (float2)( 0.0 )) ), any( (uv > (float2)( 1.0 )) ) ) )
		return outside;
	if ( ( waterReflRed & 16 ) != 0 )
		return waterCloudMap.Sample( _waterCloudMap_sampler, uv )[ch];
	// lane SKYSEA1: fade to the band's mean cover over the map's outer 15% (the hard square drew a line on the sea)
	float2 e2 = min( uv, 1.0 - uv );
	return lerp( outside, waterCloudMap.Sample( _waterCloudMap_sampler, uv )[ch], smoothstep( 0.0, 0.15, min( e2.x, e2.y ) ) );
}

/* the reflected sky with the weather's clouds: the reflected ray meets each band's base over CLOUD1's map, as the
 * volume clouds read it. The volume clouds draw after the water (over the toned frame), so the march never sees them.
 * APPROXIMATE colour: a lit cloud at 1.5 x the brightest sky colour's luminance, x the weather's tint */
float3 wwWaterCloudSky( float3 sky, float3 P, float3 R )
{
	bool graze = ( waterReflRed & 32 ) == 0;
	if ( or( or( waterCloudGeo.w < 0.5, ( waterReflRed & 8 ) != 0 ), ( R.z <= 0.0 && !graze ) ) )
		return sky;
	float covMean = 1.0 - ( 1.0 - waterCloudCov.x ) * ( 1.0 - waterCloudCov.y ) * ( 1.0 - waterCloudCov.z );
	float cov = covMean;
	if ( R.z > 0.0 ) {
		float cl = wwWaterCloudBand( P, R, waterCloudZ.x, waterCloudCov.x, 0 );
		float cm = wwWaterCloudBand( P, R, waterCloudZ.y, waterCloudCov.y, 1 );
		float ch = wwWaterCloudBand( P, R, waterCloudZ.z, waterCloudCov.z, 3 );
		cov = 1.0 - ( 1.0 - cl ) * ( 1.0 - cm ) * ( 1.0 - ch );
	}
	/* the far band crowds into the horizon line under R.z 0.08 (flat water: past 12.5 x the eye's height). Lane FARWATER1:
	 * there the cover goes to the bands' mean, the deck the drawn sky shows at the horizon; the red (WW_CELL_WATER_REFL_RED=
	 * grazing) lets it go to the clear WTHR gradient, the deep blue band on the far water under an overcast */
	float g = smoothstep( 0.0, 0.08, R.z );
	cov = graze ? lerp( covMean, cov, g ) : cov * g;
	const float3 lumW = float3( 0.2126, 0.7152, 0.0722 );
	float l = max( max( dot( waterSky[0].rgb, lumW ), dot( waterSky[1].rgb, lumW ) ), dot( waterSky[2].rgb, lumW ) );
	return lerp( sky, waterCloudTint * ( 1.5 * l ), clamp( cov, 0.0, 1.0 ) );
}

// the reflected scene: the march where it is confident (sharp on calm water, its blur as the waves roughen), else the
// sky (the fallback off screen, at the edges and behind the camera; the march's confidence fades between them)
float3 wwWaterReflEnv( float3 sky, float4 raw, float4 fin, bool haveSsr, float alpha )
{
	if ( !haveSsr )
		return sky;
	float w = ( waterReflRed & 4 ) != 0 ? 0.9 : smoothstep( waterGlintK.w, 8.0 * waterGlintK.w, alpha );
	if ( waterSsrFlat )	// lane FIX1101A: the blur fills the sharp march's holes (dither, refusals): no sky speckle on calm water
		w = max( w, clamp( fin.a - raw.a, 0.0, 1.0 ) );
	float4 s = lerp( raw, fin, w );
	return lerp( sky, max( s.rgb, (float3)( 0.0 ) ), clamp( s.a, 0.0, 1.0 ) );
}

/* lane FIX1031B (LATE ITEM 5.1): the physical Fresnel composes this water when the reflections row is on, and always
 * with the clear colour (red WW_CELL_WATER_CLEAR_RED=old: the WATR's F x reflectivity, as the clear colour shipped):
 * a bright see-through body under F x reflectivity 0.2-0.3 showed no sky even at grazing angles (bungo 10-31) */
bool wwWaterReflPhys()
{
#ifdef WW_WATER_CLEAR
	return or( waterReflOn, ( waterClearOn && waterClearRed != 4 ) );
#else
	return waterReflOn;
#endif
}

/* the surface: the body seen through by 1 - F, the reflection by F, the same law near and far (the game swaps to one
 * WATR colour past 8192 u). Reds: record = the game's F0 and its reflectivity cap; farcolour = the game's far swap */
float3 wwWaterReflMix( float3 body, float3 env, float F, float Fgame, float reflectivity, float3 far, float dfade )
{
	float3 c = ( waterReflRed & 1 ) != 0 ? lerp( body, env, Fgame * reflectivity ) : lerp( body, env, F );
	return ( waterReflRed & 2 ) != 0 ? lerp( far, c, dfade ) : c;
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/water/water_refl.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookm_ps.hlsl, fo4_waterlookp_ps.hlsl
#ifdef WW_FN_ALL
static const float kWaterPi = 3.14159265;
#endif

#if defined(WW_FN_wwLayerVar) || defined(WW_FN_ALL)
float wwLayerVar(Texture2D<float4> m, SamplerState _m_sampler, float2 uv, float2 tap)
{
    float2 t = (tap * 2.0f) - 1.0f.xx;
    float2 e = m.Sample(_m_sampler, uv).xy;
    return max(e.x - (t.x * t.x), 0.0f) + max(e.y - (t.y * t.y), 0.0f);
}
#endif

#if defined(WW_FN_wwLayerVarAll) || defined(WW_FN_ALL)
float wwLayerVarAll(Texture2D<float4> m, SamplerState _m_sampler)
{
    float2 e = m.SampleLevel(_m_sampler, 0.5f.xx, 20.0f).xy;
    return e.x + e.y;
}
#endif

#if defined(WW_FN_wwWaterAlpha2) || defined(WW_FN_ALL)
float wwWaterAlpha2(float2 uv0, float2 uv1, float2 uv2, float2 tap0, float2 tap1, float2 tap2, float4 Amp, float dfade, float flatten, float specPow, float3 N, bool lean)
{
    float a0 = 2.0f / ((4.0f * max(specPow, 1.0f)) + 2.0f);
    if (!lean)
    {
        return a0;
    }
    float2 vPart6 = uv0;
    float2 vPart5 = tap0;
    float2 vPart4 = uv1;
    float2 vPart3 = tap1;
    float2 vPart2 = uv2;
    float2 vPart = tap2;
    float v = ((Amp.x * Amp.x) * wwLayerVar(waterMoment0, _waterMoment0_sampler, vPart6, vPart5)) + ((dfade * dfade) * (((Amp.y * Amp.y) * wwLayerVar(waterMoment1, _waterMoment1_sampler, vPart4, vPart3)) + ((Amp.z * Amp.z) * wwLayerVar(waterMoment2, _waterMoment2_sampler, vPart2, vPart))));
    float z = 1.0f + ((Amp.y + Amp.z) * dfade);
    float lost = (1.0f - (dfade * dfade)) * (((Amp.y * Amp.y) * wwLayerVarAll(waterMoment1, _waterMoment1_sampler)) + ((Amp.z * Amp.z) * wwLayerVarAll(waterMoment2, _waterMoment2_sampler)));
    float zl = (1.0f + Amp.y) + Amp.z;
    float slopeVar = (((v / (z * z)) + (lost / (zl * zl))) * flatten) * flatten;
    float2 s = N.xy / max(N.z, 0.0500000007450580596923828125f).xx;
    float2 dx = ddx(s);
    float2 dy = ddy(s);
    float foot = min(0.5f * (dot(dx, dx) + dot(dy, dy)), waterGlintK.z);
    return (a0 + slopeVar) + foot;
}
#endif

#if defined(WW_FN_wwWaterFresnel) || defined(WW_FN_ALL)
float wwWaterFresnel(float NoV, float alpha, float F0)
{
    float c = 1.0f - clamp(NoV, 0.0f, 1.0f);
    float c2 = c * c;
    return F0 + ((((max(1.0f - alpha, F0) - F0) * c2) * c2) * c);
}
#endif

#if defined(WW_FN_wwWaterGlare) || defined(WW_FN_ALL)
float3 wwWaterGlare(float3 Ns, float3 V, float3 L, float3 sun, float a2, float F0, bool shoulder)
{
    float3 Vo = -V;
    float NoL = dot(Ns, L);
    float NoV = max(dot(Ns, Vo), 0.001000000047497451305389404296875f);
    if (NoL <= 0.0f)
    {
        return 0.0f.xxx;
    }
    float3 H = normalize(L + Vo);
    float NoH = clamp(dot(Ns, H), 0.0f, 1.0f);
    float LoH = clamp(dot(L, H), 0.0f, 1.0f);
    float d = ((NoH * NoH) * (a2 - 1.0f)) + 1.0f;
    float D_1 = a2 / ((3.1415927410125732421875f * d) * d);
    float vis = 0.5f / ((NoL * sqrt(((NoV * NoV) * (1.0f - a2)) + a2)) + (NoV * sqrt(((NoL * NoL) * (1.0f - a2)) + a2)));
    float c = 1.0f - LoH;
    float F = F0 + ((((((1.0f - F0) * c) * c) * c) * c) * c);
    float g = (((3.1415927410125732421875f * D_1) * vis) * F) * NoL;
    if (shoulder)
    {
        g /= (1.0f + (g / waterGlintK.x));
    }
    return sun * g;
}
#endif

#if defined(WW_FN_wwWaterCloudBand) || defined(WW_FN_ALL)
float wwWaterCloudBand(float3 P, float3 R, float z, float outside, int ch)
{
    float t = (z - P.z) / R.z;
    if (t <= 0.0f)
    {
        return 0.0f;
    }
    float2 uv = (((P.xy + (R.xy * t)) - waterCloudGeo.xy) / (2.0f * waterCloudGeo.z).xx) + 0.5f.xx;
    bool vPart6 = any(bool2(uv.x < 0.0f.xx.x, uv.y < 0.0f.xx.y));
    bool vPart5;
    if (!vPart6)
    {
        vPart5 = any(bool2(uv.x > 1.0f.xx.x, uv.y > 1.0f.xx.y));
    }
    else
    {
        vPart5 = vPart6;
    }
    if (vPart5)
    {
        return outside;
    }
    if ((waterReflRed & 16) != 0)
    {
        return waterCloudMap.Sample(_waterCloudMap_sampler, uv)[ch];
    }
    // lane SKYSEA1: fade to the band's mean cover over the map's outer 15% (the hard square drew a line on the sea)
    float2 e2 = min(uv, 1.0f.xx - uv);
    return lerp(outside, waterCloudMap.Sample(_waterCloudMap_sampler, uv)[ch], smoothstep(0.0f, 0.1500000059604644775390625f, min(e2.x, e2.y)));
}
#endif

#if defined(WW_FN_wwWaterCloudSky) || defined(WW_FN_ALL)
float3 wwWaterCloudSky(float3 sky, float3 P, float3 R)
{
    bool graze = (waterReflRed & 32) == 0;
    bool vPart6 = waterCloudGeo.w < 0.5f;
    bool vPart5;
    if (!vPart6)
    {
        vPart5 = (waterReflRed & 8) != 0;
    }
    else
    {
        vPart5 = vPart6;
    }
    bool vPart4;
    if (!vPart5)
    {
        vPart4 = (R.z <= 0.0f) && (!graze);
    }
    else
    {
        vPart4 = vPart5;
    }
    if (vPart4)
    {
        return sky;
    }
    float covMean = 1.0f - (((1.0f - waterCloudCov.x) * (1.0f - waterCloudCov.y)) * (1.0f - waterCloudCov.z));
    float cov = covMean;
    if (R.z > 0.0f)
    {
        float3 vPart3 = P;
        float3 vPart2 = R;
        float vPart = waterCloudZ.x;
        float clPart2 = waterCloudCov.x;
        int clPart = 0;
        float cl = wwWaterCloudBand(vPart3, vPart2, vPart, clPart2, clPart);
        float3 cmPart5 = P;
        float3 cmPart4 = R;
        float cmPart3 = waterCloudZ.y;
        float cmPart2 = waterCloudCov.y;
        int cmPart = 1;
        float cm = wwWaterCloudBand(cmPart5, cmPart4, cmPart3, cmPart2, cmPart);
        float3 chPart5 = P;
        float3 chPart4 = R;
        float chPart3 = waterCloudZ.z;
        float chPart2 = waterCloudCov.z;
        int chPart = 3;
        float ch = wwWaterCloudBand(chPart5, chPart4, chPart3, chPart2, chPart);
        cov = 1.0f - (((1.0f - cl) * (1.0f - cm)) * (1.0f - ch));
    }
    // the far band crowds into the horizon line under R.z 0.08 (flat water: past 12.5 x the eye's height). Lane FARWATER1:
    // there the cover goes to the bands' mean, the deck the drawn sky shows at the horizon; the red (WW_CELL_WATER_REFL_RED=
    // grazing) lets it go to the clear WTHR gradient, the deep blue band on the far water under an overcast
    float g = smoothstep(0.0f, 0.07999999821186065673828125f, R.z);
    float covPick;
    if (graze)
    {
        covPick = lerp(covMean, cov, g);
    }
    else
    {
        covPick = cov * g;
    }
    cov = covPick;
    float l = max(max(dot(waterSky[0].xyz, float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f)), dot(waterSky[1].xyz, float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f))), dot(waterSky[2].xyz, float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f)));
    return lerp(sky, waterCloudTint * (1.5f * l), clamp(cov, 0.0f, 1.0f).xxx);
}
#endif

#if defined(WW_FN_wwWaterReflEnv) || defined(WW_FN_ALL)
float3 wwWaterReflEnv(float3 sky, float4 raw, float4 fin, bool haveSsr, float alpha)
{
    if (!haveSsr)
    {
        return sky;
    }
    float vPart6;
    if ((waterReflRed & 4) != 0)
    {
        vPart6 = 0.89999997615814208984375f;
    }
    else
    {
        vPart6 = smoothstep(waterGlintK.w, 8.0f * waterGlintK.w, alpha);
    }
    float w = vPart6;
    // lane FIX1101A: the blur fills the sharp march's holes (dither, refusals): no sky speckle on calm water
    if (waterSsrFlat)
    {
        w = max(w, clamp(fin.w - raw.w, 0.0f, 1.0f));
    }
    float4 s = lerp(raw, fin, w.xxxx);
    return lerp(sky, max(s.xyz, 0.0f.xxx), clamp(s.w, 0.0f, 1.0f).xxx);
}
#endif

#if defined(WW_FN_wwWaterReflPhys) || defined(WW_FN_ALL)
bool wwWaterReflPhys()
{
    bool vPart6;
    if (!waterReflOn)
    {
        vPart6 = waterClearOn && (waterClearRed != 4);
    }
    else
    {
        vPart6 = waterReflOn;
    }
    return vPart6;
}
#endif

#if defined(WW_FN_wwWaterReflMix) || defined(WW_FN_ALL)
float3 wwWaterReflMix(float3 body, float3 env, float F, float Fgame, float reflectivity, float3 far, float dfade)
{
    float3 vPart6;
    if ((waterReflRed & 1) != 0)
    {
        vPart6 = lerp(body, env, (Fgame * reflectivity).xxx);
    }
    else
    {
        vPart6 = lerp(body, env, F.xxx);
    }
    float3 c = vPart6;
    float3 vPart5;
    if ((waterReflRed & 2) != 0)
    {
        vPart5 = lerp(far, c, dfade.xxx);
    }
    else
    {
        vPart5 = c;
    }
    return vPart5;
}
#endif
#endif	// WW_HLSL_SRC
