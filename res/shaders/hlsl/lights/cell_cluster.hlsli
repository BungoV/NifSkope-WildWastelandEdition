// HLSL source of features/lights/cell_cluster.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	bool cellClusterOn;             // false: every pixel walks every light (the flat loop)
	float4 cellClusterDims;           // tiles x, tiles y, slices, tile size in pixels (whole numbers)
	float2 cellClusterOrigin;         // the viewport's origin, in window pixels
	float4 cellClusterDepth;          // view depth = dot( xyz, world P ) + w
	float2 cellClusterLog;            // slice = floor( ( log( depth ) - x ) * y )
	int cellClusterBase;            // where the indices start (2 x the cluster count)
#else
// Clustered light lists (src/features/lights/cellcluster.h): a pixel walks only the lights its cluster lists, in the flat
// loop's order, so the sum is the same bits. Included by cell_lights.glsl. Use:
//     int n; int b = cellLightsBegin( P, n );
//     for ( int k = 0; k < n; k++ ) { int i = cellLightAt( b, k ); ... }


Buffer<int4> cellCluster : register(t45, space0);     // per cluster (offset, count), count -1 = walk every light; then the indices

int cellClusterOf( float3 P )
{
	int3 d = int3( cellClusterDims.xyz + 0.5 );
	int2 t = clamp( int2( floor( ( gl_FragCoord.xy - cellClusterOrigin ) / cellClusterDims.w ) ), (int2)( 0 ), d.xy - 1 );
	float s = floor( ( log( max( dot( cellClusterDepth.xyz, P ) + cellClusterDepth.w, 1e-6 ) ) - cellClusterLog.x ) * cellClusterLog.y );
	return ( clamp( int( s ), 0, d.z - 1 ) * d.y + t.y ) * d.x + t.x;
}

// The first index of P's list and its length; -1 with n = every light when there is no list.
int cellLightsBegin( float3 P, out int n )
{
	n = cellLightCount;
	if ( !cellClusterOn )
		return -1;
	int c = cellClusterOf( P );
	int k = cellCluster.Load( 2 * c + 1 ).r;
	if ( k < 0 )
		return -1;
	n = k;
	return cellClusterBase + cellCluster.Load( 2 * c ).r;
}

int cellLightAt( int base, int k )
{
	return base < 0 ? k : cellCluster.Load( base + k ).r;
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/lights/cell_cluster.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: cell_precip_gpu_gs.hlsl, cell_precip_look_gs.hlsl, cell_precip_ps.hlsl, fo4_cell_ps.hlsl, fo4_cellcsm_ps.hlsl, fo4_cellcsmfgc_ps.hlsl, fo4_cellcsmfgl_ps.hlsl, fo4_cellcsmfgp_ps.hlsl, fo4_cellcsmfs_ps.hlsl, fo4_cellfgc_ps.hlsl, fo4_cellfgl_ps.hlsl, fo4_cellfgp_ps.hlsl, fo4_cellfs_ps.hlsl, fo4_effectcell_ps.hlsl, fo4_effectcellfx_ps.hlsl, fo4_effectdecal_ps.hlsl, fo4_effectdecalcsm_ps.hlsl, impostor_cell_ps.hlsl, impostor_cellinst_ps.hlsl, impostor_cellinstez_ps.hlsl, particles_cell_ps.hlsl, pbrm_cell_ps.hlsl, pbrm_cellcsm_ps.hlsl

#if defined(WW_FN_cellClusterOf) || defined(WW_FN_ALL)
int cellClusterOf(float3 P)
{
    int3 d = int3(cellClusterDims.xyz + 0.5f.xxx);
    int2 t = clamp(int2(floor((gl_FragCoord.xy - cellClusterOrigin) / cellClusterDims.w.xx)), int2(0, 0), d.xy - int2(1, 1));
    float s = floor((log(max(dot(cellClusterDepth.xyz, P) + cellClusterDepth.w, 9.9999999747524270787835121154785e-07f)) - cellClusterLog.x) * cellClusterLog.y);
    return (((clamp(int(s), 0, d.z - 1) * d.y) + t.y) * d.x) + t.x;
}
#endif

#if defined(WW_FN_cellLightsBegin) || defined(WW_FN_ALL)
int cellLightsBegin(float3 P, out int n)
{
    n = cellLightCount;
    if (!cellClusterOn)
    {
        return -1;
    }
    float3 cPart = P;
    int c = cellClusterOf(cPart);
    int k = cellCluster.Load((2 * c) + 1).x;
    if (k < 0)
    {
        return -1;
    }
    n = k;
    return cellClusterBase + cellCluster.Load(2 * c).x;
}
#endif

#if defined(WW_FN_cellLightAt) || defined(WW_FN_ALL)
int cellLightAt(int base, int k)
{
    int cPart;
    if (base < 0)
    {
        cPart = k;
    }
    else
    {
        cPart = cellCluster.Load(base + k).x;
    }
    return cPart;
}
#endif
#endif	// WW_HLSL_SRC
