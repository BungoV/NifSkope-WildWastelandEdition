// HLSL source of land_swash.glsl (include-only GLSL), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	bool landSwashOn;	// the Wave run-up row
	int landSwashRed;	// 1 still (the tip held at its top), 2 nofoam, 4 regular (no groups, no along-shore noise)
	float4 landSwashK;	// sea period (s, SURF1's), waves a group (SURF1's), foam opacity, foam e-fold (m, horizontal)
	float landSwashTime;	// the world clock (s), as the water's
#else
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


// the land distance (u) and its gradient (per u, pointing inland) from the four texels wwLandShoreSmooth reads
float3 wwLandShoreGrad( float2 worldXY )
{
	float2 p = ( worldXY - landShoreGrid.xy ) / max( landShoreGrid.z, 1.0 ) - 0.5;
	int2 t = int2( floor( p ) );
	float2 f = p - float2( t );
	float a = float( wwLandShoreTexel( t ) ), b = float( wwLandShoreTexel( t + int2( 1, 0 ) ) );
	float c = float( wwLandShoreTexel( t + int2( 0, 1 ) ) ), d = float( wwLandShoreTexel( t + int2( 1, 1 ) ) );
	float q = landShoreOn ? landShoreGrid.w : 32.0;
	float2 g = float2( lerp( b - a, d - c, f.y ), lerp( c - a, d - b, f.x ) ) * q / max( landShoreGrid.z, 1.0 );
	return float3( lerp( lerp( a, b, f.x ), lerp( c, d, f.x ), f.y ) * q, g );
}

// Stockdon et al. 2006: the 2% run-up height (m) for Hs h0 (m), period T (s), beach slope b
float wwSwashR2( float h0, float T, float b )
{
	float hl = h0 * 9.81 * T * T / 6.2831853;
	return 1.1 * ( 0.35 * b * sqrt( hl ) + 0.5 * sqrt( hl * ( 0.563 * b * b + 0.004 ) ) );
}

/* (film, foam, wet, the wave's top in m) at world point P with the ground's world normal N, clock t (s). Call it in
 * uniform flow or not: no derivatives. */
float4 wwLandSwash( float3 P, float3 N, float t )
{
	if ( or( or( !landSwashOn, !landShoreOn ), !waterInfoOn ) )
		return (float4)( 0.0 );
	float3 dg = wwLandShoreGrad( P.xy );
	float d = dg.x;
	float nz = clamp( N.z, 0.05, 1.0 );
	float b = clamp( sqrt( 1.0 - nz * nz ) / nz, 0.005, 1.0 );
	float z = d * b * kShoreM;	// m above the still line
	if ( z > 4.0 )
		return (float4)( 0.0 );	// past any run-up (Hs 6 m, 12 s on a 1:1 bank: under 4 m)
	// the water: down the land distance's gradient, one water texel past the line
	float2 Pw = P.xy - dg.yz / max( length( dg.yz ), 1e-4 ) * ( d + waterInfoGrid.z );
	float4 wv = wwShoreWave( Pw );
	if ( wv.w < 0.5 )
		return (float4)( 0.0 );
	float hs = wv.x * kShoreM;
	bool sea = wwWaterInfo( Pw ).w < 0.5;
	float T = sea ? landSwashK.x : clamp( sqrt( 6.2831853 * hs / ( 0.04 * 9.81 ) ), 1.0, 12.0 );
	float om = 6.2831853 / T;
	float L0 = 9.81 * T * T / 6.2831853 / kShoreM;	// u
	bool regular = ( landSwashRed & 4 ) != 0;
	float phi = om * t + ( regular ? 0.0 : 3.0 * ( wwShoreNoise( Pw / ( 2.0 * L0 ) ) - 0.5 ) );
	float u = frac( phi / 6.2831853 );	// 0 as the bore reaches the line
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
		foam = film * exp( -max( zt - z, 0.0 ) / ( landSwashK.w * b ) ) * lerp( 1.0, 0.2, smoothstep( 0.3, 0.7, u ) )
			* wv.y * landSwashK.z * smoothstep( 0.0, 0.05, R );
	float wet = 1.0 - smoothstep( 0.7 * R2, R2, z );
	return float4( film * smoothstep( 0.0, 0.02, R ), foam, wet * smoothstep( 0.0, 0.02, R2 ), R );
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/land_swash.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_cell_ps.hlsl, fo4_cellcsm_ps.hlsl, fo4_cellcsmfgc_ps.hlsl, fo4_cellcsmfgl_ps.hlsl, fo4_cellcsmfgp_ps.hlsl, fo4_cellcsmfs_ps.hlsl, fo4_cellfgc_ps.hlsl, fo4_cellfgl_ps.hlsl, fo4_cellfgp_ps.hlsl, fo4_cellfs_ps.hlsl, pbrm_cell_ps.hlsl, pbrm_cellcsm_ps.hlsl

#if defined(WW_FN_wwLandShoreGrad) || defined(WW_FN_ALL)
float3 wwLandShoreGrad(float2 worldXY)
{
    float2 p = ((worldXY - landShoreGrid.xy) / max(landShoreGrid.z, 1.0f).xx) - 0.5f.xx;
    int2 t = int2(floor(p));
    float2 f = p - float2(t);
    int2 aPart = t;
    float a = float(wwLandShoreTexel(aPart));
    int2 bPart = t + int2(1, 0);
    float b = float(wwLandShoreTexel(bPart));
    int2 cPart = t + int2(0, 1);
    float c = float(wwLandShoreTexel(cPart));
    int2 dPart = t + int2(1, 1);
    float d = float(wwLandShoreTexel(dPart));
    float qPick;
    if (landShoreOn)
    {
        qPick = landShoreGrid.w;
    }
    else
    {
        qPick = 32.0f;
    }
    float q = qPick;
    float2 g = (float2(lerp(b - a, d - c, f.y), lerp(c - a, d - b, f.x)) * q) / max(landShoreGrid.z, 1.0f).xx;
    return float3(lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y) * q, g);
}
#endif

