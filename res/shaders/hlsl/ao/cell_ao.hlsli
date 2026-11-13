// HLSL source of features/ao/cell_ao.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	bool cellAoOn;			// an opaque cell-lit draw with this frame's obscurance (never a blended one)
	float4 cellAoRect;		// the viewport: x, y, 1 / w, 1 / h
	bool cellAoZOnly;		// RESUME 15: the depth pre-pass: cellAoPassOut is the linear view depth alone (an R32F target)
	int cellAoEarly;		// lane PERF1: 1 = the obscurance pass (probe 20) returns once it has the normal; 2 = its red (full shading, marked)
#else
// Screen-space ambient obscurance read by cell-lit draws (src/gl/celllights.h, cell_ao.frag).
// Included after cell_lights.glsl.


Texture2D<float4> cellAo : register(t42, space0); SamplerState _cellAo_sampler : register(s42, space0);		// half the view, top row first, bilinear

// the game multiplies its whole deferred composite by this, before the fog
float cellAoFactor()
{
	if ( !cellAoOn )
		return 1.0;
	float2 uv = ( gl_FragCoord.xy - cellAoRect.xy ) * cellAoRect.zw;
	return cellAo.Sample( _cellAo_sampler, float2( uv.x, 1.0 - uv.y ) ).r;
}

// MODULES1 round 10: the ONE AO rule for near and far. The frame's screen obscurance x a baked obscurance (a far
// placement's vertex AO with its ground-contact ramp; 1 on every other shape), on the whole lit colour before the fog:
// the same factor for the ambient / GI, the sun or moon, the local lights, the emissive and the specular.
float cellAoShared( float baked )
{
	return cellAoFactor() * clamp( baked, 0.0, 1.0 );
}

// the obscurance's opaque pass: view normal and linear depth in game units
float4 cellAoPassOut( float3 normalView, float3 posView )
{
	float z = -posView.z * length( cellRow[0].xyz );
	if ( cellAoZOnly )
		return (float4)( z );
	return float4( normalize( normalView ), z );
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/ao/cell_ao.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_cell_ps.hlsl, fo4_cellcsm_ps.hlsl, fo4_cellcsmfgc_ps.hlsl, fo4_cellcsmfgl_ps.hlsl, fo4_cellcsmfgp_ps.hlsl, fo4_cellcsmfs_ps.hlsl, fo4_cellfgc_ps.hlsl, fo4_cellfgl_ps.hlsl, fo4_cellfgp_ps.hlsl, fo4_cellfs_ps.hlsl, fo4_effectdecal_ps.hlsl, fo4_effectdecalcsm_ps.hlsl, pbrm_cell_ps.hlsl, pbrm_cellcsm_ps.hlsl

#if defined(WW_FN_cellAoFactor) || defined(WW_FN_ALL)
float cellAoFactor()
{
    if (!cellAoOn)
    {
        return 1.0f;
    }
    float2 uv = (gl_FragCoord.xy - cellAoRect.xy) * cellAoRect.zw;
    return cellAo.Sample(_cellAo_sampler, float2(uv.x, 1.0f - uv.y)).x;
}
#endif

#if defined(WW_FN_cellAoShared) || defined(WW_FN_ALL)
float cellAoShared(float baked)
{
    return cellAoFactor() * clamp(baked, 0.0f, 1.0f);
}
#endif

#if defined(WW_FN_cellAoPassOut) || defined(WW_FN_ALL)
float4 cellAoPassOut(float3 normalView, float3 posView)
{
    float z = (-posView.z) * length(cellRow[0].xyz);
    if (cellAoZOnly)
    {
        return z.xxxx;
    }
    return float4(normalize(normalView), z);
}
#endif
#endif	// WW_HLSL_SRC
