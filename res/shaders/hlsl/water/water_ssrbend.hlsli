// HLSL source of features/water/water_ssrbend.glsl (include-only GLSL), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
/* lane FIX1101A (water SSR on by default, src/features/reflections/cellssrwater.h): the water ray pass reflects the view ray off the FLAT
 * surface (no normal stack runs there: the pass cost the whole front half of this program, ~1.1 ms at 1080p), and the
 * colour pass bends the read by its own normal: the read moves by the difference of the two reflected directions'
 * vanishing points on the screen (exact for far hits, the banks and the skyline; a near hit is moved too far, so the
 * shift is capped at 8 % of the screen). Red WW_CELL_SSR_WATER_RED=1: the ray from the full normal, no bend. */
// (after water_refl.glsl and cell_lights.glsl: cellWorldDir, projectionMatrix)
float2 wwWaterSsrBend( float3 Rw, float3 Rflat )
{
	float3 ax = cellWorldDir( float3( 1.0, 0.0, 0.0 ) ), ay = cellWorldDir( float3( 0.0, 1.0, 0.0 ) ), az = cellWorldDir( float3( 0.0, 0.0, 1.0 ) );
	float3 a = float3( dot( Rw, ax ), dot( Rw, ay ), dot( Rw, az ) );
	float3 b = float3( dot( Rflat, ax ), dot( Rflat, ay ), dot( Rflat, az ) );
	float za = max( -a.z, 0.05 ), zb = max( -b.z, 0.05 );
	// the read is indexed by the WATER pixel, and a mirror flips: to meet a hit moved by (dx, dy) on the screen the read
	// moves (dx, -dy) (the calm image of a far bank mirrors about the horizon). 0.8 x and the cap: a near hit's shift is
	// smaller than its direction's (tests/features/reflections/waterssr/ssr_gate.py: error 0.09 of the stripe contrast vs 0.15 unbent)
	float2 d = 0.8 * float2( 0.5 * projectionMatrix[0][0] * ( a.x / za - b.x / zb ), 0.5 * projectionMatrix[1][1] * ( a.y / za - b.y / zb ) );
	float l = length( d );
	return l > 0.08 ? d * ( 0.08 / l ) : d;
}
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/water/water_ssrbend.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookm_ps.hlsl, fo4_waterlookp_ps.hlsl

#if defined(WW_FN_wwWaterSsrBend) || defined(WW_FN_ALL)
float2 wwWaterSsrBend(float3 Rw, float3 Rflat)
{
    float3 axPart = float3(1.0f, 0.0f, 0.0f);
    float3 ax = cellWorldDir(axPart);
    float3 ayPart = float3(0.0f, 1.0f, 0.0f);
    float3 ay = cellWorldDir(ayPart);
    float3 azPart = float3(0.0f, 0.0f, 1.0f);
    float3 az = cellWorldDir(azPart);
    float3 a = float3(dot(Rw, ax), dot(Rw, ay), dot(Rw, az));
    float3 b = float3(dot(Rflat, ax), dot(Rflat, ay), dot(Rflat, az));
    float za = max(-a.z, 0.0500000007450580596923828125f);
    float zb = max(-b.z, 0.0500000007450580596923828125f);
    // the read is indexed by the WATER pixel, and a mirror flips: to meet a hit moved by (dx, dy) on the screen the read
    // moves (dx, -dy) (the calm image of a far bank mirrors about the horizon). 0.8 x and the cap: a near hit's shift is
    // smaller than its direction's (tests/features/reflections/waterssr/ssr_gate.py: error 0.09 of the stripe contrast vs 0.15 unbent)
    float2 d = float2((0.5f * projectionMatrix[0].x) * ((a.x / za) - (b.x / zb)), (0.5f * projectionMatrix[1].y) * ((a.y / za) - (b.y / zb))) * 0.800000011920928955078125f;
    float l = length(d);
    float2 wwWaterSsrBendRet;
    if (l > 0.07999999821186065673828125f)
    {
        wwWaterSsrBendRet = d * (0.07999999821186065673828125f / l);
    }
    else
    {
        wwWaterSsrBendRet = d;
    }
    return wwWaterSsrBendRet;
}
#endif
#endif	// WW_HLSL_SRC
