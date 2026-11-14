// HLSL source of features/reflections/cell_ssr.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	bool cellSsrOn;			// an opaque draw of a flagged material, with this frame's reflections bound
	bool cellSsrMat;		// the material's flag: environment-mapped AND its material file's reflections switch
	float4 cellSsrRect;		// the viewport: x, y, 1 / w, 1 / h
#else
// Screen-space reflection read by cell-lit draws (src/features/reflections/cellssr.cpp, cell_ssr.frag).
// Included after cell_lights.glsl.


Texture2D<float4> cellSsr : register(t43, space0); SamplerState _cellSsr_sampler : register(s43, space0);		// half the view, top row first, bilinear: rgb the reflected color, a the confidence

float4 cellSsrTexel()
{
	float2 uv = ( gl_FragCoord.xy - cellSsrRect.xy ) * cellSsrRect.zw;
	return cellSsr.Sample( _cellSsr_sampler, float2( uv.x, 1.0 - uv.y ) );
}

/* The game's composite: lerp(cube, reflection, min(confidence, 1)) x env scale (cell_lights.glsl cellCubeGame).
 * Probe 60, the reflections' own scene pass, carries no reflection: the game marches before its env term. */
float3 cellSsrMix( float3 cubeK, float gloss, float spec, float envScale )
{
	if ( cellProbe == 60 )
		return (float3)( 0.0 );
	if ( !cellSsrOn )
		return cubeK;
	float4 s = cellSsrTexel();
	cellReflAdd *= 1.0 - min( s.a, 1.0 );	// lane REFLPROBE1: the march's confidence takes the probes' share too
	float k = 3.0 * clamp( spec, 0.0, 1.0 ) * min( sqrt( clamp( clamp( gloss, 0.0, 1.0 ) - 0.3, 0.0, 1.0 ) ), 1.0 )
		* clamp( envScale, 0.0, 50.0 );
	return lerp( cubeK, s.rgb * k, min( s.a, 1.0 ) );
}

// probe 60: the scene pass (linear lit color without reflection, obscurance and fog; alpha the flag).
// probe 61: sqrt of c = min(confidence, 1), green x c, mean rgb x c; alpha 0 elsewhere so blended draws keep it
float4 cellSsrProbeOut( float3 sceneLinear )
{
	if ( cellProbe == 60 )
		return float4( max( sceneLinear, (float3)( 0.0 ) ), cellSsrMat ? 1.0 : 0.0 );
	if ( !cellSsrOn )
		return (float4)( 0.0 );
	float4 s = cellSsrTexel();
	float c = clamp( s.a, 0.0, 1.0 );
	return float4( sqrt( clamp( float3( c, s.g * c, dot( s.rgb, (float3)( 1.0 / 3.0 ) ) * c ), 0.0, 1.0 ) ), 1.0 );
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/reflections/cell_ssr.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_cell_ps.hlsl, fo4_cellcsm_ps.hlsl, fo4_cellcsmfgc_ps.hlsl, fo4_cellcsmfgl_ps.hlsl, fo4_cellcsmfgp_ps.hlsl, fo4_cellcsmfs_ps.hlsl, fo4_cellfgc_ps.hlsl, fo4_cellfgl_ps.hlsl, fo4_cellfgp_ps.hlsl, fo4_cellfs_ps.hlsl, pbrm_cell_ps.hlsl, pbrm_cellcsm_ps.hlsl

#if defined(WW_FN_cellSsrTexel) || defined(WW_FN_ALL)
float4 cellSsrTexel()
{
    float2 uv = (gl_FragCoord.xy - cellSsrRect.xy) * cellSsrRect.zw;
    return cellSsr.Sample(_cellSsr_sampler, float2(uv.x, 1.0f - uv.y));
}
#endif

#if defined(WW_FN_cellSsrMix) || defined(WW_FN_ALL)
float3 cellSsrMix(float3 cubeK, float gloss, float spec, float envScale)
{
    if (cellProbe == 60)
    {
        return 0.0f.xxx;
    }
    if (!cellSsrOn)
    {
        return cubeK;
    }
    float4 s = cellSsrTexel();
    // lane REFLPROBE1: the march's confidence takes the probes' share too
    cellReflAdd *= (1.0f - min(s.w, 1.0f));
    float k = ((3.0f * clamp(spec, 0.0f, 1.0f)) * min(sqrt(clamp(clamp(gloss, 0.0f, 1.0f) - 0.300000011920928955078125f, 0.0f, 1.0f)), 1.0f)) * clamp(envScale, 0.0f, 50.0f);
    return lerp(cubeK, s.xyz * k, min(s.w, 1.0f).xxx);
}
#endif

#if defined(WW_FN_cellSsrProbeOut) || defined(WW_FN_ALL)
float4 cellSsrProbeOut(float3 sceneLinear)
{
    if (cellProbe == 60)
    {
        return float4(max(sceneLinear, 0.0f.xxx), float(cellSsrMat));
    }
    if (!cellSsrOn)
    {
        return 0.0f.xxxx;
    }
    float4 s = cellSsrTexel();
    float c = clamp(s.w, 0.0f, 1.0f);
    return float4(sqrt(clamp(float3(c, s.y * c, dot(s.xyz, 0.3333333432674407958984375f.xxx) * c), 0.0f.xxx, 1.0f.xxx)), 1.0f);
}
#endif
#endif	// WW_HLSL_SRC
