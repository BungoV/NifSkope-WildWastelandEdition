// HLSL source of features/sky/atmo_apfog.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
// lane SKYA (ATMO2): how the physical sky's aerial perspective and the cell fog share one surface's air.
// Both describe the same lower atmosphere: the cell fog (the game's fog, or the Volumetric Air fitted to it) is the
// weather's own measure of it, and the aerial perspective is the sky's medium (the clear air + the haze rung taken
// from the same fog numbers, atmosphere.cpp hazeFromFog). Laying one over the other counts the haze twice. So the
// combined transmittance is the MIN of the two: the aerial perspective is applied first, then the fog keeps only
// the share it has not already taken, T' = min(1, Tfog / Tap), and its in-scatter scaled by (1 - T') / (1 - Tfog).
// Where the fog is thicker (near, in a fog bank) the fog leads (bungo 10-19: our fog leads); where the clear air
// is thicker (past the fog's curve, the far LOD) the sky's air leads, continuous with the sky behind it.
// Red "double" (WW_ATMO_AP_RED=double): T' = Tfog, the haze counted twice. Offline gate: tests/spells/atmo_lut_check.py.
// Pure functions: no uniforms (the gate compiles this file alone).

// the fog's residual transmittance once the aerial perspective's tAp is in
float wwApFogResidualT( float tAp, float tFog, bool doubled )
{
	if ( doubled )
		return tFog;
	return clamp( tFog / max( tAp, 1e-6 ), 0.0, 1.0 );
}

// a medium's in-scatter L (its whole transmittance tFull) scaled to its residual tRes
float3 wwApFogResidualL( float3 L, float tFull, float tRes )
{
	if ( tRes >= 1.0 )
		return (float3)( 0.0 );
	return L * clamp( ( 1.0 - tRes ) / max( 1.0 - tFull, 1e-6 ), 0.0, 1.0 );
}

// (L, T) of the surface's whole air: the aerial perspective (ap) then the fog's residual (fog = its L, T)
float4 wwApFogCombine( float4 ap, float4 fog, bool doubled )
{
	float tr = wwApFogResidualT( ap.a, fog.a, doubled );
	float3 lr = doubled ? fog.rgb : wwApFogResidualL( fog.rgb, fog.a, tr );
	return float4( ap.rgb * tr + lr, ap.a * tr );
}
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/sky/atmo_apfog.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: cell_ddgiprobe_ps.hlsl, cell_fardots_ps.hlsl, cell_precip_gpu_gs.hlsl, cell_precip_look_gs.hlsl, cell_precip_ps.hlsl, cell_volsky_ps.hlsl, fo4_cell_ps.hlsl, fo4_cellcsm_ps.hlsl, fo4_cellcsmfgc_ps.hlsl, fo4_cellcsmfgl_ps.hlsl, fo4_cellcsmfgp_ps.hlsl, fo4_cellcsmfs_ps.hlsl, fo4_cellfgc_ps.hlsl, fo4_cellfgl_ps.hlsl, fo4_cellfgp_ps.hlsl, fo4_cellfs_ps.hlsl, fo4_effectcell_ps.hlsl, fo4_effectcellfx_ps.hlsl, fo4_effectdecal_ps.hlsl, fo4_effectdecalcsm_ps.hlsl, fo4_fog_ps.hlsl, fo4_fogcsm_ps.hlsl, fo4_water_ps.hlsl, fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookm_ps.hlsl, fo4_waterlookp_ps.hlsl, impostor_cell_ps.hlsl, impostor_cellinst_ps.hlsl, impostor_cellinstez_ps.hlsl, lookdev_ground_cell_ps.hlsl, lookdev_ground_csm_ps.hlsl, lookdev_ground_ps.hlsl, particles_cell_ps.hlsl, pbrm_cell_ps.hlsl, pbrm_cellcsm_ps.hlsl, pbrm_csm_ps.hlsl, pbrm_default_ps.hlsl
// Not here (declarations, or no converted entry carried them): wwApFogCombine

#if defined(WW_FN_wwApFogResidualT) || defined(WW_FN_ALL)
float wwApFogResidualT(float tAp, float tFog, bool doubled)
{
    if (doubled)
    {
        return tFog;
    }
    return clamp(tFog / max(tAp, 9.9999999747524270787835121154785e-07f), 0.0f, 1.0f);
}
#endif

#if defined(WW_FN_wwApFogResidualL) || defined(WW_FN_ALL)
float3 wwApFogResidualL(float3 L, float tFull, float tRes)
{
    if (tRes >= 1.0f)
    {
        return 0.0f.xxx;
    }
    return L * clamp((1.0f - tRes) / max(1.0f - tFull, 9.9999999747524270787835121154785e-07f), 0.0f, 1.0f);
}
#endif
#endif	// WW_HLSL_SRC
