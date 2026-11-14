// HLSL source of features/water/water_skyvis.glsl (include-only GLSL), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	bool waterSkyVisOn;	// the Water sky visibility row
	int waterSkyVisRed;	// 1 one, 2 nosun
#else
#ifndef WW_GLSL_SIZE_Texture3D_float4_
#define WW_GLSL_SIZE_Texture3D_float4_
int3 textureSize(Texture3D<float4> t, int lod) { uint w, h, d, l; t.GetDimensions(uint(lod), w, h, d, l); return int3(w, h, d); }
#endif
/* lane WATERLOOK1 step 10 (WATERSKYVIS1, src/features/water/cellwater.h): the water's sky reflection and sun glare dimmed where the
 * sky is hidden (under a bridge, a pier, an overhang). Only fo4_waterlook.frag compiles this (WW_WATER_SKYVIS, after
 * cell_lights.glsl); the Water sky visibility row.
 * The read is SPECOCC1's (cellSpecSky): the probe sky grid's open-sky share rides cellGi after the GI's own slabs, three
 * axis slabs weighted n^2 on the direction read. The sky reflection reads it on the lobe's dominant direction (R for a
 * mirror, bending to N as the waves roughen); the sun's glare on the sun's own direction. Full strength (the water's own
 * row, not the surfaces' Sky visibility strength), faded out over the grid's padded edge; 1 where the grid has no
 * evidence, indoors, under water or with no sky grid packed (cellSpecSkyBase 0). Red WW_CELL_WATER_SKYVIS_RED: one
 * (visibility 1 everywhere), nosun (the glare not dimmed).
 * lane WATERREBASE1 (WATERSKYVISFAR2 goal 5): one factor, never two. Every "no near evidence" answer (row off, red, no
 * grid, the padded edge) falls back to farV, the far water's baked byte (1.0 without WW_WATER_SKYFAR), so the near grid
 * and the far byte never both dim the same sky or glare. */



float wwWaterSkyVisDir( float3 P, float3 N, float3 D, float farV )
{
	if ( or( or( or( or( or( !waterSkyVisOn, ( waterSkyVisRed & 1 ) != 0 ), !cellGiOn ), !cellGiSky ), cellInterior ), cellSpecSkyBase < 1.0 ) )
		return farV;
	float3 t = ( P + N * ( 0.5 * cellGiVoxel ) - cellGiOrigin ) / cellGiVoxel;	// texels: voxel i's centre is i + 0.5
	float z = clamp( t.z, 0.5, cellGiDims.z - 0.5 );	// the slabs stack on z: never filter across one
	float3 sl = float3( D.x >= 0.0 ? 0.0 : 1.0, D.y >= 0.0 ? 2.0 : 3.0, D.z >= 0.0 ? 4.0 : 5.0 );
	float3 w = D * D;
	float depth = float( textureSize( cellGi, 0 ).z );
	float2 a = (float2)( 0.0 );
	for ( int k = 0; k < 3; k++ ) {
		float4 s = cellGi.Sample( _cellGi_sampler, float3( t.xy / cellGiDims.xy, ( ( cellSpecSkyBase + sl[k] ) * cellGiDims.z + z ) / depth ) );
		a += w[k] * s.ra;
	}
	if ( a.y < 0.01 )
		return farV;
	return lerp( farV, clamp( a.x / a.y, 0.0, 1.0 ), cellGiEdgeFade( t.xy ) );
}

// the sky reflection's share of open sky: the lobe's dominant direction, R bending to N with the roughness (alpha)
float wwWaterSkyVis( float3 P, float3 N, float3 R, float rough, float farV )
{
	float r = clamp( rough, 0.0, 1.0 );
	return wwWaterSkyVisDir( P, N, normalize( lerp( N, R, ( 1.0 - r ) * ( sqrt( 1.0 - r ) + r ) ) ), farV );
}

// the sun's glare: the sky grid on the sun's direction (red nosun: the far byte alone)
float wwWaterSkyVisSun( float3 P, float3 N, float3 L, float farV )
{
	return ( waterSkyVisRed & 2 ) != 0 ? farV : wwWaterSkyVisDir( P, N, L, farV );
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/water/water_skyvis.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookp_ps.hlsl

#if defined(WW_FN_wwWaterSkyVisDir) || defined(WW_FN_ALL)
float wwWaterSkyVisDir(float3 P, float3 N, float3 D_1, float farV)
{
    bool cond3 = !waterSkyVisOn;
    bool condPick;
    if (!cond3)
    {
        condPick = (waterSkyVisRed & 1) != 0;
    }
    else
    {
        condPick = cond3;
    }
    bool cond2 = condPick || true;
    bool cond;
    if (!cond2)
    {
        cond = !(cellGiSky && false);
    }
    else
    {
        cond = cond2;
    }
    bool wwh0 = (cellSpecSkyBase < 1.0f);
    if ((cond || cellInterior) || wwh0)
    {
        return farV;
    }
    // texels: voxel i's centre is i + 0.5
    float3 t = ((P + (N * (0.5f * cellGiVoxel))) - cellGiOrigin) / cellGiVoxel.xxx;
    // the slabs stack on z: never filter across one
    float z = clamp(t.z, 0.5f, cellGiDims.z - 0.5f);
    float3 sl = float3((D_1.x >= 0.0f) ? 0.0f : 1.0f, (D_1.y >= 0.0f) ? 2.0f : 3.0f, (D_1.z >= 0.0f) ? 4.0f : 5.0f);
    float3 w = D_1 * D_1;
    float depth = float(int3(spvTextureSize(cellGi, uint(0), dummy_parameter)).z);
    float2 a = 0.0f.xx;
    for (int k = 0; k < 3; k++)
    {
        float4 s = cellGi.Sample(_cellGi_sampler, float3(t.xy / cellGiDims.xy, (((cellSpecSkyBase + sl[k]) * cellGiDims.z) + z) / depth));
        a += (s.xw * w[k]);
    }
    if (a.y < 0.00999999977648258209228515625f)
    {
        return farV;
    }
    float2 wwWaterSkyVisDirRet = t.xy;
    return lerp(farV, clamp(a.x / a.y, 0.0f, 1.0f), cellGiEdgeFade(wwWaterSkyVisDirRet));
}
#endif

#if defined(WW_FN_wwWaterSkyVis) || defined(WW_FN_ALL)
float wwWaterSkyVis(float3 P, float3 N, float3 R, float rough, float farV)
{
    float r = clamp(rough, 0.0f, 1.0f);
    float3 cond3 = P;
    float3 condPick = N;
    float3 cond2 = normalize(lerp(N, R, ((1.0f - r) * (sqrt(1.0f - r) + r)).xxx));
    float cond = farV;
    return wwWaterSkyVisDir(cond3, condPick, cond2, cond);
}
#endif

#if defined(WW_FN_wwWaterSkyVisSun) || defined(WW_FN_ALL)
float wwWaterSkyVisSun(float3 P, float3 N, float3 L, float farV)
{
    float cond3;
    if ((waterSkyVisRed & 2) != 0)
    {
        cond3 = farV;
    }
    else
    {
        float3 condPick = P;
        float3 cond2 = N;
        float3 cond = L;
        float wwWaterSkyVisDirRet = farV;
        cond3 = wwWaterSkyVisDir(condPick, cond2, cond, wwWaterSkyVisDirRet);
    }
    return cond3;
}
#endif
#endif	// WW_HLSL_SRC
