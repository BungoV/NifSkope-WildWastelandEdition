// HLSL source of cell_grass.glsl (include-only GLSL), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	// x = fGrassStartFadeDistance, y = fGrassFadeRange (view units), z = 1 on a grass shape (else the factor is 1.0).
	float3 wwGrassFade;
#else
// The game's grass fade on the cell view's grass shapes (src/gl/cellgrass.h).


float wwGrassFadeK( float3 v )
{
	if ( wwGrassFade.z < 0.5 )
		return 1.0;
	return 1.0 - clamp( ( length( v ) - wwGrassFade.x ) / max( wwGrassFade.y, 1e-3 ), 0.0, 1.0 );
}

// the game's grass carries no vertex alpha into its pixel (o6.w is the fade): on a grass shape C.a is the wind's
// weight (fo4_default.vert), so it reads 1; every other shape keeps its own
float wwGrassVertexA( float vertexA )
{
	return wwGrassFade.z < 0.5 ? vertexA : 1.0;
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/cell_grass.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_cell_ps.hlsl, fo4_cellcsm_ps.hlsl, fo4_cellcsmfgc_ps.hlsl, fo4_cellcsmfgl_ps.hlsl, fo4_cellcsmfgp_ps.hlsl, fo4_cellcsmfs_ps.hlsl, fo4_cellfgc_ps.hlsl, fo4_cellfgl_ps.hlsl, fo4_cellfgp_ps.hlsl, fo4_cellfs_ps.hlsl, pbrm_cell_ps.hlsl, pbrm_cellcsm_ps.hlsl

#if defined(WW_FN_wwGrassFadeK) || defined(WW_FN_ALL)
float wwGrassFadeK(float3 v)
{
    if (wwGrassFade.z < 0.5f)
    {
        return 1.0f;
    }
    return 1.0f - clamp((length(v) - wwGrassFade.x) / max(wwGrassFade.y, 0.001000000047497451305389404296875f), 0.0f, 1.0f);
}
#endif

#if defined(WW_FN_wwGrassVertexA) || defined(WW_FN_ALL)
float wwGrassVertexA(float vertexA)
{
    return (wwGrassFade.z < 0.5f) ? vertexA : 1.0f;
}
#endif
#endif	// WW_HLSL_SRC