#if defined(WW_FN_wwSwashR2) || defined(WW_FN_ALL)
float wwSwashR2(float h0, float T, float b)
{
    float hl = (((h0 * 9.81000041961669921875f) * T) * T) / 6.283185482025146484375f;
    return 1.10000002384185791015625f * (((0.3499999940395355224609375f * b) * sqrt(hl)) + (0.5f * sqrt(hl * (((0.563000023365020751953125f * b) * b) + 0.0040000001899898052215576171875f))));
}
#endif

#if defined(WW_FN_wwLandSwash) || defined(WW_FN_ALL)
float4 wwLandSwash(float3 P, float3 N, float t)
{
    if (((!landSwashOn) || (!landShoreOn)) || (!waterInfoOn))
    {
        return 0.0f.xxxx;
    }
    float2 aPart = P.xy;
    float3 dg = wwLandShoreGrad(aPart);
    float d = dg.x;
    float nz = clamp(N.z, 0.0500000007450580596923828125f, 1.0f);
    float b = clamp(sqrt(1.0f - (nz * nz)) / nz, 0.004999999888241291046142578125f, 1.0f);
    // m above the still line
    float z = (d * b) * 0.01428749971091747283935546875f;
    if (z > 4.0f)
    {
        return 0.0f.xxxx;
    }
    // the water: down the land distance's gradient, one water texel past the line
    float2 Pw = P.xy - ((dg.yz / max(length(dg.yz), 9.9999997473787516355514526367188e-05f).xx) * (d + waterInfoGrid.z));
    float2 bPart = Pw;
    float4 wv = wwShoreWave(bPart);
    if (wv.w < 0.5f)
    {
        return 0.0f.xxxx;
    }
    float hs = wv.x * 0.01428749971091747283935546875f;
    float2 cPart = Pw;
    bool sea = wwWaterInfo(cPart).w < 0.5f;
    float dPart;
    if (sea)
    {
        dPart = landSwashK.x;
    }
    else
    {
        dPart = clamp(sqrt((6.283185482025146484375f * hs) / 0.3923999965190887451171875f), 1.0f, 12.0f);
    }
    float T = dPart;
    float om = 6.283185482025146484375f / T;
    // u
    float L0 = (((9.81000041961669921875f * T) * T) / 6.283185482025146484375f) / 0.01428749971091747283935546875f;
    bool regular = (landSwashRed & 4) != 0;
    float qPick;
    if (regular)
    {
        qPick = 0.0f;
    }
    else
    {
        float2 qPart = Pw / (2.0f * L0).xx;
        qPick = 3.0f * (wwShoreNoise(qPart) - 0.5f);
    }
    float phi = (om * t) + qPick;
    // 0 as the bore reaches the line
    float u = frac(phi / 6.283185482025146484375f);
    float env = 1.0f;
    if (!regular)
    {
        float2 psiPart = (Pw / (3.0f * L0).xx) + 0.4099999964237213134765625f.xx;
        float psi = ((om / max(landSwashK.y, 1.0f)) * (t - (u * T))) + (6.283185482025146484375f * wwShoreNoise(psiPart));
        env = 0.3499999940395355224609375f + (0.64999997615814208984375f * (0.5f + (0.5f * cos(psi))));
    }
    float R2Part3 = hs;
    float R2Part2 = T;
    float R2Part = b;
    float R2 = wwSwashR2(R2Part3, R2Part2, R2Part);
    float R = R2 * sqrt(env);
    if ((landSwashRed & 1) != 0)
    {
        u = 0.5f;
    }
    // the tip
    float zt = ((R * 4.0f) * u) * (1.0f - u);
    // 0.1 m along the ground
    float edge = max(0.100000001490116119384765625f * b, 0.001000000047497451305389404296875f);
    float film = 1.0f - smoothstep(zt - edge, zt, z);
    float foam = 0.0f;
    if ((landSwashRed & 2) == 0)
    {
        foam = ((((film * exp((-max(zt - z, 0.0f)) / (landSwashK.w * b))) * lerp(1.0f, 0.20000000298023223876953125f, smoothstep(0.300000011920928955078125f, 0.699999988079071044921875f, u))) * wv.y) * landSwashK.z) * smoothstep(0.0f, 0.0500000007450580596923828125f, R);
    }
    float wet = 1.0f - smoothstep(0.699999988079071044921875f * R2, R2, z);
    return float4(film * smoothstep(0.0f, 0.0199999995529651641845703125f, R), foam, wet * smoothstep(0.0f, 0.0199999995529651641845703125f, R2), R);
}
#endif
#endif	// WW_HLSL_SRC
